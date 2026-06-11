#include "ops_internal.hpp"

#include <algorithm>
#include <set>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <ctime>

#include "../../../../third_party/nlohmann/json.hpp"

namespace ops {

namespace {

using json = nlohmann::json;

std::size_t read_count_threshold(const char* key, std::size_t fallback) {
    const auto raw = get_env_non_empty(key);
    if (!raw.has_value()) {
        return fallback;
    }
    const auto parsed = parse_int64(*raw);
    if (!parsed.has_value() || *parsed < 0) {
        return fallback;
    }
    return static_cast<std::size_t>(*parsed);
}

std::string join_reasons(const std::vector<std::string>& reasons) {
    if (reasons.empty()) {
        return "no details";
    }
    std::string out;
    for (std::size_t i = 0; i < reasons.size(); ++i) {
        if (i != 0) {
            out += " | ";
        }
        out += reasons[i];
    }
    return out;
}

std::optional<long long> parse_utc_epoch_seconds(const std::string& iso_utc) {
    const std::string value = trim(iso_utc);
    if (value.empty()) {
        return std::nullopt;
    }
    std::tm tm{};
    std::istringstream iss(value);
    iss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
    if (iss.fail()) {
        return std::nullopt;
    }
    tm.tm_isdst = 0;
    const std::time_t epoch = timegm(&tm);
    if (epoch < 0) {
        return std::nullopt;
    }
    return static_cast<long long>(epoch);
}

struct QueueRecord {
    std::string user_id;
    std::string enqueued_at_utc;
    std::string source;
};

struct QueueFileStats {
    std::size_t count{0};
    std::optional<long long> oldest_age_seconds;
    std::optional<long long> oldest_age_minutes;
    std::optional<std::string> oldest_enqueued_at_utc;
};

std::optional<QueueRecord> parse_queue_record_line(const std::string& raw_line, const std::string& default_source) {
    const std::string line = trim(raw_line);
    if (line.empty()) {
        return std::nullopt;
    }

    const std::size_t sep1 = line.find('|');
    if (sep1 == std::string::npos) {
        return std::nullopt;
    }
    const std::size_t sep2 = line.find('|', sep1 + 1);
    if (sep2 == std::string::npos) {
        return std::nullopt;
    }
    if (line.find('|', sep2 + 1) != std::string::npos) {
        return std::nullopt;
    }

    QueueRecord record;
    record.user_id = trim(line.substr(0, sep1));
    record.enqueued_at_utc = trim(line.substr(sep1 + 1, sep2 - sep1 - 1));
    record.source = trim(line.substr(sep2 + 1));
    if (record.source.empty()) {
        record.source = default_source;
    }

    if (!parse_int64(record.user_id).has_value() || record.enqueued_at_utc.empty() || record.source.empty()) {
        return std::nullopt;
    }
    if (!parse_utc_epoch_seconds(record.enqueued_at_utc).has_value()) {
        return std::nullopt;
    }
    return record;
}

QueueFileStats analyze_queue_file(const fs::path& queue_file, const std::string& default_source, long long now_epoch) {
    QueueFileStats stats{};
    std::ifstream in(queue_file);
    if (!in) {
        return stats;
    }

    std::string line;
    while (std::getline(in, line)) {
        auto record = parse_queue_record_line(line, default_source);
        if (!record.has_value()) {
            continue;
        }

        auto enqueued_epoch = parse_utc_epoch_seconds(record->enqueued_at_utc);
        if (!enqueued_epoch.has_value()) {
            continue;
        }

        stats.count += 1;
        const long long age_seconds = std::max(0LL, now_epoch - *enqueued_epoch);
        if (!stats.oldest_age_seconds.has_value() || age_seconds > *stats.oldest_age_seconds) {
            stats.oldest_age_seconds = age_seconds;
            stats.oldest_enqueued_at_utc = record->enqueued_at_utc;
        }
    }

    if (stats.oldest_age_seconds.has_value()) {
        stats.oldest_age_minutes = (*stats.oldest_age_seconds + 59LL) / 60LL;
    }
    return stats;
}

json read_json_or_empty(const fs::path& file) {
    std::ifstream in(file);
    if (!in) {
        return json::object();
    }
    try {
        json parsed;
        in >> parsed;
        return parsed;
    } catch (...) {
        return json::object();
    }
}

std::optional<long long> extract_api42_http_code(const std::string& status) {
    std::istringstream iss(trim(status));
    std::string mode;
    iss >> mode;
    if (mode != "authenticated" && mode != "unauthenticated") {
        return std::nullopt;
    }

    std::string code_token;
    iss >> code_token;
    return parse_int64(code_token);
}

void write_system_health_snapshot(const RuntimePaths& paths, const json& payload) {
    const fs::path snapshot_file = paths.logs_state_dir / "health_latest.json";
    ensure_directory(snapshot_file.parent_path());
    std::ofstream out(snapshot_file, std::ios::out | std::ios::trunc);
    if (!out) {
        return;
    }
    out << payload.dump(2) << "\n";
}

}  // namespace

int cmd_system_health(const RuntimePaths& paths) {
    json snapshot = json::object();
    const std::string now_utc = now_utc_iso8601();
    snapshot["timestamp_utc"] = now_utc;
    const long long now_epoch = static_cast<long long>(std::time(nullptr));

    print_section("Timestamp");
    std::cout << "UTC now: " << now_utc << "\n";

    bool docker_available = false;
    const std::vector<ServiceHealth> services = collect_service_health(&docker_available);
    snapshot["docker_available"] = docker_available;

    print_section("Microservices health");
    int running_services = 0;
    int healthy_services = 0;
    json microservices = json::array();
    for (const auto& svc : services) {
        if (svc.running) {
            running_services += 1;
        }
        if (svc.health == "healthy") {
            healthy_services += 1;
        }
        std::cout << "- " << svc.key
                  << " (" << svc.container_name << ")"
                  << " :: " << (svc.running ? "RUNNING" : "DOWN")
                  << " :: health=" << svc.health
                  << " :: " << svc.status
                  << " :: " << svc.ports << "\n";
        microservices.push_back({
            {"key", svc.key},
            {"container", svc.container_name},
            {"running", svc.running},
            {"health", svc.health},
            {"status", svc.status},
            {"ports", svc.ports},
        });
    }
    snapshot["microservices"] = microservices;
    snapshot["microservices_summary"] = {
        {"running", running_services},
        {"healthy", healthy_services},
        {"total", static_cast<int>(services.size())},
    };

    print_section("Disk usage");
    std::error_code ec;
    const fs::space_info disk = fs::space("/", ec);
    double used_pct = -1.0;
    long long free_mb = -1;
    if (!ec && disk.capacity > 0) {
        used_pct =
            (static_cast<double>(disk.capacity - disk.available) * 100.0) / static_cast<double>(disk.capacity);
        free_mb = static_cast<long long>(disk.available / (1024ull * 1024ull));
        std::cout << "/ used=" << std::fixed << std::setprecision(1) << used_pct
                  << "% free=" << free_mb << " MB mount=/\n";
    } else {
        std::cout << "Disk usage unavailable\n";
    }
    snapshot["disk_usage"] = {
        {"used_percent", used_pct},
        {"free_mb", free_mb},
    };

    print_section("Host usage");
    int load_rc = 0;
    const std::string load_raw = trim(run_bash(
        "awk '{print $1\" \"$2\" \"$3}' /proc/loadavg",
        &load_rc
    ));
    double loadavg_1 = 0.0;
    double loadavg_5 = 0.0;
    double loadavg_15 = 0.0;
    if (load_rc == 0 && !load_raw.empty()) {
        std::istringstream lss(load_raw);
        lss >> loadavg_1 >> loadavg_5 >> loadavg_15;
    }

    int mem_rc = 0;
    const std::string mem_raw = trim(run_bash(
        "awk '/^MemTotal:/{t=$2}/^MemAvailable:/{a=$2} END{print t\" \"a}' /proc/meminfo",
        &mem_rc
    ));
    long long mem_total_kb = 0;
    long long mem_available_kb = 0;
    if (mem_rc == 0 && !mem_raw.empty()) {
        std::istringstream mss(mem_raw);
        mss >> mem_total_kb >> mem_available_kb;
    }
    const long long mem_used_kb = std::max(0LL, mem_total_kb - mem_available_kb);
    std::cout << "loadavg=" << loadavg_1 << "," << loadavg_5 << "," << loadavg_15
              << " mem_used_mb=" << (mem_used_kb / 1024)
              << " mem_total_mb=" << (mem_total_kb / 1024) << "\n";
    snapshot["host_usage"] = {
        {"loadavg_1", loadavg_1},
        {"loadavg_5", loadavg_5},
        {"loadavg_15", loadavg_15},
        {"mem_total_kb", mem_total_kb},
        {"mem_available_kb", mem_available_kb},
        {"mem_used_kb", mem_used_kb},
    };

    print_section("Database size");
    const std::string db_user = get_env_or_default("DB_USER", "api42");
    const std::string db_name = get_env_or_default("DB_NAME", "api42");
    int db_size_rc = 0;
    const std::string db_size_out = trim(run_bash(
        "timeout 5 docker exec transcendence_db psql -U " + shell_quote(db_user) +
            " -d " + shell_quote(db_name) +
            " -Atc " + shell_quote("SELECT pg_database_size(current_database());") +
            " 2>/dev/null",
        &db_size_rc
    ));
    if (db_size_rc == 0 && !db_size_out.empty()) {
        std::cout << "db_size_bytes=" << db_size_out << "\n";
        snapshot["db_size_bytes"] = parse_int64(db_size_out).value_or(0);
    } else {
        std::cout << "Database size unavailable\n";
        snapshot["db_size_bytes"] = nullptr;
    }

    print_section("API42 status");
    std::string api42_status = "unavailable";
    if (!command_exists("curl")) {
        std::cout << "curl not installed/accessible.\n";
        api42_status = "curl_unavailable";
    } else {
        const std::string api_base = get_env_or_default("API_BASE", "https://api.intra.42.fr");
        const std::string oauth_state = load_key_value(paths.root_dir / ".oauth_state", "ACCESS_TOKEN");

        int rc = 0;
        if (!oauth_state.empty()) {
            const std::string cmd =
                "curl -s -o /dev/null -w '%{http_code} %{time_total}' -H " +
                shell_quote("Authorization: Bearer " + oauth_state) + " " +
                shell_quote(api_base + "/v2/me");
            const std::string output = trim(run_bash(cmd, &rc));
            if (rc == 0 && !output.empty()) {
                std::cout << "Authenticated /v2/me -> code/time: " << output << "\n";
                api42_status = "authenticated " + output;
            } else {
                std::cout << "Authenticated /v2/me -> failed\n";
                api42_status = "authenticated_failed";
            }
        } else {
            const std::string cmd =
                "curl -s -o /dev/null -w '%{http_code} %{time_total}' " +
                shell_quote(api_base + "/oauth/authorize");
            const std::string output = trim(run_bash(cmd, &rc));
            if (rc == 0 && !output.empty()) {
                std::cout << "Unauthenticated /oauth/authorize -> code/time: " << output << "\n";
                api42_status = "unauthenticated " + output;
            } else {
                std::cout << "Unauthenticated /oauth/authorize -> failed\n";
                api42_status = "unauthenticated_failed";
            }
        }
    }
    snapshot["api42_status"] = api42_status;
    const auto api42_http_code = extract_api42_http_code(api42_status);
    if (api42_http_code.has_value()) {
        snapshot["api42_http_code"] = *api42_http_code;
    } else {
        snapshot["api42_http_code"] = nullptr;
    }

    print_section("Agents (pid files)");
    print_pid_status("detector", paths.logs_pids_dir / "detector.pid");
    print_pid_status("fetcher_internal", paths.logs_pids_dir / "fetcher_internal.pid");
    print_pid_status("fetcher_external", paths.logs_pids_dir / "fetcher_external.pid");
    print_pid_status("upserter_users", paths.logs_pids_dir / "upserter_users.pid");
    print_pid_status("upserter_events", paths.logs_pids_dir / "upserter_events.pid");

    print_section("Process flow overview");
    const fs::path fetch_q_int = paths.backlog_dir / "fetch_queue_internal.txt";
    const fs::path fetch_q_ext = paths.backlog_dir / "fetch_queue_external.txt";
    const fs::path process_q = paths.backlog_dir / "process_queue.txt";
    const QueueFileStats q_int_stats = analyze_queue_file(fetch_q_int, "internal", now_epoch);
    const QueueFileStats q_ext_stats = analyze_queue_file(fetch_q_ext, "external", now_epoch);
    const QueueFileStats q_proc_stats = analyze_queue_file(process_q, "process", now_epoch);
    const std::size_t q_int_count = q_int_stats.count;
    const std::size_t q_ext_count = q_ext_stats.count;
    const std::size_t q_proc_count = q_proc_stats.count;
    const std::size_t q_int_warn_max = read_count_threshold("HEALTH_WARN_FETCH_QUEUE_INTERNAL_MAX", 200);
    const std::size_t q_ext_degraded_max = read_count_threshold("HEALTH_DEGRADED_FETCH_QUEUE_EXTERNAL_MAX", 1000);
    const std::size_t q_proc_degraded_max = read_count_threshold("HEALTH_DEGRADED_PROCESS_QUEUE_MAX", 200);

    std::cout << "- fetch_queue_internal: " << q_int_count << " entries (" << fetch_q_int.string() << ")\n";
    std::cout << "- fetch_queue_external: " << q_ext_count << " entries (" << fetch_q_ext.string() << ")\n";
    std::cout << "- process_queue: " << q_proc_count << " entries (" << process_q.string() << ")\n";
    std::cout << "- oldest_sla_minutes: internal="
              << (q_int_stats.oldest_age_minutes.has_value() ? std::to_string(*q_int_stats.oldest_age_minutes) : "-")
              << " external="
              << (q_ext_stats.oldest_age_minutes.has_value() ? std::to_string(*q_ext_stats.oldest_age_minutes) : "-")
              << " process="
              << (q_proc_stats.oldest_age_minutes.has_value() ? std::to_string(*q_proc_stats.oldest_age_minutes) : "-")
              << "\n";
    std::cout << "thresholds: internal_warn>" << q_int_warn_max
              << " external_degraded>" << q_ext_degraded_max
              << " process_degraded>" << q_proc_degraded_max << "\n";
    snapshot["queues"] = {
        {"fetch_queue_internal", q_int_count},
        {"fetch_queue_external", q_ext_count},
        {"process_queue", q_proc_count},
    };
    snapshot["queue_thresholds"] = {
        {"fetch_queue_internal_warn_max", q_int_warn_max},
        {"fetch_queue_external_degraded_max", q_ext_degraded_max},
        {"process_queue_degraded_max", q_proc_degraded_max},
    };

    std::cout << "\nLast detector run:\n" << read_last_line(paths.logs_agents_dir / "detector.log") << "\n";
    std::cout << "\nLast fetcher_internal log:\n" << read_last_line(paths.logs_agents_dir / "fetcher_internal.log") << "\n";
    std::cout << "\nLast fetcher_external log:\n" << read_last_line(paths.logs_agents_dir / "fetcher_external.log") << "\n";
    std::cout << "\nLast upserter users log:\n" << read_last_line(paths.logs_agents_dir / "upserter_users.log") << "\n";
    std::cout << "\nLast upserter events log:\n" << read_last_line(paths.logs_agents_dir / "upserter_events.log") << "\n";

    print_section("Backup status");
    const json backup_meta = read_json_or_empty(paths.logs_state_dir / "backup_latest.json");
    if (backup_meta.is_object() && !backup_meta.empty()) {
        std::cout << "status=" << backup_meta.value("status", "unknown")
                  << " file=" << backup_meta.value("backup_file", "")
                  << " size_bytes=" << backup_meta.value("size_bytes", 0)
                  << " timestamp=" << backup_meta.value("timestamp_utc", "")
                  << "\n";
        snapshot["backup"] = backup_meta;
    } else {
        std::cout << "No backup metadata yet\n";
        snapshot["backup"] = {
            {"status", "unknown"},
            {"backup_file", ""},
            {"size_bytes", 0},
            {"timestamp_utc", ""},
        };
    }

    print_section("DR info");
    const std::string dr_runbook = "docs/DISASTER_RECOVERY_RUNBOOK.md";
    std::cout << "runbook=" << dr_runbook << "\n";
    std::cout << "restore_cmd=gzip -dc backups/<file>.sql.gz | docker compose -f infra/docker-compose.yml exec -T db psql -U ${DB_USER:-api42} ${DB_NAME:-api42}\n";
    snapshot["dr_info"] = {
        {"runbook", dr_runbook},
        {"restore_command", "gzip -dc backups/<file>.sql.gz | docker compose -f infra/docker-compose.yml exec -T db psql -U ${DB_USER:-api42} ${DB_NAME:-api42}"},
    };

    const std::vector<std::string> critical_service_keys = {
        "db",
        "api",
        "web",
        "detector",
        "upserter_events",
    };
    const std::vector<std::string> warning_service_keys = {
        "fetcher_internal",
        "fetcher_external_1",
        "fetcher_external_2",
        "fetcher_external_3",
        "upserter_users",
        "maintenance_scheduler",
    };
    const auto find_service = [&](const std::string& key) -> const ServiceHealth* {
        for (const auto& svc : services) {
            if (svc.key == key) {
                return &svc;
            }
        }
        return nullptr;
    };
    const auto service_is_healthy = [&](const std::string& key) {
        const ServiceHealth* svc = find_service(key);
        if (svc == nullptr) {
            return false;
        }
        return svc->running && svc->health == "healthy";
    };

    const int required_total = static_cast<int>(critical_service_keys.size());
    int required_running = 0;
    int required_healthy = 0;
    std::vector<std::string> critical_unhealthy_keys;
    std::vector<std::string> warning_unhealthy_keys;

    for (const auto& key : critical_service_keys) {
        const ServiceHealth* svc = find_service(key);
        if (svc != nullptr && svc->running) {
            required_running += 1;
        }
        if (svc != nullptr && svc->health == "healthy") {
            required_healthy += 1;
        }
        if (!service_is_healthy(key)) {
            critical_unhealthy_keys.push_back(key);
        }
    }
    for (const auto& key : warning_service_keys) {
        if (!service_is_healthy(key)) {
            warning_unhealthy_keys.push_back(key);
        }
    }
    snapshot["microservices_required_summary"] = {
        {"running", required_running},
        {"healthy", required_healthy},
        {"total", required_total},
        {"optional", json(warning_service_keys)},
    };

    std::string overall = "healthy";
    std::vector<std::string> reasons;
    const auto set_warning = [&](const std::string& reason) {
        if (overall == "healthy") {
            overall = "warning";
        }
        reasons.push_back(reason);
    };
    const auto set_degraded = [&](const std::string& reason) {
        overall = "degraded";
        reasons.push_back(reason);
    };

    if (!docker_available) {
        set_degraded("docker unavailable");
    }
    if (!critical_unhealthy_keys.empty()) {
        std::ostringstream oss;
        for (std::size_t i = 0; i < critical_unhealthy_keys.size(); ++i) {
            if (i != 0) {
                oss << ", ";
            }
            oss << critical_unhealthy_keys[i];
        }
        set_degraded("critical services unhealthy (" + oss.str() + ")");
    }
    if (!warning_unhealthy_keys.empty()) {
        std::ostringstream oss;
        for (std::size_t i = 0; i < warning_unhealthy_keys.size(); ++i) {
            if (i != 0) {
                oss << ", ";
            }
            oss << warning_unhealthy_keys[i];
        }
        set_warning("warning services unhealthy (" + oss.str() + ")");
    }
    if (api42_http_code.has_value()) {
        if (*api42_http_code == 429 || *api42_http_code >= 500) {
            set_warning("api42 transient (" + api42_status + ")");
        } else if (*api42_http_code >= 400) {
            set_degraded("api42 unavailable (" + api42_status + ")");
        }
    } else {
        set_degraded("api42 unavailable");
    }
    if (snapshot["backup"].value("status", "unknown") == "failed") {
        set_warning("backup failed");
    }
    if (q_ext_count > q_ext_degraded_max) {
        set_warning(
            "fetch_queue_external high " + std::to_string(q_ext_count) +
            " > " + std::to_string(q_ext_degraded_max)
        );
    }
    if (q_proc_count > q_proc_degraded_max) {
        set_warning(
            "process_queue high " + std::to_string(q_proc_count) +
            " > " + std::to_string(q_proc_degraded_max)
        );
    }
    if (q_int_count > q_int_warn_max) {
        set_warning(
            "fetch_queue_internal high " + std::to_string(q_int_count) +
            " > " + std::to_string(q_int_warn_max)
        );
    }

    if (reasons.empty()) {
        reasons.push_back("services healthy, api42 ok, backup ok, queues ok");
    }
    snapshot["overall_status"] = overall;
    snapshot["overall_reason"] = join_reasons(reasons);
    write_system_health_snapshot(paths, snapshot);

    return 0;
}

}  // namespace ops
