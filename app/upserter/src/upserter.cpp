#include <algorithm>
#include <array>
#include <chrono>
#include <cctype>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <deque>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <random>
#include <sstream>
#include <string>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <thread>
#include <unordered_set>
#include <unistd.h>
#include <vector>

#include "../../../third_party/nlohmann/json.hpp"

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace {

void ensure_parent_directory(const fs::path& path);

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

std::string read_text_file(const fs::path& path) {
    std::ifstream in(path);
    if (!in) {
        return "";
    }
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

std::string now_utc_iso8601() {
    std::time_t now = std::time(nullptr);
    std::tm tm{};
    gmtime_r(&now, &tm);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm);
    return std::string(buf);
}

void log_line(const fs::path& log_file, const std::string& message) {
    ensure_parent_directory(log_file);
    std::ofstream out(log_file, std::ios::out | std::ios::app);
    if (out) {
        out << message << "\n";
    }
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
    lines.reserve(keep_last_lines + 256);

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

bool is_digits_only(const std::string& value) {
    if (value.empty()) {
        return false;
    }
    return std::all_of(value.begin(), value.end(), [](unsigned char c) {
        return std::isdigit(c) != 0;
    });
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

std::optional<QueueRecord> pop_random_queue_record(
    const fs::path& queue_path,
    const fs::path& queue_lock,
    std::mt19937_64& rng,
    const std::string& default_source
) {
    FileLock lock(queue_lock);

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

    std::uniform_int_distribution<std::size_t> dist(0, records.size() - 1);
    const std::size_t selected_index = dist(rng);
    const QueueRecord selected_record = records[selected_index];

    for (std::size_t i = 0; i < records.size(); ++i) {
        if (i == selected_index) {
            continue;
        }
        out << serialize_queue_record(records[i]) << "\n";
    }

    return selected_record;
}

void append_queue_record(
    const fs::path& queue_path,
    const fs::path& queue_lock,
    QueueRecord record
) {
    if (!is_digits_only(record.user_id)) {
        return;
    }
    if (record.enqueued_at_utc.empty()) {
        record.enqueued_at_utc = now_utc_iso8601();
    }
    if (record.source.empty()) {
        record.source = "process";
    }

    FileLock lock(queue_lock);
    std::ofstream out(queue_path, std::ios::out | std::ios::app);
    if (out) {
        out << serialize_queue_record(record) << "\n";
    }
}

std::optional<long long> extract_campus_id(const json& user_json) {
    if (!user_json.is_object()) {
        return std::nullopt;
    }

    if (user_json.contains("campus_users") && user_json["campus_users"].is_array() && !user_json["campus_users"].empty()) {
        std::optional<long long> best_primary_campus_id;
        std::optional<std::string> best_primary_updated_at;

        for (const auto& cu : user_json["campus_users"]) {
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
        for (const auto& cu : user_json["campus_users"]) {
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

    if (user_json.contains("campus") && user_json["campus"].is_array() && !user_json["campus"].empty()) {
        const auto& first = user_json["campus"][0];
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

std::optional<fs::path> find_snapshot_file(
    const fs::path& exports_users,
    const std::string& user_id,
    long long* campus_id_out
) {
    std::error_code ec;
    if (!fs::exists(exports_users, ec)) {
        return std::nullopt;
    }

    std::optional<fs::path> selected_file;
    std::optional<fs::file_time_type> selected_mtime;
    long long selected_campus_id = 0;

    for (const auto& entry : fs::directory_iterator(exports_users, ec)) {
        if (ec || !entry.is_directory()) {
            ec.clear();
            continue;
        }
        const std::string dirname = entry.path().filename().string();
        if (dirname.rfind("campus_", 0) != 0) {
            continue;
        }

        const fs::path candidate = entry.path() / ("user_" + user_id + ".json");
        if (!fs::exists(candidate, ec) || ec) {
            ec.clear();
            continue;
        }

        std::error_code time_ec;
        const auto candidate_mtime = fs::last_write_time(candidate, time_ec);
        if (time_ec) {
            continue;
        }

        const long long candidate_campus_id = parse_int64_or_default(dirname.substr(7), 0);
        const bool should_select = !selected_file.has_value()
            || !selected_mtime.has_value()
            || candidate_mtime > *selected_mtime
            || (candidate_mtime == *selected_mtime && candidate.string() < selected_file->string());

        if (should_select) {
            selected_file = candidate;
            selected_mtime = candidate_mtime;
            selected_campus_id = candidate_campus_id;
        }
    }

    if (!selected_file.has_value()) {
        return std::nullopt;
    }

    if (campus_id_out) {
        *campus_id_out = selected_campus_id;
    }

    return selected_file;
}

std::optional<json> load_json_file(const fs::path& path) {
    std::ifstream in(path);
    if (!in) {
        return std::nullopt;
    }
    try {
        return json::parse(in);
    } catch (...) {
        return std::nullopt;
    }
}

const json* json_object_member(const json& object, const char* key) {
    if (!object.is_object()) {
        return nullptr;
    }
    auto it = object.find(key);
    if (it == object.end()) {
        return nullptr;
    }
    return &(*it);
}

std::string sql_escape(const std::string& text) {
    std::string out;
    out.reserve(text.size() + 8);
    for (char c : text) {
        if (c == '\\') {
            out += "\\\\";
        } else if (c == '\'') {
            out += "''";
        } else {
            out.push_back(c);
        }
    }
    return out;
}

std::string sql_text(const std::string& text) {
    return "E'" + sql_escape(text) + "'";
}

std::string sql_nullable_text(const std::optional<std::string>& value) {
    return value.has_value() ? sql_text(*value) : "NULL";
}

std::string sql_nullable_int(const std::optional<long long>& value) {
    return value.has_value() ? std::to_string(*value) : "NULL";
}

std::string sql_nullable_bool(const std::optional<bool>& value) {
    if (!value.has_value()) {
        return "NULL";
    }
    return *value ? "TRUE" : "FALSE";
}

std::string sql_nullable_timestamptz(const std::optional<std::string>& value) {
    return value.has_value() ? (sql_text(*value) + "::timestamptz") : "NULL";
}

std::string sql_nullable_jsonb(const std::optional<std::string>& value) {
    return value.has_value() ? (sql_text(*value) + "::jsonb") : "NULL";
}

bool run_sql_command(
    const std::string& db_host,
    const std::string& db_port,
    const std::string& db_user,
    const std::string& db_password,
    const std::string& db_name,
    const std::string& sql
) {
    std::string cmd =
        "PGPASSWORD=" + shell_quote(db_password) +
        " psql -w -v ON_ERROR_STOP=1 -h " + shell_quote(db_host) +
        " -p " + shell_quote(db_port) +
        " -U " + shell_quote(db_user) +
        " -d " + shell_quote(db_name) +
        " -c " + shell_quote(sql) +
        " >/dev/null 2>&1";

    int rc = 0;
    (void)run_bash(cmd, &rc);
    return rc == 0;
}

bool ensure_detector_events_table(
    const std::string& db_host,
    const std::string& db_port,
    const std::string& db_user,
    const std::string& db_password,
    const std::string& db_name
) {
    const std::string sql = R"SQL(
CREATE TABLE IF NOT EXISTS detector_events (
  id             BIGSERIAL PRIMARY KEY,
  event_uid      CHAR(32) UNIQUE NOT NULL,
  raw_line       TEXT NOT NULL,
  source         TEXT NOT NULL DEFAULT 'detector',
  ts             BIGINT,
  event_at       TIMESTAMPTZ,
  updated_at     TIMESTAMPTZ,
  user_id        BIGINT,
  user_login     TEXT,
  campus_id      BIGINT,
  first_snapshot BOOLEAN,
  event_types    JSONB NOT NULL DEFAULT '[]'::jsonb,
  changes        JSONB NOT NULL DEFAULT '[]'::jsonb,
  payload        JSONB NOT NULL,
  ingested_at    TIMESTAMPTZ NOT NULL DEFAULT NOW()
);
CREATE INDEX IF NOT EXISTS idx_detector_events_event_at ON detector_events (event_at DESC);
CREATE INDEX IF NOT EXISTS idx_detector_events_user_id ON detector_events (user_id);
CREATE INDEX IF NOT EXISTS idx_detector_events_campus_id ON detector_events (campus_id);
CREATE INDEX IF NOT EXISTS idx_detector_events_ingested_at ON detector_events (ingested_at DESC);
CREATE INDEX IF NOT EXISTS idx_detector_events_event_types_gin ON detector_events USING GIN (event_types);
)SQL";

    return run_sql_command(db_host, db_port, db_user, db_password, db_name, sql);
}

struct DetectorEventRow {
    std::string raw_line;
    json payload;
    std::string source{"detector"};
    std::optional<long long> ts;
    std::optional<std::string> updated_at;
    std::optional<long long> user_id;
    std::optional<std::string> user_login;
    std::optional<long long> campus_id;
    std::optional<bool> first_snapshot;
    json event_types{json::array()};
    json changes{json::array()};
};

std::optional<long long> json_to_optional_int(const json* value) {
    if (!value) {
        return std::nullopt;
    }
    try {
        if (value->is_number_integer()) {
            return value->get<long long>();
        }
        if (value->is_number_unsigned()) {
            return static_cast<long long>(value->get<unsigned long long>());
        }
        if (value->is_string()) {
            return parse_int64(value->get<std::string>());
        }
    } catch (...) {
        return std::nullopt;
    }
    return std::nullopt;
}

std::optional<std::string> json_to_optional_string(const json* value) {
    if (!value) {
        return std::nullopt;
    }
    try {
        if (value->is_string()) {
            std::string out = trim(value->get<std::string>());
            if (!out.empty()) {
                return out;
            }
            return std::nullopt;
        }
        if (value->is_number_integer()) {
            return std::to_string(value->get<long long>());
        }
    } catch (...) {
        return std::nullopt;
    }
    return std::nullopt;
}

std::optional<bool> json_to_optional_bool(const json* value) {
    if (!value || !value->is_boolean()) {
        return std::nullopt;
    }
    try {
        return value->get<bool>();
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<DetectorEventRow> parse_detector_event_line(const std::string& line) {
    if (line.empty()) {
        return std::nullopt;
    }

    json parsed;
    try {
        parsed = json::parse(line);
    } catch (...) {
        return std::nullopt;
    }
    if (!parsed.is_object()) {
        return std::nullopt;
    }

    DetectorEventRow out;
    out.raw_line = line;
    out.payload = parsed;
    out.source = json_to_optional_string(json_object_member(parsed, "source")).value_or("detector");
    out.ts = json_to_optional_int(json_object_member(parsed, "ts"));
    out.updated_at = json_to_optional_string(json_object_member(parsed, "updated_at"));
    out.user_id = json_to_optional_int(json_object_member(parsed, "user_id"));
    out.user_login = json_to_optional_string(json_object_member(parsed, "user_login"));
    out.campus_id = json_to_optional_int(json_object_member(parsed, "campus_id"));
    out.first_snapshot = json_to_optional_bool(json_object_member(parsed, "first_snapshot"));

    if (const json* types = json_object_member(parsed, "types"); types && types->is_array()) {
        out.event_types = *types;
    } else {
        out.event_types = json::array();
    }

    if (const json* changes = json_object_member(parsed, "changes"); changes && changes->is_array()) {
        out.changes = *changes;
    } else {
        out.changes = json::array();
    }

    return out;
}

std::vector<std::string> read_events_lines_with_mutex(
    const fs::path& events_queue,
    const fs::path& events_mutex
) {
    std::string content;
    {
        ExclusiveCreateLock lock(events_mutex);
        content = read_text_file(events_queue);
    }

    std::vector<std::string> lines;
    if (content.empty()) {
        return lines;
    }

    std::istringstream iss(content);
    std::string line;
    while (std::getline(iss, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (!trim(line).empty()) {
            lines.push_back(line);
        }
    }
    return lines;
}

bool run_detector_events_insert_batch(
    const std::string& db_host,
    const std::string& db_port,
    const std::string& db_user,
    const std::string& db_password,
    const std::string& db_name,
    const std::vector<DetectorEventRow>& rows
) {
    if (rows.empty()) {
        return true;
    }

    std::ostringstream sql;
    sql
        << "INSERT INTO detector_events "
        << "(event_uid, raw_line, source, ts, event_at, updated_at, user_id, user_login, campus_id, first_snapshot, event_types, changes, payload) "
        << "VALUES ";

    for (std::size_t i = 0; i < rows.size(); ++i) {
        const DetectorEventRow& row = rows[i];
        const std::string ts_value = row.ts.has_value() ? std::to_string(*row.ts) : "NULL";
        const std::string event_at_value = row.ts.has_value() ? ("to_timestamp(" + std::to_string(*row.ts) + ")") : "NULL";
        const std::string updated_at_value = row.updated_at.has_value() ? (sql_text(*row.updated_at) + "::timestamptz") : "NULL";
        const std::string user_id_value = row.user_id.has_value() ? std::to_string(*row.user_id) : "NULL";
        const std::string user_login_value = row.user_login.has_value() ? sql_text(*row.user_login) : "NULL";
        const std::string campus_id_value = row.campus_id.has_value() ? std::to_string(*row.campus_id) : "NULL";
        const std::string first_snapshot_value =
            row.first_snapshot.has_value() ? (*row.first_snapshot ? "TRUE" : "FALSE") : "NULL";

        sql
            << "(md5(" << sql_text(row.raw_line) << "), "
            << sql_text(row.raw_line) << ", "
            << sql_text(row.source) << ", "
            << ts_value << ", "
            << event_at_value << ", "
            << updated_at_value << ", "
            << user_id_value << ", "
            << user_login_value << ", "
            << campus_id_value << ", "
            << first_snapshot_value << ", "
            << sql_text(row.event_types.dump()) << "::jsonb, "
            << sql_text(row.changes.dump()) << "::jsonb, "
            << sql_text(row.payload.dump()) << "::jsonb)";

        if (i + 1 < rows.size()) {
            sql << ", ";
        }
    }
    sql << " ON CONFLICT (event_uid) DO NOTHING;";

    return run_sql_command(db_host, db_port, db_user, db_password, db_name, sql.str());
}

void remember_recent_event_line(
    const std::string& line,
    std::deque<std::string>& order,
    std::unordered_set<std::string>& set,
    std::size_t max_size
) {
    if (line.empty() || set.find(line) != set.end()) {
        return;
    }
    order.push_back(line);
    set.insert(line);

    while (order.size() > max_size) {
        set.erase(order.front());
        order.pop_front();
    }
}

struct DetectorEventSyncStats {
    std::size_t scanned_lines{0};
    std::size_t candidates{0};
    std::size_t persisted_candidates{0};
    std::size_t parse_errors{0};
    std::size_t db_row_failures{0};
    bool lock_failed{false};
    bool db_failed{false};
};

DetectorEventSyncStats sync_detector_events_once(
    const std::string& db_host,
    const std::string& db_port,
    const std::string& db_user,
    const std::string& db_password,
    const std::string& db_name,
    const fs::path& events_queue,
    const fs::path& events_mutex,
    std::deque<std::string>& recent_lines_order,
    std::unordered_set<std::string>& recent_lines_set,
    std::size_t recent_lines_max,
    std::size_t max_candidates_per_sync,
    std::size_t max_batch_size
) {
    DetectorEventSyncStats stats;
    std::vector<std::string> lines;
    try {
        lines = read_events_lines_with_mutex(events_queue, events_mutex);
    } catch (...) {
        stats.lock_failed = true;
        return stats;
    }

    stats.scanned_lines = lines.size();
    if (lines.empty()) {
        return stats;
    }

    std::vector<DetectorEventRow> candidates;
    candidates.reserve(std::min(lines.size(), max_candidates_per_sync));

    for (const auto& line : lines) {
        if (recent_lines_set.find(line) != recent_lines_set.end()) {
            continue;
        }

        auto parsed = parse_detector_event_line(line);
        if (!parsed.has_value()) {
            stats.parse_errors += 1;
            continue;
        }
        candidates.push_back(std::move(*parsed));
        if (candidates.size() >= max_candidates_per_sync) {
            break;
        }
    }

    stats.candidates = candidates.size();
    if (candidates.empty()) {
        return stats;
    }

    for (std::size_t offset = 0; offset < candidates.size(); offset += max_batch_size) {
        const std::size_t end = std::min(offset + max_batch_size, candidates.size());
        std::vector<DetectorEventRow> batch;
        batch.reserve(end - offset);
        for (std::size_t i = offset; i < end; ++i) {
            batch.push_back(candidates[i]);
        }

        if (!run_detector_events_insert_batch(db_host, db_port, db_user, db_password, db_name, batch)) {
            // Fallback to row-by-row upsert so one malformed row cannot block all canonical persistence.
            for (const auto& row : batch) {
                std::vector<DetectorEventRow> single{row};
                if (!run_detector_events_insert_batch(db_host, db_port, db_user, db_password, db_name, single)) {
                    stats.db_failed = true;
                    stats.db_row_failures += 1;
                    continue;
                }
                remember_recent_event_line(row.raw_line, recent_lines_order, recent_lines_set, recent_lines_max);
                stats.persisted_candidates += 1;
            }
            continue;
        }

        for (const auto& row : batch) {
            remember_recent_event_line(row.raw_line, recent_lines_order, recent_lines_set, recent_lines_max);
        }
        stats.persisted_candidates += batch.size();
    }

    return stats;
}

bool ensure_project_users_columns(
    const std::string& db_host,
    const std::string& db_port,
    const std::string& db_user,
    const std::string& db_password,
    const std::string& db_name
) {
    const std::string sql = R"SQL(
ALTER TABLE project_users
  ADD COLUMN IF NOT EXISTS cursus_ids JSONB,
  ADD COLUMN IF NOT EXISTS current_team_id BIGINT,
  ADD COLUMN IF NOT EXISTS marked BOOLEAN,
  ADD COLUMN IF NOT EXISTS marked_at TIMESTAMPTZ,
  ADD COLUMN IF NOT EXISTS retriable_at TIMESTAMPTZ,
  ADD COLUMN IF NOT EXISTS occurrence INTEGER;
CREATE INDEX IF NOT EXISTS idx_project_users_marked ON project_users (marked);
CREATE INDEX IF NOT EXISTS idx_project_users_marked_at ON project_users (marked_at DESC);
)SQL";
    return run_sql_command(db_host, db_port, db_user, db_password, db_name, sql);
}

bool contains_cursus_id(const json* cursus_ids, long long expected_id) {
    if (!cursus_ids || !cursus_ids->is_array()) {
        return false;
    }
    for (const auto& entry : *cursus_ids) {
        if (entry.is_number_integer() && entry.get<long long>() == expected_id) {
            return true;
        }
        if (entry.is_number_unsigned() && static_cast<long long>(entry.get<unsigned long long>()) == expected_id) {
            return true;
        }
        if (entry.is_string()) {
            auto parsed = parse_int64(entry.get<std::string>());
            if (parsed.has_value() && *parsed == expected_id) {
                return true;
            }
        }
    }
    return false;
}

struct ProjectUserRow {
    long long id{0};
    long long project_id{0};
    long long campus_id{0};
    long long user_id{0};
    std::optional<std::string> user_login;
    std::optional<std::string> user_email;
    std::optional<long long> final_mark;
    std::optional<std::string> status;
    std::optional<bool> validated;
    std::optional<std::string> created_at;
    std::optional<std::string> updated_at;
    std::string cursus_ids_json{"[21]"};
    std::optional<long long> current_team_id;
    std::optional<bool> marked;
    std::optional<std::string> marked_at;
    std::optional<std::string> retriable_at;
    std::optional<long long> occurrence;
};

std::vector<ProjectUserRow> extract_project_user_rows_for_cursus_21(
    const json& user_json,
    long long user_id,
    long long campus_id
) {
    std::vector<ProjectUserRow> rows;
    const json* projects_users = json_object_member(user_json, "projects_users");
    if (!projects_users || !projects_users->is_array()) {
        return rows;
    }

    const auto user_login = json_to_optional_string(json_object_member(user_json, "login"));
    const auto user_email = json_to_optional_string(json_object_member(user_json, "email"));
    rows.reserve(projects_users->size());

    for (const auto& item : *projects_users) {
        if (!item.is_object()) {
            continue;
        }
        if (!contains_cursus_id(json_object_member(item, "cursus_ids"), 21)) {
            continue;
        }

        const auto rel_id = json_to_optional_int(json_object_member(item, "id"));
        if (!rel_id.has_value() || *rel_id <= 0) {
            continue;
        }

        const json* project = json_object_member(item, "project");
        if (!project || !project->is_object()) {
            continue;
        }
        const auto project_id = json_to_optional_int(json_object_member(*project, "id"));
        if (!project_id.has_value() || *project_id <= 0) {
            continue;
        }

        ProjectUserRow row;
        row.id = *rel_id;
        row.project_id = *project_id;
        row.campus_id = campus_id;
        row.user_id = user_id;
        row.user_login = user_login;
        row.user_email = user_email;
        row.final_mark = json_to_optional_int(json_object_member(item, "final_mark"));
        row.status = json_to_optional_string(json_object_member(item, "status"));
        row.validated = json_to_optional_bool(json_object_member(item, "validated?"));
        if (!row.validated.has_value()) {
            row.validated = json_to_optional_bool(json_object_member(item, "validated"));
        }
        row.created_at = json_to_optional_string(json_object_member(item, "created_at"));
        row.updated_at = json_to_optional_string(json_object_member(item, "updated_at"));
        row.current_team_id = json_to_optional_int(json_object_member(item, "current_team_id"));
        row.marked = json_to_optional_bool(json_object_member(item, "marked"));
        row.marked_at = json_to_optional_string(json_object_member(item, "marked_at"));
        row.retriable_at = json_to_optional_string(json_object_member(item, "retriable_at"));
        row.occurrence = json_to_optional_int(json_object_member(item, "occurrence"));
        rows.push_back(std::move(row));
    }

    return rows;
}

bool run_project_users_upsert_batch(
    const std::string& db_host,
    const std::string& db_port,
    const std::string& db_user,
    const std::string& db_password,
    const std::string& db_name,
    const std::vector<ProjectUserRow>& rows
) {
    if (rows.empty()) {
        return true;
    }

    std::ostringstream sql;
    sql
        << "INSERT INTO project_users ("
        << "id, project_id, campus_id, user_id, user_login, user_email, final_mark, status, validated, "
        << "created_at, updated_at, cursus_ids, current_team_id, marked, marked_at, retriable_at, occurrence"
        << ") VALUES ";

    bool first = true;
    for (const auto& row : rows) {
        if (!first) {
            sql << ", ";
        }
        first = false;

        sql << "("
            << row.id << ", "
            << row.project_id << ", "
            << row.campus_id << ", "
            << row.user_id << ", "
            << sql_nullable_text(row.user_login) << ", "
            << sql_nullable_text(row.user_email) << ", "
            << sql_nullable_int(row.final_mark) << ", "
            << sql_nullable_text(row.status) << ", "
            << sql_nullable_bool(row.validated) << ", "
            << sql_nullable_timestamptz(row.created_at) << ", "
            << sql_nullable_timestamptz(row.updated_at) << ", "
            << sql_text(row.cursus_ids_json) << "::jsonb, "
            << sql_nullable_int(row.current_team_id) << ", "
            << sql_nullable_bool(row.marked) << ", "
            << sql_nullable_timestamptz(row.marked_at) << ", "
            << sql_nullable_timestamptz(row.retriable_at) << ", "
            << sql_nullable_int(row.occurrence)
            << ")";
    }

    sql
        << " ON CONFLICT (id) DO UPDATE SET "
        << "project_id=EXCLUDED.project_id, "
        << "campus_id=EXCLUDED.campus_id, "
        << "user_id=EXCLUDED.user_id, "
        << "user_login=EXCLUDED.user_login, "
        << "user_email=EXCLUDED.user_email, "
        << "final_mark=EXCLUDED.final_mark, "
        << "status=EXCLUDED.status, "
        << "validated=EXCLUDED.validated, "
        << "created_at=EXCLUDED.created_at, "
        << "updated_at=EXCLUDED.updated_at, "
        << "cursus_ids=EXCLUDED.cursus_ids, "
        << "current_team_id=EXCLUDED.current_team_id, "
        << "marked=EXCLUDED.marked, "
        << "marked_at=EXCLUDED.marked_at, "
        << "retriable_at=EXCLUDED.retriable_at, "
        << "occurrence=EXCLUDED.occurrence, "
        << "ingested_at=NOW();";

    return run_sql_command(db_host, db_port, db_user, db_password, db_name, sql.str());
}

bool sync_project_users_for_user(
    const std::string& db_host,
    const std::string& db_port,
    const std::string& db_user,
    const std::string& db_password,
    const std::string& db_name,
    const json& user_json,
    long long user_id,
    long long campus_id
) {
    const auto rows = extract_project_user_rows_for_cursus_21(user_json, user_id, campus_id);
    if (rows.empty()) {
        return true;
    }

    constexpr std::size_t kBatchSize = 200;
    for (std::size_t offset = 0; offset < rows.size(); offset += kBatchSize) {
        const std::size_t end = std::min(offset + kBatchSize, rows.size());
        std::vector<ProjectUserRow> batch;
        batch.reserve(end - offset);
        for (std::size_t i = offset; i < end; ++i) {
            batch.push_back(rows[i]);
        }
        if (!run_project_users_upsert_batch(db_host, db_port, db_user, db_password, db_name, batch)) {
            return false;
        }
    }
    return true;
}

bool ensure_achievements_users_columns_and_keys(
    const std::string& db_host,
    const std::string& db_port,
    const std::string& db_user,
    const std::string& db_password,
    const std::string& db_name
) {
    const std::string sql = R"SQL(
ALTER TABLE achievements_users
  ADD COLUMN IF NOT EXISTS name TEXT,
  ADD COLUMN IF NOT EXISTS kind TEXT,
  ADD COLUMN IF NOT EXISTS description TEXT,
  ADD COLUMN IF NOT EXISTS image TEXT,
  ADD COLUMN IF NOT EXISTS tier TEXT,
  ADD COLUMN IF NOT EXISTS nbr_of_success INTEGER,
  ADD COLUMN IF NOT EXISTS visible BOOLEAN,
  ADD COLUMN IF NOT EXISTS users_url TEXT;

ALTER TABLE achievements_users DROP CONSTRAINT IF EXISTS achievements_users_pkey;
ALTER TABLE achievements_users DROP COLUMN IF EXISTS id;

-- Defensive dedup in case legacy rows already exist.
DELETE FROM achievements_users a
USING achievements_users b
WHERE a.ctid < b.ctid
  AND a.user_id = b.user_id
  AND a.achievement_id = b.achievement_id;

ALTER TABLE achievements_users
  ADD CONSTRAINT achievements_users_pkey PRIMARY KEY (user_id, achievement_id);

CREATE INDEX IF NOT EXISTS idx_achievements_users_achievement_id ON achievements_users (achievement_id);
CREATE INDEX IF NOT EXISTS idx_achievements_users_user_id ON achievements_users (user_id);
CREATE INDEX IF NOT EXISTS idx_achievements_users_campus_id ON achievements_users (campus_id);
CREATE INDEX IF NOT EXISTS idx_achievements_users_kind ON achievements_users (kind);
)SQL";
    return run_sql_command(db_host, db_port, db_user, db_password, db_name, sql);
}

struct AchievementUserRow {
    long long achievement_id{0};
    long long campus_id{0};
    long long user_id{0};
    std::optional<std::string> user_login;
    std::optional<std::string> user_email;
    std::optional<std::string> name;
    std::optional<std::string> kind;
    std::optional<std::string> description;
    std::optional<std::string> image;
    std::optional<std::string> tier;
    std::optional<long long> nbr_of_success;
    std::optional<bool> visible;
    std::optional<std::string> users_url;
};

std::vector<AchievementUserRow> extract_achievement_user_rows(
    const json& user_json,
    long long user_id,
    long long campus_id
) {
    std::vector<AchievementUserRow> rows;
    const json* achievements = json_object_member(user_json, "achievements");
    if (!achievements || !achievements->is_array()) {
        return rows;
    }

    const auto user_login = json_to_optional_string(json_object_member(user_json, "login"));
    const auto user_email = json_to_optional_string(json_object_member(user_json, "email"));
    rows.reserve(achievements->size());

    for (const auto& item : *achievements) {
        if (!item.is_object()) {
            continue;
        }
        const auto achievement_id = json_to_optional_int(json_object_member(item, "id"));
        if (!achievement_id.has_value() || *achievement_id <= 0) {
            continue;
        }

        AchievementUserRow row;
        row.achievement_id = *achievement_id;
        row.campus_id = campus_id;
        row.user_id = user_id;
        row.user_login = user_login;
        row.user_email = user_email;
        row.name = json_to_optional_string(json_object_member(item, "name"));
        row.kind = json_to_optional_string(json_object_member(item, "kind"));
        row.description = json_to_optional_string(json_object_member(item, "description"));
        row.image = json_to_optional_string(json_object_member(item, "image"));
        row.tier = json_to_optional_string(json_object_member(item, "tier"));
        row.nbr_of_success = json_to_optional_int(json_object_member(item, "nbr_of_success"));
        row.visible = json_to_optional_bool(json_object_member(item, "visible"));
        row.users_url = json_to_optional_string(json_object_member(item, "users_url"));
        rows.push_back(std::move(row));
    }

    return rows;
}

bool run_achievements_users_upsert_batch(
    const std::string& db_host,
    const std::string& db_port,
    const std::string& db_user,
    const std::string& db_password,
    const std::string& db_name,
    const std::vector<AchievementUserRow>& rows
) {
    if (rows.empty()) {
        return true;
    }

    std::ostringstream sql;
    sql
        << "INSERT INTO achievements_users ("
        << "achievement_id, campus_id, user_id, user_login, user_email, "
        << "name, kind, description, image, tier, nbr_of_success, visible, users_url, "
        << "created_at, updated_at"
        << ") VALUES ";

    bool first = true;
    for (const auto& row : rows) {
        if (!first) {
            sql << ", ";
        }
        first = false;
        sql << "("
            << row.achievement_id << ", "
            << row.campus_id << ", "
            << row.user_id << ", "
            << sql_nullable_text(row.user_login) << ", "
            << sql_nullable_text(row.user_email) << ", "
            << sql_nullable_text(row.name) << ", "
            << sql_nullable_text(row.kind) << ", "
            << sql_nullable_text(row.description) << ", "
            << sql_nullable_text(row.image) << ", "
            << sql_nullable_text(row.tier) << ", "
            << sql_nullable_int(row.nbr_of_success) << ", "
            << sql_nullable_bool(row.visible) << ", "
            << sql_nullable_text(row.users_url) << ", "
            << "NULL, NULL"
            << ")";
    }

    sql
        << " ON CONFLICT (user_id, achievement_id) DO UPDATE SET "
        << "campus_id=EXCLUDED.campus_id, "
        << "user_login=EXCLUDED.user_login, "
        << "user_email=EXCLUDED.user_email, "
        << "name=EXCLUDED.name, "
        << "kind=EXCLUDED.kind, "
        << "description=EXCLUDED.description, "
        << "image=EXCLUDED.image, "
        << "tier=EXCLUDED.tier, "
        << "nbr_of_success=EXCLUDED.nbr_of_success, "
        << "visible=EXCLUDED.visible, "
        << "users_url=EXCLUDED.users_url, "
        << "ingested_at=NOW();";

    return run_sql_command(db_host, db_port, db_user, db_password, db_name, sql.str());
}

bool sync_achievements_users_for_user(
    const std::string& db_host,
    const std::string& db_port,
    const std::string& db_user,
    const std::string& db_password,
    const std::string& db_name,
    const json& user_json,
    long long user_id,
    long long campus_id
) {
    const auto rows = extract_achievement_user_rows(user_json, user_id, campus_id);
    if (rows.empty()) {
        return true;
    }

    constexpr std::size_t kBatchSize = 200;
    for (std::size_t offset = 0; offset < rows.size(); offset += kBatchSize) {
        const std::size_t end = std::min(offset + kBatchSize, rows.size());
        std::vector<AchievementUserRow> batch;
        batch.reserve(end - offset);
        for (std::size_t i = offset; i < end; ++i) {
            batch.push_back(rows[i]);
        }
        if (!run_achievements_users_upsert_batch(db_host, db_port, db_user, db_password, db_name, batch)) {
            return false;
        }
    }
    return true;
}

bool run_upsert(
    const std::string& db_host,
    const std::string& db_port,
    const std::string& db_user,
    const std::string& db_password,
    const std::string& db_name,
    long long user_id,
    const json& user_json,
    long long campus_id
) {
    const auto email = json_to_optional_string(json_object_member(user_json, "email"));
    const auto login = json_to_optional_string(json_object_member(user_json, "login"));
    const auto first_name = json_to_optional_string(json_object_member(user_json, "first_name"));
    const auto last_name = json_to_optional_string(json_object_member(user_json, "last_name"));
    const auto usual_full_name = json_to_optional_string(json_object_member(user_json, "usual_full_name"));
    const auto usual_first_name = json_to_optional_string(json_object_member(user_json, "usual_first_name"));
    const auto url = json_to_optional_string(json_object_member(user_json, "url"));
    const auto phone = json_to_optional_string(json_object_member(user_json, "phone"));
    const auto displayname = json_to_optional_string(json_object_member(user_json, "displayname"));
    const auto kind = json_to_optional_string(json_object_member(user_json, "kind"));
    const auto correction_point = json_to_optional_int(json_object_member(user_json, "correction_point"));
    const auto pool_month = json_to_optional_string(json_object_member(user_json, "pool_month"));
    const auto pool_year = json_to_optional_string(json_object_member(user_json, "pool_year"));
    const auto location = json_to_optional_string(json_object_member(user_json, "location"));
    const auto wallet = json_to_optional_int(json_object_member(user_json, "wallet"));
    const auto anonymize_date = json_to_optional_string(json_object_member(user_json, "anonymize_date"));
    const auto data_erasure_date = json_to_optional_string(json_object_member(user_json, "data_erasure_date"));
    const auto created_at = json_to_optional_string(json_object_member(user_json, "created_at"));
    const auto updated_at = json_to_optional_string(json_object_member(user_json, "updated_at"));
    const auto alumnized_at = json_to_optional_string(json_object_member(user_json, "alumnized_at"));

    auto staff = json_to_optional_bool(json_object_member(user_json, "staff?"));
    if (!staff.has_value()) {
        staff = json_to_optional_bool(json_object_member(user_json, "staff"));
    }
    auto alumni = json_to_optional_bool(json_object_member(user_json, "alumni?"));
    if (!alumni.has_value()) {
        alumni = json_to_optional_bool(json_object_member(user_json, "alumni"));
    }
    auto active = json_to_optional_bool(json_object_member(user_json, "active?"));
    if (!active.has_value()) {
        active = json_to_optional_bool(json_object_member(user_json, "active"));
    }

    std::optional<std::string> image_link;
    std::optional<std::string> image_large;
    std::optional<std::string> image_medium;
    std::optional<std::string> image_small;
    std::optional<std::string> image_micro;
    std::optional<std::string> image_raw;
    if (const json* image = json_object_member(user_json, "image"); image && !image->is_null()) {
        image_raw = image->dump();
        if (image->is_object()) {
            image_link = json_to_optional_string(json_object_member(*image, "link"));
            if (const json* versions = json_object_member(*image, "versions"); versions && versions->is_object()) {
                image_large = json_to_optional_string(json_object_member(*versions, "large"));
                image_medium = json_to_optional_string(json_object_member(*versions, "medium"));
                image_small = json_to_optional_string(json_object_member(*versions, "small"));
                image_micro = json_to_optional_string(json_object_member(*versions, "micro"));
            }
        }
    }

    std::ostringstream sql;
    sql
        << "INSERT INTO users ("
        << "id, email, login, first_name, last_name, usual_full_name, usual_first_name, url, phone, displayname, kind, "
        << "image_link, image_large, image_medium, image_small, image_micro, image, staff, correction_point, pool_month, pool_year, "
        << "location, wallet, anonymize_date, data_erasure_date, created_at, updated_at, alumnized_at, alumni, active, campus_id"
        << ") "
        << "VALUES (" << user_id << ", "
        << sql_nullable_text(email) << ", "
        << sql_nullable_text(login) << ", "
        << sql_nullable_text(first_name) << ", "
        << sql_nullable_text(last_name) << ", "
        << sql_nullable_text(usual_full_name) << ", "
        << sql_nullable_text(usual_first_name) << ", "
        << sql_nullable_text(url) << ", "
        << sql_nullable_text(phone) << ", "
        << sql_nullable_text(displayname) << ", "
        << sql_nullable_text(kind) << ", "
        << sql_nullable_text(image_link) << ", "
        << sql_nullable_text(image_large) << ", "
        << sql_nullable_text(image_medium) << ", "
        << sql_nullable_text(image_small) << ", "
        << sql_nullable_text(image_micro) << ", "
        << sql_nullable_jsonb(image_raw) << ", "
        << sql_nullable_bool(staff) << ", "
        << sql_nullable_int(correction_point) << ", "
        << sql_nullable_text(pool_month) << ", "
        << sql_nullable_text(pool_year) << ", "
        << sql_nullable_text(location) << ", "
        << sql_nullable_int(wallet) << ", "
        << sql_nullable_timestamptz(anonymize_date) << ", "
        << sql_nullable_timestamptz(data_erasure_date) << ", "
        << sql_nullable_timestamptz(created_at) << ", "
        << sql_nullable_timestamptz(updated_at) << ", "
        << sql_nullable_timestamptz(alumnized_at) << ", "
        << sql_nullable_bool(alumni) << ", "
        << sql_nullable_bool(active) << ", "
        << campus_id << ") "
        << "ON CONFLICT (id) DO UPDATE SET "
        << "email=EXCLUDED.email, "
        << "login=EXCLUDED.login, "
        << "first_name=EXCLUDED.first_name, "
        << "last_name=EXCLUDED.last_name, "
        << "usual_full_name=EXCLUDED.usual_full_name, "
        << "usual_first_name=EXCLUDED.usual_first_name, "
        << "url=EXCLUDED.url, "
        << "phone=EXCLUDED.phone, "
        << "displayname=EXCLUDED.displayname, "
        << "kind=EXCLUDED.kind, "
        << "image_link=EXCLUDED.image_link, "
        << "image_large=EXCLUDED.image_large, "
        << "image_medium=EXCLUDED.image_medium, "
        << "image_small=EXCLUDED.image_small, "
        << "image_micro=EXCLUDED.image_micro, "
        << "image=EXCLUDED.image, "
        << "staff=EXCLUDED.staff, "
        << "correction_point=EXCLUDED.correction_point, "
        << "pool_month=EXCLUDED.pool_month, "
        << "pool_year=EXCLUDED.pool_year, "
        << "wallet=EXCLUDED.wallet, "
        << "anonymize_date=EXCLUDED.anonymize_date, "
        << "data_erasure_date=EXCLUDED.data_erasure_date, "
        << "created_at=EXCLUDED.created_at, "
        << "updated_at=EXCLUDED.updated_at, "
        << "alumnized_at=EXCLUDED.alumnized_at, "
        << "alumni=EXCLUDED.alumni, "
        << "active=EXCLUDED.active, "
        << "location=EXCLUDED.location, "
        << "campus_id=EXCLUDED.campus_id, "
        << "ingested_at=NOW();";

    return run_sql_command(db_host, db_port, db_user, db_password, db_name, sql.str());
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
        fs::path logs_agents_dir = logs_root / "agents";
        fs::path exports_root = runtime_dir / "exports";
        fs::path backlog_dir = runtime_dir / "backlog";

        const fs::path exports_users = exports_root / "09_users";
        const fs::path process_queue = backlog_dir / "process_queue.txt";
        const fs::path process_lock = backlog_dir / "process_queue.txt.lock";
        const fs::path events_queue = backlog_dir / "events_queue.jsonl";
        const fs::path events_mutex = backlog_dir / "events_queue.mutex";

        ensure_directory(backlog_dir);
        ensure_directory(logs_root);
        ensure_directory(logs_agents_dir);
        ensure_directory(exports_users);

        const std::vector<fs::path> config_paths = {
            root_dir / "app" / "upserter" / "config" / "agents.config",
        };

        const std::string db_host_cfg = read_config_value_multi(config_paths, "DB_HOST");
        const std::string db_port_cfg = read_config_value_multi(config_paths, "DB_PORT");
        const std::string db_user_cfg = read_config_value_multi(config_paths, "DB_USER");
        const std::string db_password_cfg = read_config_value_multi(config_paths, "DB_PASSWORD");
        const std::string db_name_cfg = read_config_value_multi(config_paths, "DB_NAME");

        const std::string db_host = get_env_or_default("DB_HOST", db_host_cfg.empty() ? "localhost" : db_host_cfg);
        const std::string db_port = get_env_or_default("DB_PORT", db_port_cfg.empty() ? "5432" : db_port_cfg);
        const std::string db_user = get_env_or_default("DB_USER", db_user_cfg.empty() ? "api42" : db_user_cfg);
        const std::string db_password = get_env_or_default("DB_PASSWORD", db_password_cfg);
        const std::string db_name = get_env_or_default("DB_NAME", db_name_cfg.empty() ? "api42" : db_name_cfg);

        if (db_password.empty()) {
            throw std::runtime_error("DB_PASSWORD must be set via environment or agents.config");
        }

        std::string upserter_mode = trim(get_env_or_default("UPSERTER_MODE", "users"));
        std::transform(
            upserter_mode.begin(),
            upserter_mode.end(),
            upserter_mode.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); }
        );

        if (upserter_mode != "users" && upserter_mode != "events") {
            throw std::runtime_error("UPSERTER_MODE must be users or events");
        }

        if (upserter_mode == "events") {
            const fs::path log_file = logs_agents_dir / "upserter_events.log";
            touch_file(events_queue);

            const long long events_sync_interval_ms = std::max<long long>(
                500,
                parse_int64_or_default(get_env_or_default("EVENTS_DB_SYNC_INTERVAL_MS", "2500"), 2500)
            );
            const std::size_t events_sync_max_candidates = static_cast<std::size_t>(std::max<long long>(
                50,
                parse_int64_or_default(get_env_or_default("EVENTS_DB_SYNC_MAX_CANDIDATES", "800"), 800)
            ));
            const std::size_t events_sync_batch_size = static_cast<std::size_t>(std::max<long long>(
                20,
                parse_int64_or_default(get_env_or_default("EVENTS_DB_SYNC_BATCH_SIZE", "200"), 200)
            ));
            const std::size_t events_recent_cache_size = static_cast<std::size_t>(std::max<long long>(
                1000,
                parse_int64_or_default(get_env_or_default("EVENTS_DB_RECENT_CACHE_SIZE", "20000"), 20000)
            ));

            if (!ensure_detector_events_table(db_host, db_port, db_user, db_password, db_name)) {
                throw std::runtime_error("failed to ensure detector_events table");
            }

            log_line(
                log_file,
                "[" + now_utc_iso8601() + "] Upserter events started"
                " events_sync_interval_ms=" + std::to_string(events_sync_interval_ms) +
                " batch=" + std::to_string(events_sync_batch_size) +
                " max_candidates=" + std::to_string(events_sync_max_candidates)
            );

            long long events_sync_counter = 0;
            std::deque<std::string> recent_event_lines_order;
            std::unordered_set<std::string> recent_event_lines_set;
            auto next_events_sync = std::chrono::steady_clock::now();

            while (true) {
                const auto now = std::chrono::steady_clock::now();
                if (now < next_events_sync) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    continue;
                }

                const auto stats = sync_detector_events_once(
                    db_host,
                    db_port,
                    db_user,
                    db_password,
                    db_name,
                    events_queue,
                    events_mutex,
                    recent_event_lines_order,
                    recent_event_lines_set,
                    events_recent_cache_size,
                    events_sync_max_candidates,
                    events_sync_batch_size
                );
                next_events_sync = now + std::chrono::milliseconds(events_sync_interval_ms);
                events_sync_counter += 1;

                if (stats.lock_failed) {
                    log_line(log_file, "[" + now_utc_iso8601() + "] detector_events sync: lock timeout");
                } else if (stats.db_failed) {
                    log_line(
                        log_file,
                        "[" + now_utc_iso8601() + "] detector_events sync: partial DB failures"
                        " scanned=" + std::to_string(stats.scanned_lines) +
                        " candidates=" + std::to_string(stats.candidates) +
                        " persisted_candidates=" + std::to_string(stats.persisted_candidates) +
                        " db_row_failures=" + std::to_string(stats.db_row_failures) +
                        " parse_errors=" + std::to_string(stats.parse_errors) +
                        " cycle=" + std::to_string(events_sync_counter)
                    );
                } else if (stats.persisted_candidates > 0 || stats.parse_errors > 0) {
                    log_line(
                        log_file,
                        "[" + now_utc_iso8601() + "] detector_events sync: scanned=" + std::to_string(stats.scanned_lines) +
                        " candidates=" + std::to_string(stats.candidates) +
                        " persisted_candidates=" + std::to_string(stats.persisted_candidates) +
                        " parse_errors=" + std::to_string(stats.parse_errors) +
                        " cycle=" + std::to_string(events_sync_counter)
                    );
                }
            }
        }

        const fs::path log_file = logs_agents_dir / "upserter_users.log";
        touch_file(process_queue);
        touch_file(process_lock);
        if (!ensure_project_users_columns(db_host, db_port, db_user, db_password, db_name)) {
            throw std::runtime_error("failed to ensure project_users columns");
        }
        if (!ensure_achievements_users_columns_and_keys(db_host, db_port, db_user, db_password, db_name)) {
            throw std::runtime_error("failed to ensure achievements_users columns/keys");
        }

        log_line(log_file, "[" + now_utc_iso8601() + "] Upserter users started");

        long long upsert_counter = 0;
        std::mt19937_64 rng(std::random_device{}());

        while (true) {
            auto queue_record_opt = pop_random_queue_record(process_queue, process_lock, rng, "process");
            if (!queue_record_opt.has_value()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(350));
                continue;
            }

            const QueueRecord queue_record = *queue_record_opt;
            const std::string user_id_str = queue_record.user_id;
            const auto user_id_num_opt = parse_int64(user_id_str);
            if (!user_id_num_opt.has_value()) {
                continue;
            }
            const long long user_id = *user_id_num_opt;

            long long campus_id = 0;
            auto snapshot_file = find_snapshot_file(exports_users, user_id_str, &campus_id);
            if (!snapshot_file.has_value()) {
                log_line(log_file, "[" + now_utc_iso8601() + "] User " + user_id_str + ": snapshot not found");
                append_queue_record(process_queue, process_lock, queue_record);
                log_line(log_file, "[" + now_utc_iso8601() + "] User " + user_id_str + ": requeued (snapshot_missing)");
                std::this_thread::sleep_for(std::chrono::milliseconds(750));
                continue;
            }

            auto user_json_opt = load_json_file(*snapshot_file);
            if (!user_json_opt.has_value() || !user_json_opt->is_object()) {
                log_line(log_file, "[" + now_utc_iso8601() + "] User " + user_id_str + ": invalid snapshot JSON");
                append_queue_record(process_queue, process_lock, queue_record);
                log_line(log_file, "[" + now_utc_iso8601() + "] User " + user_id_str + ": requeued (snapshot_invalid)");
                std::this_thread::sleep_for(std::chrono::milliseconds(750));
                continue;
            }

            const json& user_json = *user_json_opt;

            if (auto parsed_campus = extract_campus_id(user_json); parsed_campus.has_value()) {
                campus_id = *parsed_campus;
            }

            if (campus_id <= 0) {
                log_line(log_file, "[" + now_utc_iso8601() + "] User " + user_id_str + ": invalid campus_id");
                append_queue_record(process_queue, process_lock, queue_record);
                log_line(log_file, "[" + now_utc_iso8601() + "] User " + user_id_str + ": requeued (campus_id_invalid)");
                std::this_thread::sleep_for(std::chrono::milliseconds(750));
                continue;
            }

            const bool upsert_ok = run_upsert(
                db_host,
                db_port,
                db_user,
                db_password,
                db_name,
                user_id,
                user_json,
                campus_id
            );

            if (!upsert_ok) {
                log_line(log_file, "[" + now_utc_iso8601() + "] User " + user_id_str + ": DB upsert failed");
                append_queue_record(process_queue, process_lock, queue_record);
                log_line(log_file, "[" + now_utc_iso8601() + "] User " + user_id_str + ": requeued (db_upsert_failed)");
                std::this_thread::sleep_for(std::chrono::milliseconds(750));
                continue;
            }

            const bool projects_ok = sync_project_users_for_user(
                db_host,
                db_port,
                db_user,
                db_password,
                db_name,
                user_json,
                user_id,
                campus_id
            );
            if (!projects_ok) {
                log_line(log_file, "[" + now_utc_iso8601() + "] User " + user_id_str + ": project_users upsert failed");
                append_queue_record(process_queue, process_lock, queue_record);
                log_line(log_file, "[" + now_utc_iso8601() + "] User " + user_id_str + ": requeued (project_users_upsert_failed)");
                std::this_thread::sleep_for(std::chrono::milliseconds(750));
                continue;
            }

            const bool achievements_ok = sync_achievements_users_for_user(
                db_host,
                db_port,
                db_user,
                db_password,
                db_name,
                user_json,
                user_id,
                campus_id
            );
            if (!achievements_ok) {
                log_line(log_file, "[" + now_utc_iso8601() + "] User " + user_id_str + ": achievements_users upsert failed");
                append_queue_record(process_queue, process_lock, queue_record);
                log_line(log_file, "[" + now_utc_iso8601() + "] User " + user_id_str + ": requeued (achievements_users_upsert_failed)");
                std::this_thread::sleep_for(std::chrono::milliseconds(750));
                continue;
            }

            upsert_counter += 1;
            log_line(log_file, "[" + now_utc_iso8601() + "] Upserted user " + user_id_str + " (total: " + std::to_string(upsert_counter) + ")");

            if ((upsert_counter % 50) == 0) {
                if (count_lines(log_file) > 5500) {
                    trim_log_file(log_file, 5000);
                }
            }
        }
    } catch (const std::exception& ex) {
        std::cerr << "upserter.cpp error: " << ex.what() << "\n";
        return 1;
    }
}
