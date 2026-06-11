#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <optional>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
#include <utility>
#include <vector>

#include <openssl/hmac.h>

#include "../../../third_party/nlohmann/json.hpp"

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace {

struct QueueUpdate {
    std::string uid;
    bool is_location_only{false};
};

struct QueueRecord {
    std::string uid;
    std::string enqueued_at_utc;
    std::string source;
};

struct EventCounts {
    int internal{0};
    int external{0};
};

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

std::string trim(std::string value) {
    const auto first = value.find_first_not_of(" \t\n\r");
    if (first == std::string::npos) {
        return "";
    }
    const auto last = value.find_last_not_of(" \t\n\r");
    return value.substr(first, last - first + 1);
}

std::optional<std::string> get_env_non_empty(const char* key) {
    const char* value = std::getenv(key);
    if (value == nullptr || *value == '\0') {
        return std::nullopt;
    }
    std::string out(value);
    if (trim(out).empty()) {
        return std::nullopt;
    }
    return out;
}

std::string get_env_or_default(const char* key, const std::string& fallback) {
    auto value = get_env_non_empty(key);
    return value.has_value() ? *value : fallback;
}

std::string first_non_empty(const std::vector<std::string>& values, const std::string& fallback = "") {
    for (const auto& value : values) {
        if (!trim(value).empty()) {
            return trim(value);
        }
    }
    return fallback;
}

std::optional<long long> parse_int64(const std::string& raw) {
    std::string text = trim(raw);
    if (text.empty()) {
        return std::nullopt;
    }
    char* end = nullptr;
    errno = 0;
    long long value = std::strtoll(text.c_str(), &end, 10);
    if (errno != 0 || end == text.c_str() || *end != '\0') {
        return std::nullopt;
    }
    return value;
}

long long parse_int64_or_default(const std::string& raw, long long fallback) {
    auto value = parse_int64(raw);
    return value.has_value() ? *value : fallback;
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

std::string now_utc_iso8601() {
    std::time_t now = std::time(nullptr);
    std::tm tm{};
    gmtime_r(&now, &tm);

    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm);
    return std::string(buf);
}

std::string utc_iso8601_from_epoch(std::time_t epoch) {
    std::tm tm{};
    gmtime_r(&epoch, &tm);

    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm);
    return std::string(buf);
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

class ExclusiveCreateLock {
  public:
    explicit ExclusiveCreateLock(const fs::path& path, int timeout_ms = 3000, int retry_delay_ms = 25)
        : path_(path) {
        ensure_parent_directory(path_);
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);

        while (true) {
            fd_ = ::open(path_.c_str(), O_CREAT | O_EXCL | O_RDWR, 0644);
            if (fd_ != -1) {
                break;
            }

            if (errno != EEXIST) {
                throw std::runtime_error("failed to create lock file: " + path_.string());
            }

            if (std::chrono::steady_clock::now() >= deadline) {
                throw std::runtime_error("timeout acquiring lock file: " + path_.string());
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(retry_delay_ms));
        }
    }

    ExclusiveCreateLock(const ExclusiveCreateLock&) = delete;
    ExclusiveCreateLock& operator=(const ExclusiveCreateLock&) = delete;

    ~ExclusiveCreateLock() {
        if (fd_ != -1) {
            ::close(fd_);
        }
        std::error_code ec;
        fs::remove(path_, ec);
    }

  private:
    fs::path path_;
    int fd_{-1};
};

std::string read_text_file(const fs::path& path) {
    std::ifstream in(path);
    if (!in) {
        return "";
    }
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

bool write_text_file(const fs::path& path, const std::string& content) {
    ensure_parent_directory(path);
    std::ofstream out(path, std::ios::out | std::ios::trunc);
    if (!out) {
        return false;
    }
    out << content;
    return static_cast<bool>(out);
}

bool write_all(int fd, const std::string& data) {
    std::size_t total = 0;
    while (total < data.size()) {
        const ssize_t n = ::write(fd, data.data() + total, data.size() - total);
        if (n <= 0) {
            return false;
        }
        total += static_cast<std::size_t>(n);
    }
    return true;
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

        std::string value = stripped.substr(prefix.size());
        value = trim(value);
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

json load_json_from_candidates(const std::vector<fs::path>& paths, const json& fallback) {
    for (const auto& path : paths) {
        std::ifstream in(path);
        if (!in) {
            continue;
        }
        try {
            return json::parse(in);
        } catch (...) {
            continue;
        }
    }
    return fallback;
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

void log_detect_changes(const fs::path& log_file, const std::string& order, const std::string& line) {
    if (order == "asc") {
        ensure_parent_directory(log_file);
        std::ofstream out(log_file, std::ios::out | std::ios::app);
        if (out) {
            out << line << "\n";
        }
        return;
    }

    fs::path lock_file = log_file;
    lock_file += ".lock";
    ensure_parent_directory(lock_file);

    int lock_fd = ::open(lock_file.c_str(), O_CREAT | O_RDWR, 0644);
    if (lock_fd != -1) {
        ::flock(lock_fd, LOCK_EX);
    }

    std::string existing = read_text_file(log_file);

    std::string tmp_template = log_file.string() + ".tmp.XXXXXX";
    std::vector<char> tmp_buf(tmp_template.begin(), tmp_template.end());
    tmp_buf.push_back('\0');
    int tmp_fd = ::mkstemp(tmp_buf.data());
    if (tmp_fd != -1) {
        std::string payload = line + "\n" + existing;
        (void)write_all(tmp_fd, payload);

        struct stat st {};
        if (::stat(log_file.c_str(), &st) == 0) {
            ::fchmod(tmp_fd, st.st_mode);
        }
        ::close(tmp_fd);
        ::rename(tmp_buf.data(), log_file.c_str());
    }

    if (lock_fd != -1) {
        ::flock(lock_fd, LOCK_UN);
        ::close(lock_fd);
    }
}

json load_json_file(const fs::path& path, const json& fallback) {
    std::ifstream in(path);
    if (!in) {
        return fallback;
    }
    try {
        return json::parse(in);
    } catch (...) {
        return fallback;
    }
}

void write_json_file(const fs::path& path, const json& payload) {
    ensure_parent_directory(path);
    std::ofstream out(path, std::ios::out | std::ios::trunc);
    if (!out) {
        return;
    }
    out << payload.dump();
}

bool is_digits_only(const std::string& value) {
    if (value.empty()) {
        return false;
    }
    return std::all_of(value.begin(), value.end(), [](unsigned char c) { return std::isdigit(c) != 0; });
}

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
    record.uid = trim(line.substr(0, sep1));
    record.enqueued_at_utc = trim(line.substr(sep1 + 1, sep2 - sep1 - 1));
    record.source = trim(line.substr(sep2 + 1));
    if (record.source.empty()) {
        record.source = default_source;
    }

    if (!is_digits_only(record.uid) || record.enqueued_at_utc.empty() || record.source.empty()) {
        return std::nullopt;
    }

    return record;
}

std::string serialize_queue_record(const QueueRecord& record) {
    return record.uid + "|" + record.enqueued_at_utc + "|" + record.source;
}

std::optional<double> parse_iso8601_to_epoch(const std::string& text);

std::vector<QueueRecord> read_queue(const fs::path& path, const std::string& default_source) {
    std::vector<QueueRecord> items;
    std::ifstream in(path);
    if (!in) {
        return items;
    }

    std::string line;
    while (std::getline(in, line)) {
        auto record = parse_queue_record_line(line, default_source);
        if (!record.has_value()) {
            continue;
        }
        items.push_back(*record);
    }

    return items;
}

void write_queue(const fs::path& path, const std::vector<QueueRecord>& items) {
    ensure_parent_directory(path);
    std::ofstream out(path, std::ios::out | std::ios::trunc);
    if (!out) {
        return;
    }
    for (const auto& record : items) {
        out << serialize_queue_record(record) << "\n";
    }
}

std::vector<QueueRecord> dedup_preserve(const std::vector<QueueRecord>& items) {
    std::set<std::string> seen;
    std::vector<QueueRecord> out;
    out.reserve(items.size());

    for (const auto& record : items) {
        if (seen.find(record.uid) != seen.end()) {
            continue;
        }
        seen.insert(record.uid);
        out.push_back(record);
    }
    return out;
}

void sort_queue_oldest_first(std::vector<QueueRecord>& items) {
    struct Decorated {
        QueueRecord record;
        std::optional<double> epoch;
        std::size_t index{0};
    };

    std::vector<Decorated> decorated;
    decorated.reserve(items.size());
    for (std::size_t i = 0; i < items.size(); ++i) {
        decorated.push_back(Decorated{
            items[i],
            parse_iso8601_to_epoch(items[i].enqueued_at_utc),
            i,
        });
    }

    std::stable_sort(decorated.begin(), decorated.end(), [](const Decorated& a, const Decorated& b) {
        if (a.epoch.has_value() && b.epoch.has_value()) {
            if (*a.epoch != *b.epoch) {
                return *a.epoch < *b.epoch;
            }
            if (a.record.uid != b.record.uid) {
                return a.record.uid < b.record.uid;
            }
            return a.index < b.index;
        }
        if (a.epoch.has_value() != b.epoch.has_value()) {
            return a.epoch.has_value();
        }
        return a.index < b.index;
    });

    for (std::size_t i = 0; i < decorated.size(); ++i) {
        items[i] = std::move(decorated[i].record);
    }
}

std::vector<QueueRecord> apply_queue_updates_fifo(
    const std::vector<QueueRecord>& existing,
    const std::vector<QueueUpdate>& updates,
    const std::string& queue_source
) {
    if (updates.empty()) {
        return existing;
    }

    std::vector<std::string> merged;
    merged.reserve(existing.size() + updates.size());
    for (const auto& record : existing) {
        merged.push_back(record.uid);
    }
    for (const auto& update : updates) {
        if (!update.uid.empty()) {
            merged.push_back(update.uid);
        }
    }

    std::map<std::string, QueueRecord> existing_by_uid;
    for (const auto& record : existing) {
        if (existing_by_uid.find(record.uid) == existing_by_uid.end()) {
            existing_by_uid.emplace(record.uid, record);
        }
    }

    const std::string enqueue_now = now_utc_iso8601();
    std::set<std::string> seen;
    std::vector<QueueRecord> out;
    out.reserve(merged.size());
    for (const auto& uid : merged) {
        if (uid.empty() || seen.find(uid) != seen.end()) {
            continue;
        }
        seen.insert(uid);

        auto it = existing_by_uid.find(uid);
        if (it != existing_by_uid.end()) {
            out.push_back(it->second);
            continue;
        }

        out.push_back(QueueRecord{uid, enqueue_now, queue_source});
    }
    return out;
}

std::string hmac_sha256_hex(const std::string& key, const std::string& payload) {
    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int digest_len = 0;

    HMAC(
        EVP_sha256(),
        key.data(),
        static_cast<int>(key.size()),
        reinterpret_cast<const unsigned char*>(payload.data()),
        payload.size(),
        digest,
        &digest_len
    );

    std::ostringstream out;
    out << std::hex << std::setfill('0');
    for (unsigned int i = 0; i < digest_len; ++i) {
        out << std::setw(2) << static_cast<int>(digest[i]);
    }
    return out.str();
}

std::vector<std::string> json_array_to_strings(const json& value) {
    std::vector<std::string> out;
    if (!value.is_array()) {
        return out;
    }
    for (const auto& item : value) {
        if (item.is_string()) {
            out.push_back(item.get<std::string>());
        }
    }
    return out;
}

std::optional<std::string> json_to_uid_string(const json& value) {
    try {
        if (value.is_string()) {
            return value.get<std::string>();
        }
        if (value.is_number_integer()) {
            return std::to_string(value.get<long long>());
        }
        if (value.is_number_unsigned()) {
            return std::to_string(value.get<unsigned long long>());
        }
        if (value.is_number_float()) {
            std::ostringstream out;
            out << value.get<double>();
            return out.str();
        }
    } catch (...) {
        return std::nullopt;
    }
    return std::nullopt;
}

std::optional<long long> json_to_int(const json& value) {
    try {
        if (value.is_number_integer()) {
            return value.get<long long>();
        }
        if (value.is_number_unsigned()) {
            return static_cast<long long>(value.get<unsigned long long>());
        }
        if (value.is_number_float()) {
            return static_cast<long long>(value.get<double>());
        }
        if (value.is_string()) {
            return parse_int64(value.get<std::string>());
        }
    } catch (...) {
        return std::nullopt;
    }
    return std::nullopt;
}

std::optional<double> to_number(const json& value) {
    try {
        if (value.is_null()) {
            return std::nullopt;
        }
        if (value.is_number()) {
            return value.get<double>();
        }
        if (value.is_string()) {
            std::string text = trim(value.get<std::string>());
            if (text.empty()) {
                return std::nullopt;
            }
            size_t idx = 0;
            double num = std::stod(text, &idx);
            if (idx != text.size()) {
                return std::nullopt;
            }
            return num;
        }
    } catch (...) {
        return std::nullopt;
    }
    return std::nullopt;
}

json normalize_location(const json& value) {
    if (value.is_null()) {
        return nullptr;
    }
    if (value.is_string() && value.get<std::string>().empty()) {
        return nullptr;
    }
    return value;
}

json object_value_or_null(const json& obj, const std::string& key) {
    if (!obj.is_object()) {
        return nullptr;
    }
    auto it = obj.find(key);
    if (it == obj.end()) {
        return nullptr;
    }
    return *it;
}

std::optional<double> parse_iso8601_to_epoch(const std::string& text) {
    static const std::regex pattern(
        R"(^([0-9]{4})-([0-9]{2})-([0-9]{2})T([0-9]{2}):([0-9]{2}):([0-9]{2})(\.[0-9]+)?(Z|([+-])([0-9]{2}):([0-9]{2}))?$)"
    );

    std::smatch match;
    if (!std::regex_match(text, match, pattern)) {
        return std::nullopt;
    }

    std::tm tm{};
    tm.tm_year = std::stoi(match[1].str()) - 1900;
    tm.tm_mon = std::stoi(match[2].str()) - 1;
    tm.tm_mday = std::stoi(match[3].str());
    tm.tm_hour = std::stoi(match[4].str());
    tm.tm_min = std::stoi(match[5].str());
    tm.tm_sec = std::stoi(match[6].str());

    time_t epoch = timegm(&tm);
    if (epoch == static_cast<time_t>(-1)) {
        return std::nullopt;
    }

    double seconds = static_cast<double>(epoch);

    if (match[7].matched) {
        seconds += std::stod(match[7].str());
    }

    if (match[8].matched && match[8].str() != "Z") {
        const std::string sign = match[9].str();
        const int tz_hour = std::stoi(match[10].str());
        const int tz_min = std::stoi(match[11].str());
        const int offset = tz_hour * 3600 + tz_min * 60;
        if (sign == "+") {
            seconds -= offset;
        } else {
            seconds += offset;
        }
    }

    return seconds;
}

std::optional<double> get_updated_timestamp(const json& user) {
    if (!user.is_object() || !user.contains("updated_at") || !user["updated_at"].is_string()) {
        return std::nullopt;
    }
    return parse_iso8601_to_epoch(user["updated_at"].get<std::string>());
}

json get_campus_id_raw(const json& user) {
    if (!user.is_object()) {
        return nullptr;
    }

    if (user.contains("campus_users") && user["campus_users"].is_array() && !user["campus_users"].empty()) {
        json best_primary_campus_id = nullptr;
        std::optional<double> best_primary_updated_at_epoch;

        for (const auto& cu : user["campus_users"]) {
            if (!cu.is_object() || !cu.value("is_primary", false) || !cu.contains("campus_id")) {
                continue;
            }

            std::optional<double> updated_at_epoch;
            if (cu.contains("updated_at") && cu["updated_at"].is_string()) {
                updated_at_epoch = parse_iso8601_to_epoch(cu["updated_at"].get<std::string>());
            }

            bool is_better = best_primary_campus_id.is_null();
            if (!is_better) {
                if (updated_at_epoch.has_value() && !best_primary_updated_at_epoch.has_value()) {
                    is_better = true;
                } else if (
                    updated_at_epoch.has_value() &&
                    best_primary_updated_at_epoch.has_value() &&
                    *updated_at_epoch > *best_primary_updated_at_epoch
                ) {
                    is_better = true;
                }
            }

            if (is_better) {
                best_primary_campus_id = cu["campus_id"];
                best_primary_updated_at_epoch = updated_at_epoch;
            }
        }

        if (!best_primary_campus_id.is_null()) {
            return best_primary_campus_id;
        }

        // Fallback: first valid campus_id in campus_users.
        for (const auto& cu : user["campus_users"]) {
            if (cu.is_object() && cu.contains("campus_id")) {
                return cu["campus_id"];
            }
        }
    }

    if (user.contains("campus") && user["campus"].is_array() && !user["campus"].empty()) {
        const auto& first = user["campus"][0];
        if (first.is_object() && first.contains("id")) {
            return first["id"];
        }
    }

    return nullptr;
}

std::optional<long long> resolve_campus_from_exports(const fs::path& exports_users_dir, const std::string& uid) {
    if (uid.empty()) {
        return std::nullopt;
    }

    std::error_code ec;
    if (!fs::exists(exports_users_dir, ec) || !fs::is_directory(exports_users_dir, ec)) {
        return std::nullopt;
    }

    const std::string user_filename = "user_" + uid + ".json";

    for (const auto& entry : fs::directory_iterator(exports_users_dir, ec)) {
        if (ec) {
            break;
        }

        const fs::path campus_dir = entry.path();
        const std::string dirname = campus_dir.filename().string();
        if (dirname.rfind("campus_", 0) != 0) {
            continue;
        }

        auto campus_id = parse_int64(dirname.substr(7));
        if (!campus_id.has_value()) {
            continue;
        }

        std::error_code exists_ec;
        if (fs::exists(campus_dir / user_filename, exists_ec)) {
            return campus_id;
        }
    }

    return std::nullopt;
}

json build_fingerprint_payload(const json& user, const std::vector<std::string>& fields) {
    json filtered = json::object();

    for (const auto& key : fields) {
        if (!user.contains(key)) {
            continue;
        }
        const auto& value = user[key];
        if (key == "location") {
            const bool disconnected = value.is_null() || (value.is_string() && value.get<std::string>().empty());
            filtered[key] = disconnected ? 0 : 1;
        } else {
            filtered[key] = value;
        }
    }

    return filtered;
}

std::string fingerprint(const json& user, const std::vector<std::string>& fields, const std::string& hmac_key) {
    const json filtered = build_fingerprint_payload(user, fields);
    const std::string payload = filtered.dump();
    return hmac_sha256_hex(hmac_key, payload);
}

json build_snapshot(const json& user) {
    json snap = json::object();

    const std::vector<std::string> fields = {
        "login",
        "first_name",
        "last_name",
        "correction_point",
        "wallet",
        "location",
    };

    for (const auto& field : fields) {
        if (field == "location") {
            snap[field] = normalize_location(object_value_or_null(user, field));
        } else if (field == "correction_point" || field == "wallet") {
            auto num = to_number(object_value_or_null(user, field));
            if (num.has_value()) {
                snap[field] = *num;
            } else {
                snap[field] = nullptr;
            }
        } else {
            snap[field] = object_value_or_null(user, field);
        }
    }

    return snap;
}

std::map<std::string, json> build_event_changes(const json& baseline, const json& current) {
    std::map<std::string, json> events;

    if (!baseline.is_object() || !current.is_object()) {
        return events;
    }

    auto add_change = [&](const std::string& event_type, const json& change) {
        if (!events.count(event_type)) {
            events[event_type] = json::array();
        }
        events[event_type].push_back(change);
    };

    for (const auto& key : {"login", "first_name", "last_name"}) {
        const json old_value = object_value_or_null(baseline, key);
        const json new_value = object_value_or_null(current, key);
        if (old_value != new_value) {
            add_change("data", json{{"path", key}, {"old", old_value}, {"new", new_value}});
        }
    }

    const json old_loc = normalize_location(object_value_or_null(baseline, "location"));
    const json new_loc = normalize_location(object_value_or_null(current, "location"));
    if (old_loc != new_loc) {
        json change = {{"path", "location"}, {"old", old_loc}, {"new", new_loc}};
        if (old_loc.is_null() && !new_loc.is_null()) {
            add_change("connection", change);
        } else if (!old_loc.is_null() && new_loc.is_null()) {
            add_change("deconnection", change);
        }
    }

    const auto old_cp = to_number(object_value_or_null(baseline, "correction_point"));
    const auto new_cp = to_number(object_value_or_null(current, "correction_point"));
    if (old_cp.has_value() && new_cp.has_value() && *old_cp != *new_cp) {
        json change = {{"path", "correction_point"}, {"old", *old_cp}, {"new", *new_cp}};
        double delta = *new_cp - *old_cp;
        if (delta < 0) {
            add_change("evaluation", change);
        } else if (delta > 0) {
            add_change("correction", change);
        }
    }

    const auto old_wallet = to_number(object_value_or_null(baseline, "wallet"));
    const auto new_wallet = to_number(object_value_or_null(current, "wallet"));
    if (old_wallet.has_value() && new_wallet.has_value() && *old_wallet != *new_wallet) {
        add_change("wallet", json{{"path", "wallet"}, {"old", *old_wallet}, {"new", *new_wallet}});
    }

    return events;
}

json normalize_tracked_field(const json& object_value, const std::string& field) {
    if (field == "location") {
        return normalize_location(object_value);
    }
    if (field == "correction_point" || field == "wallet") {
        auto num = to_number(object_value);
        if (num.has_value()) {
            return *num;
        }
        return nullptr;
    }
    return object_value;
}

json build_error_debug_changes(
    const json& baseline_snapshot,
    const json& current_user,
    const std::vector<std::string>& tracked_fields,
    const json& previous_fingerprint_payload,
    const json& current_fingerprint_payload,
    const std::optional<std::string>& previous_hash,
    const std::string& current_hash,
    const std::optional<std::string>& previous_fingerprint_key,
    const std::string& current_fingerprint_key
) {
    json changes = json::array();

    for (const auto& field : tracked_fields) {
        const json old_norm = object_value_or_null(baseline_snapshot, field);
        const json new_raw = object_value_or_null(current_user, field);
        const json new_norm = normalize_tracked_field(new_raw, field);

        const bool classified_delta = old_norm != new_norm;

        bool raw_delta = false;
        if (field == "location") {
            const bool old_connected = !normalize_location(old_norm).is_null();
            const bool new_connected = !normalize_location(new_raw).is_null();
            raw_delta = old_connected != new_connected;
        } else {
            raw_delta = old_norm != new_raw;
        }

        if (!classified_delta && !raw_delta) {
            continue;
        }

        json change = {
            {"path", field},
            {"old", old_norm},
            {"new", new_norm},
        };

        if (raw_delta && !classified_delta) {
            change["reason"] = "raw_changed_without_classified_delta";
        }

        if (new_raw != new_norm) {
            change["new_raw"] = new_raw;
        }

        changes.push_back(change);
    }

    const bool has_tracked_delta = !changes.empty();
    changes.push_back(json{
        {"path", "_fingerprint"},
        {"reason", "unclassified_fingerprint_change"},
        {"previous_hash", previous_hash.has_value() ? json(*previous_hash) : json(nullptr)},
        {"current_hash", current_hash},
        {"previous_fingerprint_key", previous_fingerprint_key.has_value() ? json(*previous_fingerprint_key) : json(nullptr)},
        {"current_fingerprint_key", current_fingerprint_key},
        {"previous_payload", previous_fingerprint_payload.is_null() ? json(nullptr) : previous_fingerprint_payload},
        {"current_payload", current_fingerprint_payload},
    });

    if (!has_tracked_delta) {
        changes.push_back(json{
            {"path", "_debug"},
            {"reason", "fingerprint_changed_but_no_tracked_field_delta"},
            {"old_snapshot", baseline_snapshot},
            {"new_snapshot", build_snapshot(current_user)},
        });
    }

    return changes;
}

std::string bucket_for(const std::string& fp_key) {
    return fp_key == "internal" ? "int" : "ext";
}

std::pair<int, int> count_pair(const std::map<std::string, EventCounts>& event_counts, const std::string& name) {
    auto it = event_counts.find(name);
    if (it == event_counts.end()) {
        return {0, 0};
    }
    return {it->second.internal, it->second.external};
}

void bump_counts(std::map<std::string, EventCounts>& event_counts, const std::set<std::string>& types, const std::string& fp_key) {
    const bool internal = bucket_for(fp_key) == "int";
    for (const auto& ev : types) {
        auto it = event_counts.find(ev);
        if (it == event_counts.end()) {
            continue;
        }
        if (internal) {
            it->second.internal += 1;
        } else {
            it->second.external += 1;
        }
    }
}

void bump_error(int& err_internal, int& err_external, const std::string& fp_key = "") {
    if (bucket_for(fp_key) == "int") {
        err_internal += 1;
    } else {
        err_external += 1;
    }
}

std::string join_plus(const std::vector<std::string>& values) {
    if (values.empty()) {
        return "";
    }
    std::ostringstream out;
    for (size_t i = 0; i < values.size(); ++i) {
        if (i != 0) {
            out << "+";
        }
        out << values[i];
    }
    return out.str();
}

json fetch_delta_users_via_api_client(const fs::path& root_dir, int window_seconds) {
    std::time_t end_time = std::time(nullptr);
    std::time_t start_time = end_time - std::max(window_seconds, 1);

    const std::string end_iso = utc_iso8601_from_epoch(end_time);
    const std::string start_iso = utc_iso8601_from_epoch(start_time);
    const fs::path api_client_tool = resolve_api_client_tool(root_dir);

    json accum = json::array();
    int page = 1;
    while (true) {
        std::ostringstream endpoint;
        endpoint
            << "/v2/cursus/21/users"
            << "?range%5Bupdated_at%5D=" << start_iso << "," << end_iso
            << "&filter%5Bkind%5D=student"
            << "&alumni_p=false"
            << "&per_page=100"
            << "&page=" << page
            << "&sort=-updated_at";

        std::string command =
            build_tool_invocation(api_client_tool) + " call " +
            shell_quote(endpoint.str()) + " 2>/dev/null || echo '[]'";

        int call_status = 0;
        std::string page_text = run_bash(command, &call_status);
        (void)call_status;

        json page_json;
        try {
            page_json = json::parse(page_text.empty() ? "[]" : page_text);
        } catch (...) {
            break;
        }

        if (!page_json.is_array() || page_json.empty()) {
            break;
        }
        for (const auto& item : page_json) {
            accum.push_back(item);
        }

        page += 1;
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    return accum;
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

}  // namespace

int run_detector_once() {
    try {
        const fs::path root_dir = resolve_root_dir();

        fs::path runtime_dir = root_dir / ".." / "runtime";
        if (auto runtime_env = get_env_non_empty("RUNTIME_DIR"); runtime_env.has_value()) {
            runtime_dir = fs::path(*runtime_env);
        }

        fs::path logs_root = runtime_dir / "logs";
        fs::path logs_agents_dir = logs_root / "agents";
        fs::path exports_root = runtime_dir / "exports";
        fs::path backlog_dir = runtime_dir / "backlog";
        fs::path cache_root = runtime_dir / "cache";

        fs::path cache_dir = cache_root / "raw_detect";
        fs::path exports_dir = exports_root / "09_users";

        ensure_directory(backlog_dir);
        ensure_directory(logs_root);
        ensure_directory(logs_agents_dir);
        ensure_directory(cache_dir);
        ensure_directory(exports_dir);

        const fs::path hash_file = backlog_dir / "detector_hashes.json";
        const fs::path internal_queue = backlog_dir / "fetch_queue_internal.txt";
        const fs::path external_queue = backlog_dir / "fetch_queue_external.txt";
        const fs::path internal_lock = backlog_dir / "fetch_queue_internal.txt.lock";
        const fs::path external_lock = backlog_dir / "fetch_queue_external.txt.lock";
        const fs::path events_queue = backlog_dir / "events_queue.jsonl";
        const fs::path events_queue_mutex = backlog_dir / "events_queue.mutex";

        touch_file(hash_file);
        touch_file(internal_queue);
        touch_file(external_queue);
        touch_file(events_queue);
        touch_file(internal_lock);
        touch_file(external_lock);

        const fs::path log_file = logs_agents_dir / "detector.log";
        const std::string log_timestamp = now_utc_iso8601();
        const pid_t pid = ::getpid();
        const std::string log_order = get_env_or_default("DETECT_CHANGES_LOG_ORDER", "asc");

        const std::vector<fs::path> detector_config_paths = {
            root_dir / "app" / "detector" / "config" / "agents.config",
        };

        const std::string config_time_window = read_config_value_multi(detector_config_paths, "TIME_WINDOW");
        const std::string default_max_window = first_non_empty({config_time_window}, "65");
        const std::string max_window = get_env_or_default("TIME_WINDOW", default_max_window);

        const long long internal_campus_id = parse_int64_or_default(get_env_or_default("CAMPUS_ID", "21"), 21);

        const std::string config_backlog_nint = read_config_value_multi(detector_config_paths, "BACKLOG_NINT_THRESHOLD");
        const std::string config_backlog_next = read_config_value_multi(detector_config_paths, "BACKLOG_NEXT_THRESHOLD");

        const std::string backlog_nint_value = first_non_empty({
            get_env_non_empty("BACKLOG_NINT_THRESHOLD").value_or(""),
            config_backlog_nint,
            "100",
        });

        const std::string backlog_next_value = first_non_empty({
            get_env_non_empty("BACKLOG_NEXT_THRESHOLD").value_or(""),
            config_backlog_next,
            "500",
        });

        const int backlog_nint_threshold = static_cast<int>(parse_int64_or_default(backlog_nint_value, 100));
        const int backlog_next_threshold = static_cast<int>(parse_int64_or_default(backlog_next_value, 500));

        const int max_window_secs = static_cast<int>(parse_int64_or_default(max_window, 65));
        json raw_json = fetch_delta_users_via_api_client(root_dir, max_window_secs);
        if (!raw_json.is_array()) {
            log_detect_changes(log_file, log_order,
                "[" + log_timestamp + "] [pid=" + std::to_string(pid) + "] ERROR: Invalid JSON response");
            return 0;
        }

        std::size_t detect_count = raw_json.is_array() ? raw_json.size() : 0;
        if (detect_count == 0) {
            const std::size_t cur_qint = count_lines(internal_queue);
            const std::size_t cur_qext = count_lines(external_queue);

            std::ostringstream empty_log;
            empty_log << "[" << log_timestamp << "] [pid=" << pid << "] "
                      << "detect=0 fp=0 int=0 ext=0 qint=" << cur_qint
                      << " qext=" << cur_qext << " drop=0 WARN=empty_window";
            log_detect_changes(log_file, log_order, empty_log.str());
            return 0;
        }

        const std::string pretty_raw = raw_json.dump(2);

        std::string tmp_template = "/tmp/detector_users_XXXXXX";
        std::vector<char> tmp_buf(tmp_template.begin(), tmp_template.end());
        tmp_buf.push_back('\0');
        int tmp_fd = ::mkstemp(tmp_buf.data());
        if (tmp_fd == -1) {
            throw std::runtime_error("failed to create temp json file");
        }
        std::string tmp_json_path = tmp_buf.data();
        (void)write_all(tmp_fd, pretty_raw);
        ::close(tmp_fd);

        write_text_file(cache_dir / "users_latest.json", pretty_raw);

        FileLock internal_queue_lock(internal_lock);
        FileLock external_queue_lock(external_lock);

        json config = load_json_from_candidates({
            root_dir / "app" / "detector" / "config" / "detector_fields.json",
        }, json::object());

        std::vector<std::string> internal_fields =
            json_array_to_strings(config.value("internals", json::object()).value("fields", json::array()));
        std::vector<std::string> external_fields =
            json_array_to_strings(config.value("externals", json::object()).value("fields", json::array()));
        if (external_fields.empty()) {
            external_fields = internal_fields;
        }

        const std::string hmac_key_internal = config.value("hmac_keys", json::object()).value(
            "internal", "42network_internal_detection");
        const std::string hmac_key_external = config.value("hmac_keys", json::object()).value(
            "external", "42network_external_detection");

        json users = load_json_file(tmp_json_path, json::array());
        if (!users.is_array()) {
            users = json::array();
        }

        json hashes = load_json_file(hash_file, json::object());
        if (!hashes.is_object()) {
            hashes = json::object();
        }

        std::vector<QueueRecord> existing_internal = dedup_preserve(read_queue(internal_queue, "internal"));
        std::vector<QueueRecord> existing_external = dedup_preserve(read_queue(external_queue, "external"));

        int changed_internal_count = 0;
        int changed_external_count = 0;
        int fp_changes = 0;

        std::vector<json> events_payload;

        std::map<std::string, EventCounts> event_counts;
        for (const auto& name : {"connection", "deconnection", "wallet", "correction", "evaluation", "data", "new_seen"}) {
            event_counts[name] = EventCounts{};
        }

        int error_int = 0;
        int error_ext = 0;

        std::vector<QueueUpdate> queue_updates_internal;
        std::vector<QueueUpdate> queue_updates_external;

        std::set<std::string> send_internal_uids;
        std::set<std::string> send_external_uids;
        std::map<std::string, std::optional<long long>> campus_lookup_cache;

        const std::vector<std::string> event_order = {
            "data",
            "connection",
            "deconnection",
            "evaluation",
            "correction",
            "wallet",
        };

        for (const auto& user : users) {
            if (!user.is_object()) {
                bump_error(error_int, error_ext);
                continue;
            }

            if (user.contains("label") && user["label"].is_string() && user["label"].get<std::string>() == "error") {
                bump_error(error_int, error_ext);
                continue;
            }

            if (!user.contains("id") || user["id"].is_null()) {
                bump_error(error_int, error_ext);
                continue;
            }

            auto uid_opt = json_to_uid_string(user["id"]);
            if (!uid_opt.has_value() || uid_opt->empty()) {
                bump_error(error_int, error_ext);
                continue;
            }
            const std::string uid_str = *uid_opt;

            auto uid_number = parse_int64(uid_str);
            if (!uid_number.has_value()) {
                bump_error(error_int, error_ext);
                continue;
            }

            std::optional<long long> campus_id = std::nullopt;
            const json campus_id_raw = get_campus_id_raw(user);
            if (!campus_id_raw.is_null()) {
                campus_id = json_to_int(campus_id_raw);
            }

            if (!campus_id.has_value()) {
                auto it = campus_lookup_cache.find(uid_str);
                if (it != campus_lookup_cache.end()) {
                    campus_id = it->second;
                } else {
                    auto resolved = resolve_campus_from_exports(exports_dir, uid_str);
                    campus_lookup_cache.emplace(uid_str, resolved);
                    campus_id = resolved;
                }
            }

            json last_entry = nullptr;
            if (hashes.contains(uid_str)) {
                last_entry = hashes[uid_str];
            }

            std::optional<std::string> last_hash_value;
            if (last_entry.is_object() && last_entry.contains("hash") && last_entry["hash"].is_string()) {
                last_hash_value = last_entry["hash"].get<std::string>();
            } else if (last_entry.is_string()) {
                last_hash_value = last_entry.get<std::string>();
            }

            std::optional<std::string> last_fingerprint_key;
            if (
                last_entry.is_object() &&
                last_entry.contains("fingerprint_key") &&
                last_entry["fingerprint_key"].is_string()
            ) {
                last_fingerprint_key = last_entry["fingerprint_key"].get<std::string>();
            }

            json last_fingerprint_payload = nullptr;
            if (
                last_entry.is_object() &&
                last_entry.contains("fingerprint_payload") &&
                last_entry["fingerprint_payload"].is_object()
            ) {
                last_fingerprint_payload = last_entry["fingerprint_payload"];
            }

            const bool is_internal = campus_id.has_value() && *campus_id == internal_campus_id;
            const std::string fingerprint_key = is_internal ? "internal" : "external";
            const std::vector<std::string>& fields = (fingerprint_key == "internal") ? internal_fields : external_fields;
            const std::string& hmac_key = (fingerprint_key == "internal") ? hmac_key_internal : hmac_key_external;
            const json campus_json = campus_id.has_value() ? json(*campus_id) : json(nullptr);

            const json current_fingerprint_payload = build_fingerprint_payload(user, fields);
            const std::string fp = fingerprint(user, fields, hmac_key);
            if (last_hash_value.has_value() && *last_hash_value == fp) {
                continue;
            }

            json baseline = nullptr;
            bool has_baseline = false;
            if (last_entry.is_object() && last_entry.contains("snapshot") && last_entry["snapshot"].is_object()) {
                baseline = last_entry["snapshot"];
                has_baseline = true;
            }

            bool location_changed = false;
            bool wallet_changed = false;
            bool name_changed = false;
            std::optional<double> cp_delta = std::nullopt;
            bool new_seen_event = false;

            if (has_baseline) {
                location_changed = normalize_location(object_value_or_null(baseline, "location"))
                    != normalize_location(object_value_or_null(user, "location"));

                wallet_changed = to_number(object_value_or_null(baseline, "wallet"))
                    != to_number(object_value_or_null(user, "wallet"));

                auto cp_old = to_number(object_value_or_null(baseline, "correction_point"));
                auto cp_new = to_number(object_value_or_null(user, "correction_point"));
                if (cp_old.has_value() && cp_new.has_value()) {
                    cp_delta = *cp_new - *cp_old;
                }

                for (const auto& key : {"login", "first_name", "last_name"}) {
                    if (object_value_or_null(baseline, key) != object_value_or_null(user, key)) {
                        name_changed = true;
                        break;
                    }
                }
            } else {
                new_seen_event = true;
            }

            const bool is_location_only = has_baseline
                && location_changed
                && !(wallet_changed || name_changed || (cp_delta.has_value() && *cp_delta != 0.0));

            if (fingerprint_key == "internal") {
                changed_internal_count += 1;
            } else {
                changed_external_count += 1;
            }

            const json current_snapshot = build_snapshot(user);
            std::vector<json> new_events;

            if (new_seen_event) {
                new_events.push_back(json{
                    {"user_id", *uid_number},
                    {"user_login", user.value("login", "")},
                    {"campus_id", campus_json},
                    {"updated_at", user.value("updated_at", "")},
                    {"first_snapshot", true},
                    {"types", json::array({"new_seen"})},
                    {"changes", json::array()},
                    {"source", "detector"},
                    {"ts", static_cast<long long>(std::time(nullptr))},
                });
                bump_counts(event_counts, std::set<std::string>{"new_seen"}, fingerprint_key);
            } else {
                const auto type_changes = has_baseline ? build_event_changes(baseline, user) : std::map<std::string, json>{};
                if (!type_changes.empty()) {
                    for (const auto& event_type : event_order) {
                        auto it = type_changes.find(event_type);
                        if (it == type_changes.end()) {
                            continue;
                        }

                        new_events.push_back(json{
                            {"user_id", *uid_number},
                            {"user_login", user.value("login", "")},
                            {"campus_id", campus_json},
                            {"updated_at", user.value("updated_at", "")},
                            {"first_snapshot", false},
                            {"types", json::array({event_type})},
                            {"changes", it->second},
                            {"source", "detector"},
                            {"ts", static_cast<long long>(std::time(nullptr))},
                        });

                        bump_counts(event_counts, std::set<std::string>{event_type}, fingerprint_key);
                    }
                }

                if (new_events.empty()) {
                    const json debug_changes = build_error_debug_changes(
                        baseline,
                        user,
                        fields,
                        last_fingerprint_payload,
                        current_fingerprint_payload,
                        last_hash_value,
                        fp,
                        last_fingerprint_key,
                        fingerprint_key
                    );
                    new_events.push_back(json{
                        {"user_id", *uid_number},
                        {"user_login", user.value("login", "")},
                        {"campus_id", campus_json},
                        {"updated_at", user.value("updated_at", "")},
                        {"first_snapshot", false},
                        {"types", json::array({"error"})},
                        {"changes", debug_changes},
                        {"source", "detector"},
                        {"ts", static_cast<long long>(std::time(nullptr))},
                    });
                    bump_error(error_int, error_ext, fingerprint_key);
                }
            }

            for (const auto& event : new_events) {
                events_payload.push_back(event);
            }

            if (!is_location_only) {
                QueueUpdate update{uid_str, false};
                if (fingerprint_key == "internal") {
                    queue_updates_internal.push_back(update);
                    send_internal_uids.insert(uid_str);
                } else {
                    queue_updates_external.push_back(update);
                    send_external_uids.insert(uid_str);
                }
            }

            json hash_entry = {
                {"hash", fp},
                {"timestamp", get_updated_timestamp(user).has_value() ? json(*get_updated_timestamp(user)) : json(nullptr)},
                {"campus_id", campus_json},
                {"fingerprint_key", fingerprint_key},
                {"fingerprint_payload", current_fingerprint_payload},
                {"snapshot", current_snapshot},
            };
            hashes[uid_str] = hash_entry;
            fp_changes += 1;
        }

        std::vector<QueueRecord> internal_ids =
            apply_queue_updates_fifo(existing_internal, queue_updates_internal, "internal");
        std::vector<QueueRecord> external_ids =
            apply_queue_updates_fifo(existing_external, queue_updates_external, "external");

        std::set<std::string> internal_set;
        for (const auto& record : internal_ids) {
            internal_set.insert(record.uid);
        }

        std::vector<QueueRecord> filtered_external;
        filtered_external.reserve(external_ids.size());
        for (const auto& record : external_ids) {
            if (!internal_set.count(record.uid)) {
                filtered_external.push_back(record);
            }
        }
        external_ids = std::move(filtered_external);

        std::set<std::string> drop_candidates;
        for (const auto& item : queue_updates_external) {
            if (item.is_location_only) {
                drop_candidates.insert(item.uid);
            }
        }

        if (!drop_candidates.empty()) {
            std::vector<QueueRecord> retained;
            retained.reserve(external_ids.size());
            for (const auto& record : external_ids) {
                if (!drop_candidates.count(record.uid)) {
                    retained.push_back(record);
                }
            }
            external_ids = std::move(retained);
        }

        // Keep queues in FIFO order (oldest first, newest last).
        sort_queue_oldest_first(internal_ids);
        sort_queue_oldest_first(external_ids);

        write_queue(internal_queue, internal_ids);
        write_queue(external_queue, external_ids);

        std::vector<std::string> warns;
        if (static_cast<int>(internal_ids.size()) >= backlog_nint_threshold) {
            warns.push_back("Nint_backlog");
        }
        if (static_cast<int>(external_ids.size()) >= backlog_next_threshold) {
            warns.push_back("Next_backlog");
        }

        if (!events_payload.empty()) {
            ExclusiveCreateLock events_lock(events_queue_mutex);
            const std::string existing_events = read_text_file(events_queue);

            std::ostringstream block;
            for (const auto& event : events_payload) {
                block << event.dump() << "\n";
            }

            write_text_file(events_queue, block.str() + existing_events);
        }

        const auto [conn_int, conn_ext] = count_pair(event_counts, "connection");
        const auto [disc_int, disc_ext] = count_pair(event_counts, "deconnection");
        const auto [wallet_int, wallet_ext] = count_pair(event_counts, "wallet");
        const auto [corr_int, corr_ext] = count_pair(event_counts, "correction");
        const auto [eval_int, eval_ext] = count_pair(event_counts, "evaluation");
        const auto [data_int, data_ext] = count_pair(event_counts, "data");
        const auto [new_int, new_ext] = count_pair(event_counts, "new_seen");

        json result = {
            {"detect", static_cast<int>(users.size())},
            {"fp", fp_changes},
            {"int", changed_internal_count},
            {"ext", changed_external_count},
            {"send_internal", static_cast<int>(send_internal_uids.size())},
            {"send_external", static_cast<int>(send_external_uids.size())},
            {"qint", static_cast<int>(internal_ids.size())},
            {"qext", static_cast<int>(external_ids.size())},
            {"warn", join_plus(warns)},
            {"events", static_cast<int>(events_payload.size())},
            {"events_connection_int", conn_int},
            {"events_connection_ext", conn_ext},
            {"events_deconnection_int", disc_int},
            {"events_deconnection_ext", disc_ext},
            {"events_wallet_int", wallet_int},
            {"events_wallet_ext", wallet_ext},
            {"events_correction_int", corr_int},
            {"events_correction_ext", corr_ext},
            {"events_evaluation_int", eval_int},
            {"events_evaluation_ext", eval_ext},
            {"events_data_int", data_int},
            {"events_data_ext", data_ext},
            {"events_new_seen_int", new_int},
            {"events_new_seen_ext", new_ext},
            {"events_error_int", error_int},
            {"events_error_ext", error_ext},
        };

        write_json_file(hash_file, hashes);
        std::error_code rm_ec;
        fs::remove(tmp_json_path, rm_ec);

        std::string log_line =
            "[" + log_timestamp + "] [pid=" + std::to_string(pid) + "] "
            "detect=" + std::to_string(result.value("detect", 0)) +
            " fp=" + std::to_string(result.value("fp", 0)) +
            " events=" + std::to_string(result.value("events", 0));

        log_line += " send=(" +
            std::to_string(result.value("send_internal", 0)) + "/" +
            std::to_string(result.value("send_external", 0)) + ")";
        log_line += " disconnection=(" +
            std::to_string(result.value("events_deconnection_int", 0)) + "/" +
            std::to_string(result.value("events_deconnection_ext", 0)) + ")";
        log_line += " connection=(" +
            std::to_string(result.value("events_connection_int", 0)) + "/" +
            std::to_string(result.value("events_connection_ext", 0)) + ")";
        log_line += " wallet=(" +
            std::to_string(result.value("events_wallet_int", 0)) + "/" +
            std::to_string(result.value("events_wallet_ext", 0)) + ")";
        log_line += " correction=(" +
            std::to_string(result.value("events_correction_int", 0)) + "/" +
            std::to_string(result.value("events_correction_ext", 0)) + ")";
        log_line += " evaluation=(" +
            std::to_string(result.value("events_evaluation_int", 0)) + "/" +
            std::to_string(result.value("events_evaluation_ext", 0)) + ")";
        log_line += " data=(" +
            std::to_string(result.value("events_data_int", 0)) + "/" +
            std::to_string(result.value("events_data_ext", 0)) + ")";
        log_line += " new_seen=(" +
            std::to_string(result.value("events_new_seen_int", 0)) + "/" +
            std::to_string(result.value("events_new_seen_ext", 0)) + ")";
        log_line += " error=(" +
            std::to_string(result.value("events_error_int", 0)) + "/" +
            std::to_string(result.value("events_error_ext", 0)) + ")";

        const std::string warn_val = result.value("warn", "");
        if (!warn_val.empty() && warn_val != "null") {
            log_line += " WARN=" + warn_val;
        }

        log_detect_changes(log_file, log_order, log_line);
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "detector.cpp error: " << ex.what() << "\n";
        return 1;
    }
}

int main(int argc, char** argv) {
    bool loop_mode = false;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--loop") {
            loop_mode = true;
        }
    }

    if (!loop_mode) {
        return run_detector_once();
    }

    const fs::path root_dir = resolve_root_dir();
    const std::vector<fs::path> detector_config_paths = {
        root_dir / "app" / "detector" / "config" / "agents.config",
    };

    const std::string config_interval = read_config_value_multi(detector_config_paths, "DETECTOR_INTERVAL");
    const long long interval_default = parse_int64_or_default(config_interval, 60);
    const long long interval = parse_int64_or_default(
        get_env_or_default("DETECTOR_INTERVAL", std::to_string(interval_default)),
        interval_default
    );
    const long long safe_interval = std::max(1LL, interval);

    while (true) {
        const auto start = std::chrono::steady_clock::now();
        run_detector_once();
        const auto end = std::chrono::steady_clock::now();
        const auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(end - start).count();
        const long long sleep_for = safe_interval - (elapsed % safe_interval);
        if (sleep_for > 0) {
            std::this_thread::sleep_for(std::chrono::seconds(sleep_for));
        }
    }
}
