#include <atomic>
#include <chrono>
#include <csignal>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

namespace fs = std::filesystem;

namespace {

std::atomic<bool> g_stop_requested{false};

std::string get_env_or_default(const char* key, const std::string& fallback) {
    const char* raw = std::getenv(key);
    if (!raw) {
        return fallback;
    }
    const std::string value(raw);
    if (value.empty()) {
        return fallback;
    }
    return value;
}

long long parse_int64_or_default(const std::string& raw, long long fallback) {
    try {
        std::size_t idx = 0;
        const long long value = std::stoll(raw, &idx);
        if (idx != raw.size()) {
            return fallback;
        }
        return value;
    } catch (...) {
        return fallback;
    }
}

std::string utc_now() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t now_t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#if defined(_WIN32)
    gmtime_s(&tm, &now_t);
#else
    gmtime_r(&now_t, &tm);
#endif
    std::ostringstream out;
    out << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
    return out.str();
}

std::string shell_quote(const std::string& raw) {
    std::string quoted = "'";
    for (char c : raw) {
        if (c == '\'') {
            quoted += "'\"'\"'";
        } else {
            quoted += c;
        }
    }
    quoted += "'";
    return quoted;
}

std::string json_escape(const std::string& raw) {
    std::string out;
    out.reserve(raw.size() + 16);
    for (char c : raw) {
        switch (c) {
            case '\"':
                out += "\\\"";
                break;
            case '\\':
                out += "\\\\";
                break;
            case '\b':
                out += "\\b";
                break;
            case '\f':
                out += "\\f";
                break;
            case '\n':
                out += "\\n";
                break;
            case '\r':
                out += "\\r";
                break;
            case '\t':
                out += "\\t";
                break;
            default:
                out += c;
                break;
        }
    }
    return out;
}

void append_log(const fs::path& log_file, const std::string& message) {
    std::ofstream out(log_file, std::ios::app);
    out << message << "\n";
}

int run_shell(const std::string& command) {
    const int rc = std::system(command.c_str());
    if (rc == -1) {
        return 127;
    }
    if (WIFEXITED(rc)) {
        return WEXITSTATUS(rc);
    }
    return 127;
}

void write_text_file(const fs::path& file, const std::string& text) {
    std::ofstream out(file, std::ios::trunc);
    out << text;
}

void remove_file_if_exists(const fs::path& file) {
    std::error_code ec;
    fs::remove(file, ec);
}

std::string read_file_trimmed(const fs::path& file) {
    std::ifstream in(file);
    if (!in) {
        return "";
    }
    std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    while (!text.empty() && (text.back() == '\n' || text.back() == '\r' || text.back() == ' ' || text.back() == '\t')) {
        text.pop_back();
    }
    return text;
}

struct SchedulerContext {
    fs::path repo_root;
    fs::path runtime_dir;
    fs::path ops_agent;
    fs::path compose_file;
    fs::path log_file;
    fs::path pid_file;
    fs::path state_file;
    std::string token_manager_binary;
    long long interval_s = 3600;
    long long remove_orphans = 0;
    long long warn_internal_max = 200;
    long long degraded_external_max = 1000;
    long long degraded_process_max = 200;
};

void write_state(
    const SchedulerContext& ctx,
    const std::string& status,
    const std::string& last_run_utc,
    int last_rc,
    const std::string& message
) {
    const std::string pid = read_file_trimmed(ctx.pid_file);
    std::ostringstream out;
    out << "{\n"
        << "  \"timestamp_utc\": \"" << json_escape(utc_now()) << "\",\n"
        << "  \"status\": \"" << json_escape(status) << "\",\n"
        << "  \"interval_s\": " << ctx.interval_s << ",\n"
        << "  \"remove_orphans\": " << ctx.remove_orphans << ",\n"
        << "  \"last_run_utc\": \"" << json_escape(last_run_utc) << "\",\n"
        << "  \"last_rc\": " << last_rc << ",\n"
        << "  \"message\": \"" << json_escape(message) << "\",\n"
        << "  \"pid\": \"" << json_escape(pid) << "\"\n"
        << "}\n";
    write_text_file(ctx.state_file, out.str());
}

int run_once(const SchedulerContext& ctx, const std::string& state_status) {
    const std::string started_at = utc_now();
    append_log(
        ctx.log_file,
        "[" + started_at + "] maintenance+cleanup run start (interval=" + std::to_string(ctx.interval_s) +
            "s remove_orphans=" + std::to_string(ctx.remove_orphans) + ")"
    );

    std::string common_env =
        "ROOT_DIR=" + shell_quote(ctx.repo_root.string()) +
        " RUNTIME_DIR=" + shell_quote(ctx.runtime_dir.string()) +
        " TOKEN_MANAGER_BINARY=" + shell_quote(ctx.token_manager_binary) +
        " HEALTH_WARN_FETCH_QUEUE_INTERNAL_MAX=" + std::to_string(ctx.warn_internal_max) +
        " HEALTH_DEGRADED_FETCH_QUEUE_EXTERNAL_MAX=" + std::to_string(ctx.degraded_external_max) +
        " HEALTH_DEGRADED_PROCESS_QUEUE_MAX=" + std::to_string(ctx.degraded_process_max) + " ";

    const std::string maintenance_cmd =
        common_env + shell_quote(ctx.ops_agent.string()) + " maintenance >/dev/null 2>&1";
    const int maintenance_rc = run_shell(maintenance_cmd);

    const std::string cleanup_cmd =
        "ROOT_DIR=" + shell_quote(ctx.repo_root.string()) +
        " RUNTIME_DIR=" + shell_quote(ctx.runtime_dir.string()) + " " +
        shell_quote(ctx.ops_agent.string()) + " cleanup >/dev/null 2>&1";
    const int cleanup_rc = run_shell(cleanup_cmd);

    if (ctx.remove_orphans == 1) {
        append_log(ctx.log_file, "[" + utc_now() + "] docker compose up -d --remove-orphans");
        const std::string compose_cmd =
            "docker compose -f " + shell_quote(ctx.compose_file.string()) + " up -d --remove-orphans >> " +
            shell_quote(ctx.log_file.string()) + " 2>&1";
        (void)run_shell(compose_cmd);
    }

    int rc = 0;
    if (maintenance_rc != 0) {
        rc = maintenance_rc;
    } else if (cleanup_rc != 0) {
        rc = cleanup_rc;
    }

    const std::string finished_at = utc_now();
    append_log(
        ctx.log_file,
        "[" + finished_at + "] maintenance+cleanup run done rc=" + std::to_string(rc) +
            " maintenance_rc=" + std::to_string(maintenance_rc) +
            " cleanup_rc=" + std::to_string(cleanup_rc)
    );
    write_state(ctx, state_status, finished_at, rc, "maintenance+cleanup completed");
    return rc;
}

void signal_handler(int) {
    g_stop_requested.store(true);
}

}  // namespace

int main(int argc, char** argv) {
    const std::string command = (argc >= 2 ? std::string(argv[1]) : "loop");
    if (command != "loop" && command != "run-once") {
        std::cerr << "Usage: maintenance-scheduler-agent [loop|run-once]\n";
        return 1;
    }

    SchedulerContext ctx{};
    ctx.repo_root = fs::path(get_env_or_default("REPO_ROOT", "/srv/42_Network/repo"));
    ctx.runtime_dir = fs::path(get_env_or_default("RUNTIME_DIR", (ctx.repo_root / ".." / "runtime").string()));
    ctx.ops_agent = fs::path(
        get_env_or_default("OPS_AGENT", (ctx.runtime_dir / "cache" / "bin" / "ops-agent").string())
    );
    ctx.token_manager_binary = get_env_or_default(
        "TOKEN_MANAGER_BINARY",
        (ctx.runtime_dir / "cache" / "bin" / "token-manager-agent").string()
    );
    ctx.compose_file = ctx.repo_root / "infra" / "docker-compose.yml";
    ctx.interval_s = std::max(1LL, parse_int64_or_default(get_env_or_default("AUTO_MAINTENANCE_INTERVAL_S", "3600"), 3600));
    ctx.remove_orphans = parse_int64_or_default(get_env_or_default("AUTO_MAINTENANCE_REMOVE_ORPHANS", "0"), 0);
    ctx.warn_internal_max =
        parse_int64_or_default(get_env_or_default("HEALTH_WARN_FETCH_QUEUE_INTERNAL_MAX", "200"), 200);
    ctx.degraded_external_max =
        parse_int64_or_default(get_env_or_default("HEALTH_DEGRADED_FETCH_QUEUE_EXTERNAL_MAX", "1000"), 1000);
    ctx.degraded_process_max =
        parse_int64_or_default(get_env_or_default("HEALTH_DEGRADED_PROCESS_QUEUE_MAX", "200"), 200);

    const fs::path log_root = ctx.runtime_dir / "logs";
    const fs::path log_ops_dir = log_root / "ops";
    const fs::path log_state_dir = log_root / "state";
    const fs::path log_pids_dir = log_root / "pids";

    ctx.log_file = fs::path(get_env_or_default("MAINTENANCE_SCHEDULER_LOG_FILE", (log_ops_dir / "scheduler.log").string()));
    ctx.pid_file = fs::path(
        get_env_or_default("MAINTENANCE_SCHEDULER_PID_FILE", (log_pids_dir / "maintenance_scheduler.pid").string())
    );
    ctx.state_file = fs::path(
        get_env_or_default("MAINTENANCE_SCHEDULER_STATE_FILE", (log_state_dir / "scheduler_state.json").string())
    );

    std::error_code ec;
    fs::create_directories(log_ops_dir, ec);
    fs::create_directories(log_state_dir, ec);
    fs::create_directories(log_pids_dir, ec);

    if (!fs::exists(ctx.ops_agent)) {
        std::cerr << "ops-agent not found: " << ctx.ops_agent << "\n";
        return 1;
    }

    if (command == "run-once") {
        return run_once(ctx, "stopped");
    }

    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    write_text_file(ctx.pid_file, std::to_string(getpid()) + "\n");
    write_state(ctx, "running", "", 0, "scheduler started");
    append_log(
        ctx.log_file,
        "[" + utc_now() + "] scheduler loop started pid=" + std::to_string(getpid()) +
            " interval=" + std::to_string(ctx.interval_s) + "s"
    );

    while (!g_stop_requested.load()) {
        (void)run_once(ctx, "running");
        for (long long i = 0; i < ctx.interval_s; ++i) {
            if (g_stop_requested.load()) {
                break;
            }
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }

    remove_file_if_exists(ctx.pid_file);
    write_state(ctx, "stopped", utc_now(), 0, "scheduler stopped");
    return 0;
}
