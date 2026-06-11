#include "ops_internal.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace ops {

static std::size_t resolved_keep_lines(const char* key, const char* fallback) {
    const long long raw = parse_int64(get_env_or_default(key, fallback)).value_or(std::atoll(fallback));
    return static_cast<std::size_t>(std::max(100LL, raw));
}

static std::string compact_utc_tag(const std::string& iso_utc) {
    std::string out;
    out.reserve(iso_utc.size());
    for (const char c : iso_utc) {
        if (std::isdigit(static_cast<unsigned char>(c))) {
            out.push_back(c);
        } else if (c == 'T' || c == 'Z') {
            out.push_back(c);
        }
    }
    if (out.empty()) {
        return "unknown";
    }
    return out;
}

static fs::path next_available_path(const fs::path& preferred_path) {
    if (!fs::exists(preferred_path)) {
        return preferred_path;
    }
    const fs::path parent = preferred_path.parent_path();
    const std::string stem = preferred_path.stem().string();
    const std::string ext = preferred_path.extension().string();
    for (int i = 1; i <= 9999; ++i) {
        const fs::path candidate = parent / (stem + "_" + std::to_string(i) + ext);
        if (!fs::exists(candidate)) {
            return candidate;
        }
    }
    return preferred_path;
}

static bool archive_latest_health_snapshot(
    const RuntimePaths& paths,
    const std::string& maintenance_start_utc,
    const std::string& maintenance_end_utc,
    fs::path* archived_path_out = nullptr
) {
    const fs::path latest_snapshot = paths.logs_state_dir / "health_latest.json";
    if (!fs::exists(latest_snapshot)) {
        return false;
    }

    std::ifstream in(latest_snapshot);
    if (!in) {
        return false;
    }

    const fs::path archive_dir = paths.logs_archive_health_dir;
    ensure_directory(archive_dir);

    const std::string start_tag = compact_utc_tag(maintenance_start_utc);
    const std::string end_tag = compact_utc_tag(maintenance_end_utc);
    fs::path archive_file =
        archive_dir / ("system_health_snapshot_" + start_tag + "_to_" + end_tag + ".json");
    archive_file = next_available_path(archive_file);

    std::ofstream out(archive_file, std::ios::out | std::ios::trunc);
    if (!out) {
        return false;
    }
    out << in.rdbuf();
    if (!out) {
        return false;
    }

    if (archived_path_out != nullptr) {
        *archived_path_out = archive_file;
    }
    return true;
}

int cmd_refresh_token(const RuntimePaths& paths) {
    const fs::path log_file = paths.logs_ops_dir / "maintenance.log";
    append_log(log_file, "refresh_token start");

    try {
        const fs::path token_manager = resolve_token_manager_binary(paths);
        int rc = 0;
        (void)run_bash(shell_quote(token_manager.string()) + " refresh >/dev/null 2>&1", &rc);
        const bool ok = (rc == 0);
        append_log(log_file, std::string("token refresh: ") + (ok ? "ok" : "failed"));
        std::cout << "Token refresh: " << (ok ? "ok" : "failed") << "\n";
        return ok ? 0 : 1;
    } catch (const std::exception& ex) {
        append_log(log_file, std::string("token refresh: error: ") + ex.what());
        std::cout << "Token refresh: error (" << ex.what() << ")\n";
        return 1;
    }
}

int cmd_cleanup(const RuntimePaths& paths) {
    const fs::path log_file = paths.logs_ops_dir / "maintenance.log";
    const std::size_t cleanup_lines = resolved_keep_lines("CLEANUP_LINES", "5000");

    const std::vector<fs::path> logs = {
        paths.logs_agents_dir / "detector.log",
        paths.logs_agents_dir / "fetcher_internal.log",
        paths.logs_agents_dir / "fetcher_external.log",
        paths.logs_agents_dir / "upserter_users.log",
        paths.logs_agents_dir / "upserter_events.log",
        paths.logs_agents_dir / "token_manager.log",
        paths.logs_ops_dir / "maintenance.log",
        paths.logs_ops_dir / "scheduler.log",
        paths.logs_ops_dir / "backup.log",
    };

    append_log(log_file, "cleanup start");
    for (const auto& path : logs) {
        trim_file_lines(path, cleanup_lines);
    }
    append_log(log_file, "cleanup done (logs=" + std::to_string(cleanup_lines) + ")");
    std::cout << "Cleanup: logs trimmed\n";
    return 0;
}

int cmd_maintenance(const RuntimePaths& paths) {
    const fs::path log_file = paths.logs_ops_dir / "maintenance.log";
    const std::string maintenance_start_utc = now_utc_iso8601();
    append_log(log_file, "maintenance start");

    const int refresh_rc = cmd_refresh_token(paths);
    const int backup_rc = cmd_backup(paths);

    append_log(log_file, "maintenance complete");
    std::cout << "Maintenance: complete\n";
    std::cout << "\n";
    const int health_rc = cmd_system_health(paths);
    const std::string maintenance_end_utc = now_utc_iso8601();

    fs::path archived_snapshot_path;
    if (archive_latest_health_snapshot(paths, maintenance_start_utc, maintenance_end_utc, &archived_snapshot_path)) {
        append_log(log_file, "maintenance snapshot archived: " + archived_snapshot_path.string());
    } else {
        append_log(log_file, "maintenance snapshot archive skipped (latest snapshot unavailable)");
    }

    return (refresh_rc == 0 && backup_rc == 0 && health_rc == 0) ? 0 : 1;
}

}  // namespace ops
