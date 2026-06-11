#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace ops {

namespace fs = std::filesystem;

struct RuntimePaths {
    fs::path root_dir;
    fs::path runtime_dir;
    fs::path logs_root;
    fs::path logs_agents_dir;
    fs::path logs_ops_dir;
    fs::path logs_state_dir;
    fs::path logs_archive_health_dir;
    fs::path logs_pids_dir;
    fs::path backlog_dir;
};

struct ServiceHealth {
    std::string key;
    std::string container_name;
    bool running{false};
    std::string status;
    std::string health;
    std::string ports;
};

std::string trim(const std::string& value);
std::optional<std::string> get_env_non_empty(const char* key);
std::string get_env_or_default(const char* key, const std::string& fallback);
std::optional<long long> parse_int64(const std::string& raw);
std::string shell_quote(const std::string& value);
std::string run_bash(const std::string& command, int* exit_code = nullptr);
bool command_exists(const std::string& command);
bool file_is_executable(const fs::path& path);
void ensure_directory(const fs::path& path);
RuntimePaths resolve_runtime_paths();
std::string now_utc_iso8601();
void append_log(const fs::path& file, const std::string& message);
std::string read_last_line(const fs::path& path);
std::size_t count_lines(const fs::path& path);
void trim_file_lines(const fs::path& path, std::size_t keep_last);
std::string load_key_value(const fs::path& file, const std::string& key);
std::optional<long long> read_pid(const fs::path& pid_file);
bool pid_is_running(long long pid);
std::string read_process_cmdline(long long pid);
void print_section(const std::string& title);
void print_pid_status(const std::string& name, const fs::path& pid_file);
fs::path resolve_token_manager_binary(const RuntimePaths& paths);
std::vector<ServiceHealth> collect_service_health(bool* docker_available = nullptr);

int cmd_system_health(const RuntimePaths& paths);
int cmd_events_diff(const RuntimePaths& paths, const std::vector<std::string>& args);
int cmd_refresh_token(const RuntimePaths& paths);
int cmd_backup(const RuntimePaths& paths);
int cmd_cleanup(const RuntimePaths& paths);
int cmd_maintenance(const RuntimePaths& paths);
void print_usage();

}  // namespace ops
