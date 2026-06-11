#include <algorithm>
#include <array>
#include <cctype>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
#include <vector>

#include "../../../third_party/nlohmann/json.hpp"

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace {

class FileLock {
  public:
    explicit FileLock(const fs::path& path) {
        fd_ = ::open(path.c_str(), O_CREAT | O_RDWR, 0644);
        if (fd_ == -1) {
            throw std::runtime_error("failed to open lock file: " + path.string());
        }
        if (::flock(fd_, LOCK_EX) == -1) {
            ::close(fd_);
            fd_ = -1;
            throw std::runtime_error("failed to lock file: " + path.string());
        }
    }

    FileLock(const FileLock&) = delete;
    FileLock& operator=(const FileLock&) = delete;

    ~FileLock() {
        if (fd_ != -1) {
            ::flock(fd_, LOCK_UN);
            ::close(fd_);
        }
    }

  private:
    int fd_{-1};
};

std::string trim(const std::string& value) {
    const auto first = value.find_first_not_of(" \t\n\r");
    if (first == std::string::npos) {
        return "";
    }
    const auto last = value.find_last_not_of(" \t\n\r");
    return value.substr(first, last - first + 1);
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

void ensure_parent_directory(const fs::path& path) {
    ensure_directory(path.parent_path());
}

void touch_file(const fs::path& path) {
    ensure_parent_directory(path);
    int fd = ::open(path.c_str(), O_CREAT | O_RDWR, 0644);
    if (fd != -1) {
        ::close(fd);
    }
}

std::string now_utc_iso8601() {
    std::time_t now = std::time(nullptr);
    std::tm tm{};
    gmtime_r(&now, &tm);

    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm);
    return std::string(buf);
}

std::optional<std::string> get_env_non_empty(const char* key) {
    const char* value = std::getenv(key);
    if (!value || *value == '\0') {
        return std::nullopt;
    }
    std::string out = trim(value);
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
    std::string text = trim(raw);
    if (text.empty()) {
        return std::nullopt;
    }
    errno = 0;
    char* end = nullptr;
    long long value = std::strtoll(text.c_str(), &end, 10);
    if (errno != 0 || end == text.c_str() || *end != '\0') {
        return std::nullopt;
    }
    return value;
}

bool is_digits_only(const std::string& value) {
    if (value.empty()) {
        return false;
    }
    return std::all_of(value.begin(), value.end(), [](unsigned char c) {
        return std::isdigit(c) != 0;
    });
}

std::optional<double> parse_double(const std::string& raw) {
    std::string text = trim(raw);
    if (text.empty()) {
        return std::nullopt;
    }
    try {
        size_t idx = 0;
        double value = std::stod(text, &idx);
        if (idx != text.size()) {
            return std::nullopt;
        }
        return value;
    } catch (...) {
        return std::nullopt;
    }
}

void wait_for_fetch_slot(long long slot_ms, long long slot_index, long long slot_count) {
    if (slot_ms <= 0 || slot_count <= 1) {
        return;
    }
    const long long period_ms = slot_ms * slot_count;
    if (period_ms <= 0) {
        return;
    }

    const auto now = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now());
    const long long now_ms = now.time_since_epoch().count();
    const long long phase_ms = slot_index * slot_ms;
    const long long current_mod = now_ms % period_ms;
    const long long wait_ms = (phase_ms - current_mod + period_ms) % period_ms;
    if (wait_ms > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(wait_ms));
    }
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

std::string run_bash(const std::string& command, int* exit_code = nullptr) {
    std::string wrapped = "bash -lc " + shell_quote(command);
    FILE* pipe = ::popen(wrapped.c_str(), "r");
    if (!pipe) {
        if (exit_code) {
            *exit_code = -1;
        }
        return "";
    }

    std::string output;
    std::array<char, 8192> buffer{};
    while (std::fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
        output.append(buffer.data());
    }

    int status = ::pclose(pipe);
    if (exit_code) {
        if (WIFEXITED(status)) {
            *exit_code = WEXITSTATUS(status);
        } else {
            *exit_code = -1;
        }
    }
    return output;
}

bool is_executable_file(const fs::path& path) {
    return ::access(path.c_str(), X_OK) == 0;
}

fs::path resolve_api_client_tool(const fs::path& root_dir) {
    if (auto env_path = get_env_non_empty("API42_CLIENT_BINARY"); env_path.has_value()) {
        fs::path candidate(*env_path);
        if (is_executable_file(candidate)) {
            return candidate;
        }
    }
    if (auto env_path = get_env_non_empty("API_CLIENT_BINARY"); env_path.has_value()) {
        fs::path candidate(*env_path);
        if (is_executable_file(candidate)) {
            return candidate;
        }
    }

    const fs::path system_binary = "/usr/local/bin/api42-client-agent";
    if (is_executable_file(system_binary)) {
        return system_binary;
    }

    fs::path runtime_dir = root_dir / ".." / "runtime";
    if (auto runtime_env = get_env_non_empty("RUNTIME_DIR"); runtime_env.has_value()) {
        runtime_dir = fs::path(*runtime_env);
    }
    const fs::path runtime_cache_binary = runtime_dir / "cache" / "bin" / "api42-client-agent";
    if (is_executable_file(runtime_cache_binary)) {
        return runtime_cache_binary;
    }

    throw std::runtime_error(
        "api42-client-agent not found. Set API42_CLIENT_BINARY (or API_CLIENT_BINARY), install /usr/local/bin/api42-client-agent, "
        "or build runtime/cache/bin/api42-client-agent");
}

std::string build_tool_invocation(const fs::path& tool_path) {
    return shell_quote(tool_path.string());
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

void trim_log_file(const fs::path& log_file, std::size_t keep_last_lines) {
    std::ifstream in(log_file);
    if (!in) {
        return;
    }

    std::vector<std::string> lines;
    lines.reserve(keep_last_lines + 512);

    std::string line;
    while (std::getline(in, line)) {
        lines.push_back(line);
    }

    if (lines.size() <= keep_last_lines) {
        return;
    }

    std::ofstream out(log_file, std::ios::out | std::ios::trunc);
    if (!out) {
        return;
    }

    const std::size_t start = lines.size() - keep_last_lines;
    for (std::size_t i = start; i < lines.size(); ++i) {
        out << lines[i] << "\n";
    }
}

void log_fetcher(const fs::path& log_file, const std::string& message, bool to_stdout = true) {
    if (to_stdout) {
        std::cout << message << std::endl;
    }
    ensure_parent_directory(log_file);
    std::ofstream out(log_file, std::ios::out | std::ios::app);
    if (out) {
        out << message << "\n";
    }
}

std::string read_config_value(const fs::path& config_path, const std::string& key) {
    std::ifstream in(config_path);
    if (!in) {
        return "";
    }

    const std::string prefix = key + "=";
    std::string line;
    while (std::getline(in, line)) {
        std::string stripped = line;
        auto first = stripped.find_first_not_of(" \t");
        if (first == std::string::npos) {
            continue;
        }
        stripped = stripped.substr(first);
        if (stripped.rfind(prefix, 0) != 0) {
            continue;
        }

        std::string value = trim(stripped.substr(prefix.size()));
        if (value.empty()) {
            return "";
        }

        std::istringstream iss(value);
        std::string token;
        iss >> token;
        return token;
    }

    return "";
}

std::string read_config_value_multi(const std::vector<fs::path>& config_paths, const std::string& key) {
    for (const auto& path : config_paths) {
        const std::string value = read_config_value(path, key);
        if (!value.empty()) {
            return value;
        }
    }
    return "";
}

fs::path resolve_root_dir() {
    if (auto env_root = get_env_non_empty("ROOT_DIR"); env_root.has_value()) {
        return fs::path(*env_root);
    }

    std::error_code ec;
    fs::path cwd = fs::current_path(ec);
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

struct QueueRecord {
    std::string user_id;
    std::string enqueued_at_utc;
    std::string source;
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

    if (!is_digits_only(record.user_id) || record.enqueued_at_utc.empty() || record.source.empty()) {
        return std::nullopt;
    }

    return record;
}

std::string serialize_queue_record(const QueueRecord& record) {
    return record.user_id + "|" + record.enqueued_at_utc + "|" + record.source;
}

std::optional<QueueRecord> pop_queue_head(
    const fs::path& queue_path,
    const fs::path& lock_path,
    const std::string& default_source
) {
    FileLock lock(lock_path);

    std::ifstream in(queue_path);
    if (!in) {
        return std::nullopt;
    }

    std::vector<QueueRecord> records;
    std::string line;
    while (std::getline(in, line)) {
        auto record = parse_queue_record_line(line, default_source);
        if (!record.has_value()) {
            continue;
        }
        records.push_back(*record);
    }

    std::ofstream out(queue_path, std::ios::out | std::ios::trunc);
    if (!out) {
        return std::nullopt;
    }

    if (records.empty()) {
        return std::nullopt;
    }

    QueueRecord first_record = records.front();
    for (std::size_t i = 1; i < records.size(); ++i) {
        out << serialize_queue_record(records[i]) << "\n";
    }

    return first_record;
}

void append_queue_record(const fs::path& queue_path, const fs::path& lock_path, QueueRecord record) {
    if (!is_digits_only(record.user_id)) {
        return;
    }
    if (record.enqueued_at_utc.empty()) {
        record.enqueued_at_utc = now_utc_iso8601();
    }
    if (record.source.empty()) {
        record.source = "unknown";
    }

    FileLock lock(lock_path);
    std::ofstream out(queue_path, std::ios::out | std::ios::app);
    if (out) {
        out << serialize_queue_record(record) << "\n";
    }
}

void requeue_user_with_backoff(
    const fs::path& fetch_queue,
    const fs::path& fetch_lock,
    const fs::path& log_file,
    QueueRecord queue_record,
    const std::string& reason,
    long long backoff_ms
) {
    append_queue_record(fetch_queue, fetch_lock, queue_record);
    std::string message =
        "[" + now_utc_iso8601() + "] User " + queue_record.user_id + ": requeued (" + reason + ")";
    if (backoff_ms > 0) {
        message += " backoff=" + std::to_string(backoff_ms) + "ms";
    }
    log_fetcher(log_file, message);
    if (backoff_ms > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(backoff_ms));
    }
}

long long json_to_int_or_default(const json& value, long long fallback = 0) {
    try {
        if (value.is_number_integer()) {
            return value.get<long long>();
        }
        if (value.is_number_unsigned()) {
            return static_cast<long long>(value.get<unsigned long long>());
        }
        if (value.is_string()) {
            return parse_int64(value.get<std::string>()).value_or(fallback);
        }
    } catch (...) {
        return fallback;
    }
    return fallback;
}

std::string json_to_string_or_default(const json& value, const std::string& fallback = "") {
    try {
        if (value.is_string()) {
            return value.get<std::string>();
        }
    } catch (...) {
        return fallback;
    }
    return fallback;
}

std::optional<long long> extract_campus_id(const json& user) {
    if (!user.is_object()) {
        return std::nullopt;
    }

    if (user.contains("campus_users") && user["campus_users"].is_array() && !user["campus_users"].empty()) {
        std::optional<long long> best_primary_campus_id;
        std::optional<std::string> best_primary_updated_at;

        for (const auto& cu : user["campus_users"]) {
            if (!cu.is_object()) {
                continue;
            }
            if (!cu.value("is_primary", false) || !cu.contains("campus_id")) {
                continue;
            }

            std::optional<long long> campus_id;
            if (cu["campus_id"].is_number_integer()) {
                campus_id = cu["campus_id"].get<long long>();
            } else if (cu["campus_id"].is_string()) {
                campus_id = parse_int64(cu["campus_id"].get<std::string>());
            }
            if (!campus_id.has_value()) {
                continue;
            }

            std::optional<std::string> updated_at;
            if (cu.contains("updated_at") && cu["updated_at"].is_string()) {
                updated_at = cu["updated_at"].get<std::string>();
            }

            bool is_better = !best_primary_campus_id.has_value();
            if (!is_better) {
                if (updated_at.has_value() && !best_primary_updated_at.has_value()) {
                    is_better = true;
                } else if (
                    updated_at.has_value() &&
                    best_primary_updated_at.has_value() &&
                    *updated_at > *best_primary_updated_at
                ) {
                    is_better = true;
                }
            }

            if (is_better) {
                best_primary_campus_id = *campus_id;
                best_primary_updated_at = updated_at;
            }
        }

        if (best_primary_campus_id.has_value()) {
            return best_primary_campus_id;
        }

        // Fallback: first valid campus_id in campus_users.
        for (const auto& cu : user["campus_users"]) {
            if (!cu.is_object() || !cu.contains("campus_id")) {
                continue;
            }
            if (cu["campus_id"].is_number_integer()) {
                return cu["campus_id"].get<long long>();
            }
            if (cu["campus_id"].is_string()) {
                return parse_int64(cu["campus_id"].get<std::string>());
            }
        }
    }

    if (user.contains("campus") && user["campus"].is_array() && !user["campus"].empty()) {
        const auto& first = user["campus"][0];
        if (first.is_object() && first.contains("id")) {
            if (first["id"].is_number_integer()) {
                return first["id"].get<long long>();
            }
            if (first["id"].is_string()) {
                return parse_int64(first["id"].get<std::string>());
            }
        }
    }

    return std::nullopt;
}

}  // namespace

int main() {
    try {
        const fs::path root_dir = resolve_root_dir();

        fs::path runtime_dir = root_dir / ".." / "runtime";
        if (auto runtime_env = get_env_non_empty("RUNTIME_DIR"); runtime_env.has_value()) {
            runtime_dir = fs::path(*runtime_env);
        }

        fs::path logs_root = runtime_dir / "logs";
        fs::path exports_root = runtime_dir / "exports";
        fs::path backlog_dir = runtime_dir / "backlog";

        const fs::path log_dir = logs_root / "agents";
        const fs::path exports_users = exports_root / "09_users";

        ensure_directory(backlog_dir);
        ensure_directory(log_dir);
        ensure_directory(exports_users);

        const std::vector<fs::path> fetcher_config_paths = {
            root_dir / "app" / "fetcher" / "config" / "agents.config",
        };

        const std::string config_rate_limit_delay = read_config_value_multi(fetcher_config_paths, "RATE_LIMIT_DELAY");
        const double rate_limit_delay = parse_double(get_env_or_default(
            "RATE_LIMIT_DELAY",
            config_rate_limit_delay.empty() ? "6.0" : config_rate_limit_delay
        )).value_or(6.0);
        const long long retry_backoff_ms = std::max(
            0LL,
            parse_int64(get_env_or_default("FETCHER_RETRY_BACKOFF_MS", "1000")).value_or(1000)
        );
        const long long rate_limit_backoff_ms = std::max(
            retry_backoff_ms,
            parse_int64(get_env_or_default("FETCHER_429_BACKOFF_MS", "5000")).value_or(5000)
        );
        const long long slot_ms = std::max(
            0LL,
            parse_int64(get_env_or_default("FETCHER_SLOT_MS", "0")).value_or(0)
        );
        const long long slot_count = std::max(
            1LL,
            parse_int64(get_env_or_default("FETCHER_SLOT_COUNT", "1")).value_or(1)
        );
        const long long slot_index_raw = parse_int64(get_env_or_default("FETCHER_SLOT_INDEX", "0")).value_or(0);
        const long long slot_index = ((slot_index_raw % slot_count) + slot_count) % slot_count;

        const fs::path api_client_tool = resolve_api_client_tool(root_dir);

        const fs::path fetch_queue = fs::path(get_env_or_default(
            "FETCH_QUEUE_FILE",
            (backlog_dir / "fetch_queue_internal.txt").string()
        ));
        const fs::path process_queue = backlog_dir / "process_queue.txt";
        const fs::path process_lock = backlog_dir / "process_queue.txt.lock";
        const fs::path fetch_lock = fetch_queue.string() + ".lock";
        const std::string fetch_queue_source =
            fetch_queue.filename().string().find("external") != std::string::npos ? "external" : "internal";
        const fs::path log_file = log_dir / ("fetcher_" + fetch_queue_source + ".log");

        touch_file(fetch_queue);
        touch_file(process_queue);
        touch_file(process_lock);
        touch_file(fetch_lock);

        log_fetcher(
            log_file,
            "[" + now_utc_iso8601() + "] Fetcher started on queue " + fetch_queue.filename().string() +
                " (rate limit: " + std::to_string(rate_limit_delay) + "s per fetcher, slot=" +
                std::to_string(slot_index) + "/" + std::to_string(slot_count) +
                ", slot_ms=" + std::to_string(slot_ms) + ")"
        );

        long long counter = 0;
        long long fetch_errors = 0;

        while (true) {
            auto queue_item = pop_queue_head(fetch_queue, fetch_lock, fetch_queue_source);
            if (!queue_item.has_value()) {
                std::this_thread::sleep_for(std::chrono::seconds(5));
                continue;
            }
            const std::string user_id = queue_item->user_id;

            counter += 1;
            std::this_thread::sleep_for(std::chrono::duration<double>(rate_limit_delay));
            wait_for_fetch_slot(slot_ms, slot_index, slot_count);

            log_fetcher(log_file, "[" + now_utc_iso8601() + "] -> Fetching user " + user_id, false);

            std::string call_cmd =
                build_tool_invocation(api_client_tool) + " call-http " +
                shell_quote("/v2/users/" + user_id) + " 2>/dev/null || echo ''";

            int call_status = 0;
            std::string api_call_payload = run_bash(call_cmd, &call_status);
            (void)call_status;
            api_call_payload = trim(api_call_payload);

            if (api_call_payload.empty()) {
                log_fetcher(
                    log_file,
                    "[" + now_utc_iso8601() + "] User " + user_id + ": empty response (retrying)"
                );
                requeue_user_with_backoff(
                    fetch_queue,
                    fetch_lock,
                    log_file,
                    *queue_item,
                    "empty_response",
                    retry_backoff_ms
                );
                fetch_errors += 1;
                continue;
            }

            json call_json;
            try {
                call_json = json::parse(api_call_payload);
            } catch (...) {
                log_fetcher(
                    log_file,
                    "[" + now_utc_iso8601() + "] User " + user_id +
                        ": invalid api-client payload (retrying)"
                );
                requeue_user_with_backoff(
                    fetch_queue,
                    fetch_lock,
                    log_file,
                    *queue_item,
                    "api_client_payload_invalid",
                    retry_backoff_ms
                );
                fetch_errors += 1;
                continue;
            }

            long long http_code = 0;
            if (call_json.contains("http_code")) {
                http_code = json_to_int_or_default(call_json["http_code"], 0);
            }

            std::string user_json_text;
            if (call_json.contains("body")) {
                user_json_text = trim(json_to_string_or_default(call_json["body"], ""));
            }

            if (http_code == 429) {
                log_fetcher(
                    log_file,
                    "[" + now_utc_iso8601() + "] User " + user_id + ": HTTP 429 (retrying)"
                );
                requeue_user_with_backoff(
                    fetch_queue,
                    fetch_lock,
                    log_file,
                    *queue_item,
                    "http_429",
                    rate_limit_backoff_ms
                );
                fetch_errors += 1;
                continue;
            }

            if (http_code == 404) {
                log_fetcher(
                    log_file,
                    "[" + now_utc_iso8601() + "] User " + user_id + ": HTTP 404 (skipping)"
                );
                fetch_errors += 1;
                continue;
            }

            if (http_code == 401 || http_code >= 500 || (http_code >= 400 && http_code < 500) || http_code == 0) {
                log_fetcher(
                    log_file,
                    "[" + now_utc_iso8601() + "] User " + user_id +
                        ": HTTP " + std::to_string(http_code) + " (retrying)"
                );
                requeue_user_with_backoff(
                    fetch_queue,
                    fetch_lock,
                    log_file,
                    *queue_item,
                    "http_" + std::to_string(http_code),
                    retry_backoff_ms
                );
                fetch_errors += 1;
                continue;
            }

            if (user_json_text.empty()) {
                log_fetcher(
                    log_file,
                    "[" + now_utc_iso8601() + "] User " + user_id + ": empty body (retrying)"
                );
                requeue_user_with_backoff(
                    fetch_queue,
                    fetch_lock,
                    log_file,
                    *queue_item,
                    "empty_body",
                    retry_backoff_ms
                );
                fetch_errors += 1;
                continue;
            }

            json user_json;
            try {
                user_json = json::parse(user_json_text);
            } catch (...) {
                log_fetcher(
                    log_file,
                    "[" + now_utc_iso8601() + "] User " + user_id + ": invalid JSON (retrying)"
                );
                requeue_user_with_backoff(
                    fetch_queue,
                    fetch_lock,
                    log_file,
                    *queue_item,
                    "invalid_json",
                    retry_backoff_ms
                );
                fetch_errors += 1;
                continue;
            }

            long long campus_id = extract_campus_id(user_json).value_or(0);
            const fs::path campus_dir = exports_users / ("campus_" + std::to_string(campus_id));
            ensure_directory(campus_dir);

            const fs::path output_json = campus_dir / ("user_" + user_id + ".json");
            {
                std::ofstream out(output_json, std::ios::out | std::ios::trunc);
                if (out) {
                    out << user_json.dump();
                }
            }

            append_queue_record(
                process_queue,
                process_lock,
                QueueRecord{
                    user_id,
                    now_utc_iso8601(),
                    queue_item->source.empty() ? fetch_queue_source : queue_item->source,
                }
            );

            log_fetcher(
                log_file,
                "[" + now_utc_iso8601() + "] Fetched user " + user_id +
                    " (campus " + std::to_string(campus_id) + ") -> process_queue"
            );

            if ((counter % 20) == 0) {
                const auto fetch_queue_size = count_lines(fetch_queue);
                const auto process_queue_size = count_lines(process_queue);

                log_fetcher(
                    log_file,
                    "[" + now_utc_iso8601() + "] Stats: Fetched=" + std::to_string(counter) +
                        " | Errors=" + std::to_string(fetch_errors) +
                        " | FetchQueue=" + std::to_string(fetch_queue_size) +
                        " | ProcessQueue=" + std::to_string(process_queue_size)
                );

                if (count_lines(log_file) > 5500) {
                    trim_log_file(log_file, 5000);
                }
            }
        }
    } catch (const std::exception& ex) {
        std::cerr << "fetcher.cpp error: " << ex.what() << "\n";
        return 1;
    }
}
