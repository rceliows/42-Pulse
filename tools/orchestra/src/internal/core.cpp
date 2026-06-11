#include "orchestra_internal.hpp"
#include <mutex>

namespace orchestra {

std::string trim(const std::string& value) {
    const auto first = value.find_first_not_of(" \t\n\r");
    if (first == std::string::npos) {
        return "";
    }
    const auto last = value.find_last_not_of(" \t\n\r");
    return value.substr(first, last - first + 1);
}

bool is_valid_env_key(const std::string& key) {
    if (key.empty()) {
        return false;
    }
    const unsigned char first = static_cast<unsigned char>(key.front());
    if (!(std::isalpha(first) || key.front() == '_')) {
        return false;
    }
    for (char c : key) {
        const unsigned char uc = static_cast<unsigned char>(c);
        if (!(std::isalnum(uc) || c == '_')) {
            return false;
        }
    }
    return true;
}

std::optional<std::string> key_value_file_get_value(const fs::path& path, const std::string& key_name) {
    std::ifstream in(path);
    if (!in) {
        return std::nullopt;
    }

    std::string line;
    while (std::getline(in, line)) {
        std::string stripped = trim(line);
        if (stripped.empty() || stripped[0] == '#') {
            continue;
        }
        if (stripped.rfind("export ", 0) == 0) {
            stripped = trim(stripped.substr(7));
        }

        const auto eq = stripped.find('=');
        if (eq == std::string::npos) {
            continue;
        }
        const std::string key = trim(stripped.substr(0, eq));
        if (key != key_name) {
            continue;
        }

        std::string value = trim(stripped.substr(eq + 1));
        if (value.size() >= 2 &&
            ((value.front() == '"' && value.back() == '"') ||
             (value.front() == '\'' && value.back() == '\''))) {
            value = value.substr(1, value.size() - 2);
        }
        return value;
    }
    return std::nullopt;
}

bool key_value_file_has_non_empty_value(const fs::path& path, const std::string& key_name) {
    auto value = key_value_file_get_value(path, key_name);
    return value.has_value() && !trim(*value).empty();
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

int run_bash(const std::string& command) {
    const std::string wrapped = "bash -lc " + shell_quote(command);
    const int status = std::system(wrapped.c_str());
    if (status == -1) {
        return 1;
    }
    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    return 1;
}

std::string run_bash_capture(const std::string& command, int* exit_code) {
    const std::string wrapped = "bash -lc " + shell_quote(command);
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

bool has_jq() {
    return run_bash("command -v jq >/dev/null 2>&1") == 0;
}

std::string now_utc_iso8601() {
    std::time_t now = std::time(nullptr);
    std::tm tm{};
    gmtime_r(&now, &tm);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm);
    return std::string(buf);
}

long long now_epoch() {
    return static_cast<long long>(std::time(nullptr));
}

std::string utc_iso8601_from_epoch(long long epoch) {
    std::time_t t = static_cast<std::time_t>(epoch);
    std::tm tm{};
    gmtime_r(&t, &tm);
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

fs::path resolve_root_dir() {
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
    paths.exports_root = paths.runtime_dir / "exports";
    paths.backlog_dir = paths.runtime_dir / "backlog";

    ensure_directory(paths.logs_root);
    ensure_directory(paths.exports_root);
    ensure_directory(paths.backlog_dir);
    return paths;
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

std::optional<fs::path> resolve_binary(
    const RuntimePaths& paths,
    const char* env_key,
    const fs::path& system_binary
) {
    if (auto env_path = get_env_non_empty(env_key); env_path.has_value()) {
        const fs::path candidate(*env_path);
        if (file_is_executable(candidate)) {
            return candidate;
        }
    }

    if (file_is_executable(system_binary)) {
        return system_binary;
    }

    const fs::path cache_binary = paths.runtime_dir / "cache" / "bin" / system_binary.filename();
    if (file_is_executable(cache_binary)) {
        return cache_binary;
    }
    return std::nullopt;
}

std::string read_config_value(const fs::path& config_path, const std::string& key) {
    std::ifstream in(config_path);
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

void log_line(const fs::path& log_file, const std::string& message) {
    ensure_directory(log_file.parent_path());
    std::ofstream out(log_file, std::ios::out | std::ios::app);
    if (out) {
        out << "[" << now_utc_iso8601() << "] " << message << "\n";
    }
    std::cout << "[" << now_utc_iso8601() << "] " << message << std::endl;
}

int run_logged(const std::string& command, const fs::path& log_file) {
    const std::string cmd =
        command + " >> " + shell_quote(log_file.string()) + " 2>&1";
    return run_bash(cmd);
}

bool ensure_jq_available(const fs::path& log_file) {
    if (has_jq()) {
        return true;
    }

    log_line(log_file, "jq not found; attempting automatic installation...");

    const std::string install_cmd =
        "if command -v apt-get >/dev/null 2>&1; then "
        "  if [ \"$(id -u)\" -eq 0 ]; then "
        "    DEBIAN_FRONTEND=noninteractive apt-get update && "
        "    DEBIAN_FRONTEND=noninteractive apt-get install -y jq; "
        "  elif command -v sudo >/dev/null 2>&1; then "
        "    sudo -n apt-get update && sudo -n apt-get install -y jq; "
        "  else "
        "    exit 1; "
        "  fi; "
        "elif command -v dnf >/dev/null 2>&1; then "
        "  if [ \"$(id -u)\" -eq 0 ]; then dnf install -y jq; "
        "  elif command -v sudo >/dev/null 2>&1; then sudo -n dnf install -y jq; "
        "  else exit 1; fi; "
        "elif command -v yum >/dev/null 2>&1; then "
        "  if [ \"$(id -u)\" -eq 0 ]; then yum install -y jq; "
        "  elif command -v sudo >/dev/null 2>&1; then sudo -n yum install -y jq; "
        "  else exit 1; fi; "
        "elif command -v apk >/dev/null 2>&1; then "
        "  if [ \"$(id -u)\" -eq 0 ]; then apk add --no-cache jq; "
        "  elif command -v sudo >/dev/null 2>&1; then sudo -n apk add --no-cache jq; "
        "  else exit 1; fi; "
        "elif command -v pacman >/dev/null 2>&1; then "
        "  if [ \"$(id -u)\" -eq 0 ]; then pacman -Sy --noconfirm jq; "
        "  elif command -v sudo >/dev/null 2>&1; then sudo -n pacman -Sy --noconfirm jq; "
        "  else exit 1; fi; "
        "elif command -v zypper >/dev/null 2>&1; then "
        "  if [ \"$(id -u)\" -eq 0 ]; then zypper --non-interactive install jq; "
        "  elif command -v sudo >/dev/null 2>&1; then sudo -n zypper --non-interactive install jq; "
        "  else exit 1; fi; "
        "elif command -v brew >/dev/null 2>&1; then "
        "  brew install jq; "
        "else "
        "  exit 1; "
        "fi";

    const int install_rc = run_bash(
        install_cmd + " >> " + shell_quote(log_file.string()) + " 2>&1"
    );
    if (install_rc != 0 || !has_jq()) {
        log_line(log_file, "Failed to install jq automatically. Install jq manually and rerun.");
        return false;
    }

    log_line(log_file, "jq installed successfully.");
    return true;
}

int check_table_exists(const std::string& table) {
    std::ostringstream cmd;
    cmd << "docker exec transcendence_db psql -U api42 -d api42 -t -A -c "
        << shell_quote("SELECT 1 FROM information_schema.tables WHERE table_name='" + table + "' LIMIT 1;")
        << " | grep -q '^1$'";
    return run_bash(cmd.str());
}

std::optional<long long> json_to_int64(const json& value) {
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
    return std::nullopt;
}

bool write_text_atomic(const fs::path& output_path, const std::string& content, const fs::path& log_file) {
    ensure_directory(output_path.parent_path());
    const fs::path tmp_path = output_path.string() + ".tmp";

    {
        std::ofstream out(tmp_path, std::ios::out | std::ios::trunc);
        if (!out) {
            log_line(log_file, "Cannot write temp file: " + tmp_path.string());
            return false;
        }
        out << content;
    }

    std::error_code ec;
    fs::rename(tmp_path, output_path, ec);
    if (ec) {
        fs::remove(output_path, ec);
        fs::rename(tmp_path, output_path, ec);
        if (ec) {
            log_line(log_file, "Cannot move file into place: " + output_path.string());
            return false;
        }
    }
    return true;
}

bool write_json_atomic(const fs::path& output_path, const json& payload, const fs::path& log_file) {
    return write_text_atomic(output_path, payload.dump(2) + "\n", log_file);
}

bool read_json_file(const fs::path& input_path, json* out, const fs::path& log_file) {
    std::ifstream in(input_path);
    if (!in) {
        log_line(log_file, "Cannot open JSON file: " + input_path.string());
        return false;
    }
    try {
        in >> *out;
    } catch (const std::exception& ex) {
        log_line(log_file, "Invalid JSON in " + input_path.string() + ": " + ex.what());
        return false;
    }
    return true;
}

bool write_epoch_and_stats(
    const fs::path& export_dir,
    long long epoch,
    const std::vector<std::pair<std::string, std::string>>& stats_lines,
    const fs::path& log_file
) {
    std::ostringstream epoch_out;
    epoch_out << epoch << "\n";
    if (!write_text_atomic(export_dir / ".last_fetch_epoch", epoch_out.str(), log_file)) {
        return false;
    }

    std::ostringstream stats_out;
    for (const auto& line : stats_lines) {
        stats_out << line.first << "=" << line.second << "\n";
    }
    return write_text_atomic(export_dir / ".last_fetch_stats", stats_out.str(), log_file);
}

std::optional<json> call_api_json(
    const fs::path& api_client_bin,
    const std::string& endpoint,
    const fs::path& log_file,
    const std::string& step_label
) {
    const auto contains_rate_limit_hint = [](const std::string& text) {
        std::string lower = text;
        std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        return lower.find("429") != std::string::npos ||
               lower.find("too many requests") != std::string::npos ||
               lower.find("rate limit") != std::string::npos ||
               lower.find("ratelimit") != std::string::npos;
    };

    const auto json_is_rate_limited = [&](const json& payload) {
        if (payload.is_object()) {
            if (payload.contains("status")) {
                const auto status = json_to_int64(payload["status"]);
                if (status.has_value() && *status == 429) {
                    return true;
                }
            }
            if (payload.contains("error") && payload["error"].is_string() &&
                contains_rate_limit_hint(payload["error"].get<std::string>())) {
                return true;
            }
            if (payload.contains("message") && payload["message"].is_string() &&
                contains_rate_limit_hint(payload["message"].get<std::string>())) {
                return true;
            }
        }
        if (payload.is_string()) {
            return contains_rate_limit_hint(payload.get<std::string>());
        }
        return false;
    };

    const int max_attempts =
        std::max(1, static_cast<int>(parse_int64(get_env_or_default("ORCHESTRA_API_MAX_ATTEMPTS", "6")).value_or(6)));
    const int base_delay_ms =
        std::max(250, static_cast<int>(parse_int64(get_env_or_default("ORCHESTRA_API_RETRY_BASE_MS", "1500")).value_or(1500)));
    const int max_delay_ms =
        std::max(base_delay_ms, static_cast<int>(parse_int64(get_env_or_default("ORCHESTRA_API_RETRY_MAX_MS", "12000")).value_or(12000)));
    const int min_interval_ms =
        std::max(0, static_cast<int>(parse_int64(get_env_or_default("ORCHESTRA_API_MIN_INTERVAL_MS", "500")).value_or(500)));

    static std::mutex api_gate_mutex;
    static std::chrono::steady_clock::time_point next_allowed_call{};

    const auto wait_for_api_slot = [&]() {
        if (min_interval_ms <= 0) {
            return;
        }
        std::lock_guard<std::mutex> lock(api_gate_mutex);
        const auto now = std::chrono::steady_clock::now();
        if (now < next_allowed_call) {
            std::this_thread::sleep_for(next_allowed_call - now);
        }
        next_allowed_call = std::chrono::steady_clock::now() + std::chrono::milliseconds(min_interval_ms);
    };

    const std::string cmd =
        shell_quote(api_client_bin.string()) + " call " + shell_quote(endpoint) + " 2>/dev/null";

    for (int attempt = 1; attempt <= max_attempts; ++attempt) {
        wait_for_api_slot();
        int rc = 0;
        const std::string body = trim(run_bash_capture(cmd, &rc));

        bool retryable = false;
        std::string reason;

        if (rc != 0 || body.empty()) {
            retryable = true;
            reason = "API call failed";
        } else {
            try {
                json payload = json::parse(body);
                if (json_is_rate_limited(payload)) {
                    retryable = true;
                    reason = "API rate limited";
                } else {
                    return payload;
                }
            } catch (const std::exception& ex) {
                retryable = true;
                reason = "invalid JSON (" + std::string(ex.what()) + ")";
                if (!contains_rate_limit_hint(body)) {
                    reason += " response starts with: " + body.substr(0, std::min<std::size_t>(64, body.size()));
                }
            }
        }

        if (!retryable || attempt >= max_attempts) {
            log_line(
                log_file,
                step_label + ": " + reason + " for " + endpoint +
                    " (attempt " + std::to_string(attempt) + "/" + std::to_string(max_attempts) + ")"
            );
            return std::nullopt;
        }

        const int shift = std::min(attempt - 1, 6);
        const int backoff_ms = std::min(max_delay_ms, base_delay_ms * (1 << shift));
        log_line(
            log_file,
            step_label + ": " + reason + " for " + endpoint +
                ", retrying in " + std::to_string(backoff_ms) + "ms" +
                " (attempt " + std::to_string(attempt) + "/" + std::to_string(max_attempts) + ")"
        );
        std::this_thread::sleep_for(std::chrono::milliseconds(backoff_ms));
    }
    return std::nullopt;
}

int cmd_check_environment(const RuntimePaths& paths) {
    int issues = 0;

    auto check = [&](const std::string& label, const std::string& command, const std::string& fail_reason) {
        std::cout << std::left << std::setw(34) << label;
        const int rc = run_bash(command + " >/dev/null 2>&1");
        if (rc == 0) {
            std::cout << "[OK]" << std::endl;
            return true;
        }
        std::cout << "[FAIL] " << fail_reason << std::endl;
        issues += 1;
        return false;
    };

    std::cout << "============================================" << std::endl;
    std::cout << "Environment Check" << std::endl;
    std::cout << "============================================" << std::endl;

    (void)check("Docker daemon", "timeout 5 docker ps", "docker is not reachable");
    (void)check("docker compose", "docker compose version", "docker compose missing");
    std::cout << std::left << std::setw(34) << "jq";
    if (has_jq()) {
        std::cout << "[OK]" << std::endl;
    } else {
        std::cout << "[WARN] missing (will auto-install during orchestra)" << std::endl;
    }

    std::cout << std::left << std::setw(34) << "Exports directory";
    bool exports_ok = true;
    ensure_directory(paths.exports_root / "09_users");
    const fs::path test_file = paths.exports_root / ".write_test";
    {
        std::ofstream out(test_file);
        if (!out) {
            exports_ok = false;
        }
    }
    std::error_code ec;
    fs::remove(test_file, ec);
    if (exports_ok) {
        std::cout << "[OK]" << std::endl;
    } else {
        std::cout << "[FAIL] exports not writable" << std::endl;
        issues += 1;
    }

    std::cout << std::left << std::setw(34) << "Repo structure";
    if (fs::exists(paths.root_dir / "infra" / "docker-compose.yml") && fs::exists(paths.root_dir / "app")) {
        std::cout << "[OK]" << std::endl;
    } else {
        std::cout << "[FAIL] expected infra/docker-compose.yml and app/" << std::endl;
        issues += 1;
    }

    const fs::path env_file = paths.root_dir.parent_path() / ".env";
    std::cout << std::left << std::setw(34) << "Environment file syntax";
    bool env_syntax_ok = true;
    std::size_t bad_line_no = 0;
    std::string bad_line_text;
    {
        std::ifstream in(env_file);
        if (!in) {
            env_syntax_ok = false;
            bad_line_text = "cannot open file";
        } else {
            std::string line;
            std::size_t line_no = 0;
            while (std::getline(in, line)) {
                line_no += 1;
                const std::string stripped = trim(line);
                if (stripped.empty() || stripped[0] == '#') {
                    continue;
                }
                const auto eq = stripped.find('=');
                const std::string key = (eq == std::string::npos) ? "" : trim(stripped.substr(0, eq));
                if (eq == std::string::npos || !is_valid_env_key(key)) {
                    env_syntax_ok = false;
                    bad_line_no = line_no;
                    bad_line_text = stripped;
                    break;
                }
            }
        }
    }
    if (env_syntax_ok) {
        std::cout << "[OK]" << std::endl;
    } else {
        std::cout << "[FAIL] invalid line in " << env_file;
        if (bad_line_no > 0) {
            std::cout << ":" << bad_line_no;
        }
        if (!bad_line_text.empty()) {
            std::cout << " (" << bad_line_text << ")";
        }
        std::cout << std::endl;
        issues += 1;
    }

    const fs::path oauth_state_file = paths.root_dir / ".oauth_state";
    const bool token_ok = key_value_file_has_non_empty_value(oauth_state_file, "REFRESH_TOKEN");

    std::cout << std::left << std::setw(34) << "OAuth refresh token";
    if (token_ok) {
        std::cout << "[OK] .oauth_state" << std::endl;
    } else {
        std::cout << "[FAIL] REFRESH_TOKEN missing in " << oauth_state_file
                  << " (run: make exchange CODE=\"<AUTHORIZATION_CODE>\")"
                  << std::endl;
        issues += 1;
    }

    std::cout << std::left << std::setw(34) << "OAuth refresh capability";
    if (!token_ok) {
        std::cout << "[SKIP]" << std::endl;
    } else {
        auto token_bin = resolve_binary(paths, "TOKEN_MANAGER_BINARY", "/usr/local/bin/token-manager-agent");
        if (!token_bin.has_value()) {
            std::cout << "[FAIL] token-manager-agent not found" << std::endl;
            issues += 1;
        } else {
            const int refresh_ok = run_bash(
                shell_quote(token_bin->string()) + " refresh >/dev/null"
            );
            if (refresh_ok == 0) {
                std::cout << "[OK]" << std::endl;
            } else {
                std::cout << "[FAIL] cannot refresh token (see error above)" << std::endl;
                issues += 1;
            }
        }
    }

    std::cout << std::left << std::setw(34) << "Database container";
    const int db_running = run_bash(
        "timeout 5 docker ps --format '{{.Names}}' | grep -q '^transcendence_db$' >/dev/null 2>&1"
    );
    if (db_running == 0) {
        std::cout << "[OK] running" << std::endl;
        std::cout << std::left << std::setw(34) << "Database connectivity";
        const int db_ok = run_bash(
            "timeout 5 docker exec transcendence_db psql -U api42 -d api42 -c 'SELECT 1' >/dev/null 2>&1"
        );
        if (db_ok == 0) {
            std::cout << "[OK]" << std::endl;
        } else {
            std::cout << "[FAIL] cannot query db" << std::endl;
            issues += 1;
        }
    } else {
        std::cout << "[OK] not running (will be started by deploy)" << std::endl;
    }

    std::cout << "============================================" << std::endl;
    if (issues == 0) {
        std::cout << "READY" << std::endl;
        return 0;
    }
    std::cout << "FAILED CHECKS: " << issues << std::endl;
    return 1;
}

int cmd_init_db(const RuntimePaths& paths) {
    const fs::path log_file = paths.logs_root / ("init_db_" + std::to_string(now_epoch()) + ".log");
    log_line(log_file, "Initializing database");

    const int max_attempts = 30;
    bool ready = false;
    for (int attempt = 1; attempt <= max_attempts; ++attempt) {
        if (run_bash("docker exec transcendence_db pg_isready -U api42 >/dev/null 2>&1") == 0) {
            ready = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    if (!ready) {
        log_line(log_file, "Database not ready after 30 attempts");
        return 1;
    }

    const fs::path schema_file = paths.root_dir / "sql" / "schema.sql";
    if (!fs::exists(schema_file)) {
        log_line(log_file, "Missing schema file: " + schema_file.string());
        return 1;
    }

    const std::string load_cmd =
        "docker exec -i transcendence_db psql -U api42 -d api42 < " + shell_quote(schema_file.string());
    if (run_logged(load_cmd, log_file) != 0) {
        log_line(log_file, "Schema load failed");
        return 1;
    }

    const std::vector<std::string> tables = {
        "cursus", "campuses", "projects", "coalitions",
        "achievements", "users", "project_users",
        "achievements_users", "coalitions_users"
    };

    int missing = 0;
    for (const auto& table : tables) {
        if (check_table_exists(table) != 0) {
            log_line(log_file, "Missing table: " + table);
            missing += 1;
        }
    }

    if (missing > 0) {
        log_line(log_file, "Table verification failed");
        return 1;
    }

    log_line(log_file, "Database initialization completed");
    return 0;
}

} // namespace orchestra
