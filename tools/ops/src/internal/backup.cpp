#include "ops_internal.hpp"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <unistd.h>

namespace ops {

namespace {

struct LockFileGuard {
    int fd{-1};
    fs::path path;

    ~LockFileGuard() {
        if (fd >= 0) {
            ::close(fd);
        }
        if (!path.empty()) {
            std::error_code ec;
            fs::remove(path, ec);
        }
    }
};

std::string json_escape(const std::string& input) {
    std::string out;
    out.reserve(input.size() + 16);
    for (char c : input) {
        switch (c) {
            case '\\': out += "\\\\"; break;
            case '"': out += "\\\""; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: out.push_back(c); break;
        }
    }
    return out;
}

std::string now_utc_compact() {
    const std::time_t now = std::time(nullptr);
    std::tm tm{};
    gmtime_r(&now, &tm);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y%m%d_%H%M%S", &tm);
    return std::string(buf);
}

bool write_backup_metadata(
    const fs::path& meta_file,
    const std::string& status,
    const std::string& backup_file,
    long long size_bytes,
    long long duration_ms,
    int retention_days,
    long long pruned_count,
    const std::string& error
) {
    ensure_directory(meta_file.parent_path());
    std::ofstream out(meta_file, std::ios::out | std::ios::trunc);
    if (!out) {
        return false;
    }

    out << "{\n";
    out << "  \"timestamp_utc\": \"" << json_escape(now_utc_iso8601()) << "\",\n";
    out << "  \"status\": \"" << json_escape(status) << "\",\n";
    out << "  \"backup_file\": \"" << json_escape(backup_file) << "\",\n";
    out << "  \"size_bytes\": " << size_bytes << ",\n";
    out << "  \"duration_ms\": " << duration_ms << ",\n";
    out << "  \"retention_days\": " << retention_days << ",\n";
    out << "  \"pruned_count\": " << pruned_count << ",\n";
    out << "  \"error\": \"" << json_escape(error) << "\"\n";
    out << "}\n";
    return true;
}

}  // namespace

int cmd_backup(const RuntimePaths& paths) {
    const fs::path log_file = paths.logs_ops_dir / "backup.log";
    const fs::path meta_file = paths.logs_state_dir / "backup_latest.json";
    const fs::path lock_file = paths.logs_state_dir / "backup.lock";

    const int retention_days =
        std::max(1LL, parse_int64(get_env_or_default("BACKUP_RETENTION_DAYS", "14")).value_or(14));
    const std::string db_user = get_env_or_default("DB_USER", "api42");
    const std::string db_name = get_env_or_default("DB_NAME", "api42");
    const fs::path backup_dir = fs::path(get_env_or_default(
        "BACKUP_DIR",
        (paths.root_dir.parent_path() / "backups").string()
    ));

    append_log(log_file, "backup start");

    LockFileGuard lock;
    lock.path = lock_file;
    lock.fd = ::open(lock_file.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0644);
    if (lock.fd < 0) {
        append_log(log_file, "backup skipped: lock is already held");
        (void)write_backup_metadata(meta_file, "skipped", "", 0, 0, retention_days, 0, "backup lock already held");
        std::cout << "Backup: skipped (lock held)\n";
        return 0;
    }

    if (!command_exists("docker")) {
        append_log(log_file, "backup skipped: docker unavailable");
        (void)write_backup_metadata(meta_file, "skipped", "", 0, 0, retention_days, 0, "docker unavailable");
        std::cout << "Backup: skipped (docker unavailable)\n";
        return 0;
    }

    int docker_rc = 0;
    (void)run_bash("timeout 5 docker ps >/dev/null 2>&1", &docker_rc);
    if (docker_rc != 0) {
        append_log(log_file, "backup skipped: docker daemon unreachable");
        (void)write_backup_metadata(meta_file, "skipped", "", 0, 0, retention_days, 0, "docker daemon unreachable");
        std::cout << "Backup: skipped (docker daemon unreachable)\n";
        return 0;
    }

    int db_running_rc = 0;
    (void)run_bash(
        "timeout 5 docker ps --format '{{.Names}}' | grep -q '^transcendence_db$' >/dev/null 2>&1",
        &db_running_rc
    );
    if (db_running_rc != 0) {
        append_log(log_file, "backup skipped: transcendence_db is not running");
        (void)write_backup_metadata(meta_file, "skipped", "", 0, 0, retention_days, 0, "transcendence_db not running");
        std::cout << "Backup: skipped (db not running)\n";
        return 0;
    }

    ensure_directory(backup_dir);
    const std::string stamp = now_utc_compact();
    const fs::path backup_file = backup_dir / ("db_" + stamp + ".sql.gz");

    const fs::path compose_file = paths.root_dir / "infra" / "docker-compose.yml";
    const std::string backup_cmd =
        "set -o pipefail; docker compose -f " + shell_quote(compose_file.string()) +
        " exec -T db pg_dump -U " + shell_quote(db_user) + " " + shell_quote(db_name) +
        " | gzip > " + shell_quote(backup_file.string());

    const auto t0 = std::chrono::steady_clock::now();
    int backup_rc = 0;
    (void)run_bash(backup_cmd, &backup_rc);
    const auto t1 = std::chrono::steady_clock::now();
    const long long duration_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();

    if (backup_rc != 0) {
        std::error_code ec;
        fs::remove(backup_file, ec);
        append_log(log_file, "backup failed");
        (void)write_backup_metadata(
            meta_file,
            "failed",
            backup_file.string(),
            0,
            duration_ms,
            retention_days,
            0,
            "pg_dump command failed"
        );
        std::cout << "Backup: failed\n";
        return 1;
    }

    long long size_bytes = 0;
    {
        std::error_code ec;
        const auto file_size = fs::file_size(backup_file, ec);
        if (!ec) {
            size_bytes = static_cast<long long>(file_size);
        }
    }

    const int find_days = std::max(0, retention_days - 1);
    int prune_rc = 0;
    const std::string prune_out = trim(run_bash(
        "find " + shell_quote(backup_dir.string()) +
        " -maxdepth 1 -type f -name 'db_*.sql.gz' -mtime +" + std::to_string(find_days) +
        " -print -delete | wc -l",
        &prune_rc
    ));
    const long long pruned_count = (prune_rc == 0) ? parse_int64(prune_out).value_or(0) : 0;

    append_log(
        log_file,
        "backup success file=" + backup_file.string() +
            " size_bytes=" + std::to_string(size_bytes) +
            " duration_ms=" + std::to_string(duration_ms) +
            " pruned=" + std::to_string(pruned_count)
    );

    (void)write_backup_metadata(
        meta_file,
        "ok",
        backup_file.string(),
        size_bytes,
        duration_ms,
        retention_days,
        pruned_count,
        ""
    );

    std::cout << "Backup: ok (" << backup_file << ")\n";
    return 0;
}

}  // namespace ops
