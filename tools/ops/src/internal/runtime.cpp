#include "ops_internal.hpp"

#include <array>
#include <cerrno>
#include <csignal>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

namespace ops {

std::string trim(const std::string& value) {
    const auto first = value.find_first_not_of(" \t\n\r");
    if (first == std::string::npos) {
        return "";
    }
    const auto last = value.find_last_not_of(" \t\n\r");
    return value.substr(first, last - first + 1);
}

std::optional<std::string> get_env_non_empty(const char* key) {
    const char* value = std::getenv(key);
    if (!value || *value == '\0') {
        return std::nullopt;
    }
    const std::string out = trim(value);
    if (out.empty()) {
        return std::nullopt;
    }
    return out;
}

std::string get_env_or_default(const char* key, const std::string& fallback) {
    auto value = get_env_non_empty(key);
    return value.has_value() ? *value : fallback;
}

std::optional<long long> parse_int64(const std::string& raw) {
    const std::string text = trim(raw);
    if (text.empty()) {
        return std::nullopt;
    }
    errno = 0;
    char* end = nullptr;
    const long long value = std::strtoll(text.c_str(), &end, 10);
    if (errno != 0 || end == text.c_str() || *end != '\0') {
        return std::nullopt;
    }
    return value;
}

std::string shell_quote(const std::string& value) {
    std::string out = "'";
    for (char c : value) {
        if (c == '\'') {
            out += "'\\''";
        } else {
            out.push_back(c);
        }
    }
    out.push_back('\'');
    return out;
}

std::string run_bash(const std::string& command, int* exit_code) {
    const std::string wrapped = "bash -lc " + shell_quote(command);
    FILE* pipe = ::popen(wrapped.c_str(), "r");
    if (!pipe) {
        if (exit_code) {
            *exit_code = -1;
        }
        return "";
    }

    std::string output;
    std::array<char, 4096> buffer{};
    while (std::fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
        output.append(buffer.data());
    }

    const int status = ::pclose(pipe);
    if (exit_code) {
        if (WIFEXITED(status)) {
            *exit_code = WEXITSTATUS(status);
        } else {
            *exit_code = -1;
        }
    }
    return output;
}

bool command_exists(const std::string& command) {
    int rc = 0;
    (void)run_bash("command -v " + shell_quote(command) + " >/dev/null 2>&1", &rc);
    return rc == 0;
}

bool file_is_executable(const fs::path& path) {
    std::error_code ec;
    const auto perms = fs::status(path, ec).permissions();
    if (ec) {
        return false;
    }
    using p = fs::perms;
    return (perms & p::owner_exec) != p::none ||
           (perms & p::group_exec) != p::none ||
           (perms & p::others_exec) != p::none;
}

void ensure_directory(const fs::path& path) {
    if (path.empty()) {
        return;
    }
    std::error_code ec;
    if (!fs::exists(path, ec)) {
        fs::create_directories(path, ec);
    }
}

static fs::path resolve_root_dir() {
    if (auto env_root = get_env_non_empty("ROOT_DIR"); env_root.has_value()) {
        return fs::path(*env_root);
    }

    std::error_code ec;
    const fs::path cwd = fs::current_path(ec);
    if (!ec) {
        fs::path probe = cwd;
        while (true) {
            if (fs::exists(probe / "app")) {
                return probe;
            }
            const fs::path parent = probe.parent_path();
            if (parent == probe || parent.empty()) {
                break;
            }
            probe = parent;
        }
    }
    if (fs::exists("/app/app")) {
        return "/app";
    }
    return cwd;
}

RuntimePaths resolve_runtime_paths() {
    RuntimePaths paths;
    paths.root_dir = resolve_root_dir();

    paths.runtime_dir = paths.root_dir / ".." / "runtime";
    if (auto runtime_env = get_env_non_empty("RUNTIME_DIR"); runtime_env.has_value()) {
        paths.runtime_dir = fs::path(*runtime_env);
    }

    paths.logs_root = paths.runtime_dir / "logs";
    paths.logs_agents_dir = paths.logs_root / "agents";
    paths.logs_ops_dir = paths.logs_root / "ops";
    paths.logs_state_dir = paths.logs_root / "state";
    paths.logs_archive_health_dir = paths.logs_root / "archive" / "health";
    paths.logs_pids_dir = paths.logs_root / "pids";
    paths.backlog_dir = paths.runtime_dir / "backlog";

    ensure_directory(paths.logs_root);
    ensure_directory(paths.logs_agents_dir);
    ensure_directory(paths.logs_ops_dir);
    ensure_directory(paths.logs_state_dir);
    ensure_directory(paths.logs_archive_health_dir);
    ensure_directory(paths.logs_pids_dir);
    ensure_directory(paths.backlog_dir);
    return paths;
}

std::string now_utc_iso8601() {
    std::time_t now = std::time(nullptr);
    std::tm tm{};
    gmtime_r(&now, &tm);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm);
    return std::string(buf);
}

void append_log(const fs::path& file, const std::string& message) {
    ensure_directory(file.parent_path());
    std::ofstream out(file, std::ios::out | std::ios::app);
    if (out) {
        out << "[" << now_utc_iso8601() << "] " << message << "\n";
    }
}

std::string read_last_line(const fs::path& path) {
    std::ifstream in(path);
    if (!in) {
        return "(missing: " + path.string() + ")";
    }

    std::string line;
    std::string last;
    while (std::getline(in, line)) {
        if (!trim(line).empty()) {
            last = line;
        }
    }
    if (last.empty()) {
        return "(empty: " + path.string() + ")";
    }
    return last;
}

std::size_t count_lines(const fs::path& path) {
    std::ifstream in(path);
    if (!in) {
        return 0;
    }
    std::size_t count = 0;
    std::string line;
    while (std::getline(in, line)) {
        ++count;
    }
    return count;
}

void trim_file_lines(const fs::path& path, std::size_t keep_last) {
    std::ifstream in(path);
    if (!in) {
        return;
    }

    std::vector<std::string> lines;
    lines.reserve(keep_last + 128);

    std::string line;
    while (std::getline(in, line)) {
        lines.push_back(line);
    }

    if (lines.size() <= keep_last) {
        return;
    }

    std::ofstream out(path, std::ios::out | std::ios::trunc);
    if (!out) {
        return;
    }

    const std::size_t start = lines.size() - keep_last;
    for (std::size_t i = start; i < lines.size(); ++i) {
        out << lines[i] << "\n";
    }
}

std::string load_key_value(const fs::path& file, const std::string& key) {
    std::ifstream in(file);
    if (!in) {
        return "";
    }

    const std::string prefix = key + "=";
    std::string line;
    while (std::getline(in, line)) {
        std::string stripped = trim(line);
        if (stripped.empty() || stripped[0] == '#') {
            continue;
        }
        if (stripped.rfind(prefix, 0) != 0) {
            continue;
        }
        return trim(stripped.substr(prefix.size()));
    }
    return "";
}

std::optional<long long> read_pid(const fs::path& pid_file) {
    std::ifstream in(pid_file);
    if (!in) {
        return std::nullopt;
    }
    std::string raw;
    std::getline(in, raw);
    return parse_int64(raw);
}

bool pid_is_running(long long pid) {
    if (pid <= 0) {
        return false;
    }
    const int rc = ::kill(static_cast<pid_t>(pid), 0);
    if (rc == 0) {
        return true;
    }
    return errno == EPERM;
}

std::string read_process_cmdline(long long pid) {
    const fs::path cmdline_path = fs::path("/proc") / std::to_string(pid) / "cmdline";
    std::ifstream in(cmdline_path, std::ios::binary);
    if (!in) {
        return "";
    }
    std::string data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (data.empty()) {
        return "";
    }
    for (char& c : data) {
        if (c == '\0') {
            c = ' ';
        }
    }
    return trim(data);
}

void print_section(const std::string& title) {
    std::cout << "\n=== " << title << " ===\n";
}

void print_pid_status(const std::string& name, const fs::path& pid_file) {
    auto pid = read_pid(pid_file);
    if (!pid.has_value()) {
        std::cout << "- " << name << ": no pid file (" << pid_file.string() << ")\n";
        return;
    }
    if (!pid_is_running(*pid)) {
        std::cout << "- " << name << ": stale or not running (pid file present: " << pid_file.string() << ")\n";
        return;
    }
    std::string cmd = read_process_cmdline(*pid);
    if (cmd.empty()) {
        cmd = "unknown";
    }
    std::cout << "- " << name << ": RUNNING (pid=" << *pid << ") cmd=" << cmd << "\n";
}

fs::path resolve_token_manager_binary(const RuntimePaths& paths) {
    if (auto env_path = get_env_non_empty("TOKEN_MANAGER_BINARY"); env_path.has_value()) {
        const fs::path candidate(*env_path);
        if (file_is_executable(candidate)) {
            return candidate;
        }
    }

    const fs::path system_binary = "/usr/local/bin/token-manager-agent";
    if (file_is_executable(system_binary)) {
        return system_binary;
    }

    const fs::path cache_binary = paths.runtime_dir / "cache" / "bin" / "token-manager-agent";
    if (file_is_executable(cache_binary)) {
        return cache_binary;
    }

    throw std::runtime_error("token-manager-agent binary not found");
}

void print_usage() {
    std::cerr
        << "Usage: ops-agent <command> [args...]\n"
        << "Commands:\n"
        << "  system_health\n"
        << "  events_diff [limit]\n"
        << "  refresh_token\n"
        << "  backup\n"
        << "  cleanup\n"
        << "  maintenance\n";
}

}  // namespace ops
