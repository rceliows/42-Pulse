#pragma once

#include <algorithm>
#include <array>
#include <cctype>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
#include <utility>
#include <vector>

#include "../../../../third_party/nlohmann/json.hpp"

namespace orchestra {

namespace fs = std::filesystem;
using json = nlohmann::json;

struct RuntimePaths {
    fs::path root_dir;
    fs::path runtime_dir;
    fs::path logs_root;
    fs::path exports_root;
    fs::path backlog_dir;
};

std::string trim(const std::string& value);
bool is_valid_env_key(const std::string& key);
std::optional<std::string> key_value_file_get_value(const fs::path& path, const std::string& key_name);
bool key_value_file_has_non_empty_value(const fs::path& path, const std::string& key_name);
std::optional<std::string> get_env_non_empty(const char* key);
std::string get_env_or_default(const char* key, const std::string& fallback);
std::string shell_quote(const std::string& value);
int run_bash(const std::string& command);
std::string run_bash_capture(const std::string& command, int* exit_code = nullptr);
bool has_jq();
std::string now_utc_iso8601();
long long now_epoch();
std::string utc_iso8601_from_epoch(long long epoch);
void ensure_directory(const fs::path& path);
bool file_is_executable(const fs::path& path);
fs::path resolve_root_dir();
RuntimePaths resolve_runtime_paths();
std::optional<long long> parse_int64(const std::string& raw);
std::optional<fs::path> resolve_binary(const RuntimePaths& paths, const char* env_key, const fs::path& system_binary);
std::string read_config_value(const fs::path& config_path, const std::string& key);
void log_line(const fs::path& log_file, const std::string& message);
int run_logged(const std::string& command, const fs::path& log_file);
bool ensure_jq_available(const fs::path& log_file);
int check_table_exists(const std::string& table);
std::optional<long long> json_to_int64(const json& value);
bool write_text_atomic(const fs::path& output_path, const std::string& content, const fs::path& log_file);
bool write_json_atomic(const fs::path& output_path, const json& payload, const fs::path& log_file);
bool read_json_file(const fs::path& input_path, json* out, const fs::path& log_file);
bool write_epoch_and_stats(
    const fs::path& export_dir,
    long long epoch,
    const std::vector<std::pair<std::string, std::string>>& stats_lines,
    const fs::path& log_file);
std::optional<json> call_api_json(
    const fs::path& api_client_bin,
    const std::string& endpoint,
    const fs::path& log_file,
    const std::string& step_label);

int cmd_check_environment(const RuntimePaths& paths);
int cmd_init_db(const RuntimePaths& paths);

int cmd_fetch_cursus_21_users_simple(const RuntimePaths& paths, const fs::path& log_file);
int fetch_cursus_metadata_native(const RuntimePaths& paths, const fs::path& log_file, const fs::path& api_client_bin);
int fetch_campuses_metadata_native(const RuntimePaths& paths, const fs::path& log_file, const fs::path& api_client_bin);
int fetch_projects_metadata_native(const RuntimePaths& paths, const fs::path& log_file, const fs::path& api_client_bin);
int fetch_coalitions_metadata_native(const RuntimePaths& paths, const fs::path& log_file, const fs::path& api_client_bin);
int fetch_campus_achievements_metadata_native(const RuntimePaths& paths, const fs::path& log_file, const fs::path& api_client_bin);
int merge_campus_achievements_native(const RuntimePaths& paths, const fs::path& log_file);
int cmd_fetch_metadata(const RuntimePaths& paths, const fs::path& log_file);
int cmd_fetch_users(const RuntimePaths& paths, const fs::path& log_file, const std::vector<std::string>& args);
int cmd_load_metadata(const RuntimePaths& paths, const fs::path& log_file);
int cmd_load_internal_users(const RuntimePaths& paths, const fs::path& log_file);

int cmd_orchestra(const RuntimePaths& paths, const std::vector<std::string>& args, const fs::path& log_file);
void print_usage();
bool has_help_flag(const std::vector<std::string>& args);

} // namespace orchestra
