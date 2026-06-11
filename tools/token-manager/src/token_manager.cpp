#include <array>
#include <cctype>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

#include "../../../third_party/nlohmann/json.hpp"

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace {

struct Paths {
    fs::path root_dir;
    fs::path repo_root;
    fs::path runtime_dir;
    fs::path logs_root;
    fs::path exports_root;
    fs::path state_file;
    fs::path log_file;
};

struct Config {
    std::string api_root{"https://api.intra.42.fr"};
    std::string client_id;
    std::string client_secret;
    std::string redirect_uri;
    std::string scope;
};

struct State {
    std::string access_token;
    std::string refresh_token;
    long long expires_at{0};
};

std::string trim(const std::string& value) {
    const auto first = value.find_first_not_of(" \t\n\r");
    if (first == std::string::npos) {
        return "";
    }
    const auto last = value.find_last_not_of(" \t\n\r");
    return value.substr(first, last - first + 1);
}

std::optional<std::string> getenv_non_empty(const char* key) {
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

std::string getenv_or_default(const char* key, const std::string& fallback) {
    auto value = getenv_non_empty(key);
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

void ensure_parent_dir(const fs::path& path) {
    std::error_code ec;
    fs::create_directories(path.parent_path(), ec);
}

std::string now_utc_human() {
    std::time_t now = std::time(nullptr);
    std::tm tm{};
    gmtime_r(&now, &tm);
    char buf[64];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S UTC", &tm);
    return std::string(buf);
}

fs::path resolve_root_dir() {
    if (auto env_root = getenv_non_empty("ROOT_DIR"); env_root.has_value()) {
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

std::map<std::string, std::string> load_key_value_file(const fs::path& path) {
    std::map<std::string, std::string> out;
    std::ifstream in(path);
    if (!in) {
        return out;
    }

    std::string line;
    while (std::getline(in, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') {
            continue;
        }

        const auto eq = line.find('=');
        if (eq == std::string::npos) {
            continue;
        }

        std::string key = trim(line.substr(0, eq));
        std::string val = trim(line.substr(eq + 1));
        std::string without_inline_comment;
        without_inline_comment.reserve(val.size());

        bool in_single_quote = false;
        bool in_double_quote = false;
        for (std::size_t i = 0; i < val.size(); ++i) {
            const char c = val[i];
            if (c == '\'' && !in_double_quote) {
                in_single_quote = !in_single_quote;
                without_inline_comment.push_back(c);
                continue;
            }
            if (c == '"' && !in_single_quote) {
                in_double_quote = !in_double_quote;
                without_inline_comment.push_back(c);
                continue;
            }
            if (c == '#' && !in_single_quote && !in_double_quote) {
                const bool start_of_comment = (i == 0) || std::isspace(static_cast<unsigned char>(val[i - 1]));
                if (start_of_comment) {
                    break;
                }
            }
            without_inline_comment.push_back(c);
        }
        val = trim(without_inline_comment);
        if (val.size() >= 2 && ((val.front() == '"' && val.back() == '"') || (val.front() == '\'' && val.back() == '\''))) {
            val = val.substr(1, val.size() - 2);
        }
        out[key] = val;
    }

    return out;
}

Paths build_paths() {
    Paths p;
    p.root_dir = resolve_root_dir();

    p.repo_root = p.root_dir.parent_path();
    if (p.repo_root.empty() || p.repo_root == "/") {
        p.repo_root = p.root_dir;
    }

    p.runtime_dir = fs::path(getenv_or_default("RUNTIME_DIR", (p.root_dir / ".." / "runtime").string()));

    p.logs_root = p.runtime_dir / "logs";
    p.exports_root = p.runtime_dir / "exports";

    p.state_file = fs::path(getenv_or_default("STATE_FILE", (p.root_dir / ".oauth_state").string()));
    p.log_file = fs::path(getenv_or_default("LOG_FILE", (p.logs_root / "agents" / "token_manager.log").string()));

    return p;
}

bool load_config(const Paths& paths, Config* cfg, std::string* error) {
    cfg->api_root = getenv_or_default("API_ROOT", "https://api.intra.42.fr");

    const auto env_client_id = getenv_non_empty("CLIENT_ID");
    const auto env_client_secret = getenv_non_empty("CLIENT_SECRET");
    const auto env_redirect_uri = getenv_non_empty("REDIRECT_URI");
    const auto env_scope = getenv_non_empty("SCOPE");

    if (env_client_id.has_value()) cfg->client_id = *env_client_id;
    if (env_client_secret.has_value()) cfg->client_secret = *env_client_secret;
    if (env_redirect_uri.has_value()) cfg->redirect_uri = *env_redirect_uri;
    if (env_scope.has_value()) cfg->scope = *env_scope;

    std::vector<fs::path> candidates;
    if (auto config_file = getenv_non_empty("CONFIG_FILE"); config_file.has_value()) {
        candidates.emplace_back(*config_file);
    }
    candidates.push_back(paths.root_dir / ".env");
    candidates.push_back(paths.repo_root / ".env");

    for (const auto& cfg_path : candidates) {
        if (!fs::exists(cfg_path)) {
            continue;
        }

        auto values = load_key_value_file(cfg_path);
        if (cfg->client_id.empty() && values.count("CLIENT_ID")) cfg->client_id = values["CLIENT_ID"];
        if (cfg->client_secret.empty() && values.count("CLIENT_SECRET")) cfg->client_secret = values["CLIENT_SECRET"];
        if (cfg->redirect_uri.empty() && values.count("REDIRECT_URI")) cfg->redirect_uri = values["REDIRECT_URI"];
        if (cfg->scope.empty() && values.count("SCOPE")) cfg->scope = values["SCOPE"];
        if (auto api_env = getenv_non_empty("API_ROOT"); !api_env.has_value() && values.count("API_ROOT")) {
            cfg->api_root = values["API_ROOT"];
        }
    }

    std::vector<std::string> missing;
    if (cfg->client_id.empty()) missing.push_back("CLIENT_ID");
    if (cfg->client_secret.empty()) missing.push_back("CLIENT_SECRET");
    if (cfg->redirect_uri.empty()) missing.push_back("REDIRECT_URI");
    if (cfg->scope.empty()) missing.push_back("SCOPE");

    if (!missing.empty()) {
        std::ostringstream msg;
        msg << "Missing config values:";
        for (const auto& m : missing) {
            msg << " " << m;
        }
        msg << ". Provide them via env vars or .env file.";
        *error = msg.str();
        return false;
    }

    return true;
}

bool load_state(const Paths& paths, State* state) {
    if (!fs::exists(paths.state_file)) {
        return false;
    }
    auto values = load_key_value_file(paths.state_file);
    if (values.count("ACCESS_TOKEN")) {
        state->access_token = values["ACCESS_TOKEN"];
    }
    if (values.count("REFRESH_TOKEN")) {
        state->refresh_token = values["REFRESH_TOKEN"];
    }
    if (values.count("EXPIRES_AT")) {
        state->expires_at = parse_int64(values["EXPIRES_AT"]).value_or(0);
    }
    return true;
}

bool save_state(const Paths& paths, const State& state) {
    ensure_parent_dir(paths.state_file);
    std::ofstream out(paths.state_file, std::ios::out | std::ios::trunc);
    if (!out) {
        return false;
    }

    out << "ACCESS_TOKEN=" << state.access_token << "\n";
    out << "REFRESH_TOKEN=" << state.refresh_token << "\n";
    out << "EXPIRES_AT=" << state.expires_at << "\n";
    return true;
}

void append_log(const fs::path& path, const std::string& line) {
    ensure_parent_dir(path);
    std::ofstream out(path, std::ios::out | std::ios::app);
    if (out) {
        out << line << "\n";
    }
}

json parse_json_or_empty(const std::string& text) {
    try {
        return json::parse(text);
    } catch (...) {
        return json::object();
    }
}

std::string http_post_token(const Config& cfg, const std::vector<std::pair<std::string, std::string>>& fields) {
    std::string cmd =
        "curl -sS -X POST " + shell_quote(cfg.api_root + "/oauth/token") +
        " -u " + shell_quote(cfg.client_id + ":" + cfg.client_secret);

    for (const auto& [k, v] : fields) {
        // Use urlencoding for all form values to avoid grant/code breakage on special characters.
        cmd += " --data-urlencode " + shell_quote(k + "=" + v);
    }

    int rc = 0;
    std::string response = run_bash(cmd, &rc);
    (void)rc;
    return response;
}

std::pair<int, std::string> http_get_with_status(const std::string& url, const std::string& bearer_token, bool globbing) {
    std::string cmd = "curl -sS ";
    if (globbing) {
        cmd += "-g ";
    }
    cmd += "-w '\\n%{http_code}' -H " + shell_quote("Authorization: Bearer " + bearer_token) + " " + shell_quote(url);

    int rc = 0;
    std::string response = run_bash(cmd, &rc);
    (void)rc;

    const auto pos = response.rfind('\n');
    if (pos == std::string::npos) {
        return {0, response};
    }

    std::string body = response.substr(0, pos);
    std::string code_text = trim(response.substr(pos + 1));
    int code = static_cast<int>(parse_int64(code_text).value_or(0));
    return {code, body};
}

bool refresh_token_impl(
    const Paths& paths,
    const Config& cfg,
    State* state,
    bool print_response,
    const std::optional<fs::path>& log_file
) {
    if (state->refresh_token.empty()) {
        std::cerr << "No refresh token saved in " << paths.state_file
                  << ". Run: make exchange CODE=\"<AUTHORIZATION_CODE>\""
                  << std::endl;
        return false;
    }

    std::string ts = now_utc_human();
    if (log_file.has_value()) {
        append_log(*log_file, "[" + ts + "] Starting token refresh...");
    }

    auto request_refresh = [&](const std::string& refresh_token) {
        return http_post_token(cfg, {
            {"grant_type", "refresh_token"},
            {"refresh_token", refresh_token},
        });
    };

    std::string response = request_refresh(state->refresh_token);

    if (print_response) {
        json parsed = parse_json_or_empty(response);
        if (!parsed.is_null() && !parsed.empty()) {
            std::cout << parsed.dump(2) << std::endl;
        } else {
            std::cout << response << std::endl;
        }
    }

    json payload = parse_json_or_empty(response);
    std::string access = payload.value("access_token", "");
    std::string refresh = payload.value("refresh_token", "");
    long long expires_in = payload.value("expires_in", 0);
    std::string oauth_error = payload.value("error", "");
    std::string oauth_error_description = payload.value("error_description", "");

    if (access.empty()) {
        if (!oauth_error.empty()) {
            std::cerr << "Token refresh failed: " << oauth_error;
            if (!oauth_error_description.empty()) {
                std::cerr << " - " << oauth_error_description;
            }
            std::cerr << std::endl;
        } else if (!trim(response).empty()) {
            std::cerr << "Token refresh failed: " << trim(response) << std::endl;
        } else {
            std::cerr << "Token refresh failed: empty response" << std::endl;
        }
        if (log_file.has_value()) {
            std::string log_msg = "[" + ts + "] Token refresh FAILED";
            if (!oauth_error.empty()) {
                log_msg += " (" + oauth_error;
                if (!oauth_error_description.empty()) {
                    log_msg += ": " + oauth_error_description;
                }
                log_msg += ")";
            }
            append_log(*log_file, log_msg);
        }
        return false;
    }

    state->access_token = access;
    if (!refresh.empty()) {
        state->refresh_token = refresh;
    }
    state->expires_at = static_cast<long long>(std::time(nullptr)) + expires_in;

    if (!save_state(paths, *state)) {
        std::cerr << "Failed to write state file: " << paths.state_file << std::endl;
        return false;
    }

    if (log_file.has_value()) {
        append_log(*log_file, "[" + ts + "] Token refresh successful.");
    }
    return true;
}

void print_usage(const std::string& argv0, const Paths& paths) {
    std::cout
        << "Usage: " << argv0 << " <command> [args]\n\n"
        << "Commands:\n"
        << "  exchange <code>       Exchange authorization code for tokens and save.\n"
        << "  refresh               Refresh access token using saved refresh token.\n"
        << "  ensure-fresh          Refresh proactively if token expires in < 1 hour.\n"
        << "  token-info            Show saved token info and /oauth/token/info.\n\n"
        << "State file: " << paths.state_file.string() << "\n";
}

}  // namespace

int main(int argc, char** argv) {
    const Paths paths = build_paths();

    if (argc < 2) {
        print_usage(argv[0], paths);
        return 0;
    }

    const std::string cmd = argv[1];

    Config cfg;
    std::string cfg_error;
    if ((cmd == "exchange" || cmd == "refresh" || cmd == "ensure-fresh" || cmd == "token-info")
        && !load_config(paths, &cfg, &cfg_error)) {
        std::cerr << cfg_error << std::endl;
        return 1;
    }

    State state;
    load_state(paths, &state);

    if (cmd == "exchange") {
        if (argc < 3) {
            std::cerr << "Usage: " << argv[0] << " exchange <authorization_code>" << std::endl;
            return 1;
        }

        const std::string code = argv[2];
        std::string response = http_post_token(cfg, {
            {"grant_type", "authorization_code"},
            {"code", code},
            {"redirect_uri", cfg.redirect_uri},
        });

        json payload = parse_json_or_empty(response);
        if (!payload.is_null() && !payload.empty()) {
            std::cout << payload.dump(2) << std::endl;
        } else {
            std::cout << response << std::endl;
        }

        state.access_token = payload.value("access_token", "");
        state.refresh_token = payload.value("refresh_token", "");
        state.expires_at = static_cast<long long>(std::time(nullptr)) + payload.value("expires_in", 0);

        if (state.access_token.empty()) {
            return 1;
        }
        if (!save_state(paths, state)) {
            std::cerr << "Failed to save state to " << paths.state_file << std::endl;
            return 1;
        }
        std::cerr << "State saved to " << paths.state_file << std::endl;
        return 0;
    }

    if (cmd == "refresh") {
        if (!refresh_token_impl(paths, cfg, &state, false, paths.log_file)) {
            return 1;
        }
        return 0;
    }

    if (cmd == "ensure-fresh") {
        if (state.access_token.empty()) {
            std::cerr << "No access token saved in " << paths.state_file
                      << ". Run: make exchange CODE=\"<AUTHORIZATION_CODE>\""
                      << std::endl;
            return 1;
        }
        if (state.expires_at <= 0) {
            std::cerr << "No expiry found. Run exchange to regenerate state." << std::endl;
            return 1;
        }

        long long now = static_cast<long long>(std::time(nullptr));
        long long expires_in = state.expires_at - now;
        if (expires_in < 3600) {
            std::cerr << "Token expires in " << expires_in << "s, refreshing proactively..." << std::endl;
            if (!refresh_token_impl(paths, cfg, &state, false, paths.log_file)) {
                return 1;
            }
            load_state(paths, &state);
            long long after = state.expires_at - static_cast<long long>(std::time(nullptr));
            std::cerr << "Token refreshed, new expiry in " << after << "s" << std::endl;
        }
        return 0;
    }

    if (cmd == "token-info") {
        if (state.access_token.empty()) {
            std::cerr << "No access token saved in " << paths.state_file
                      << ". Run: make exchange CODE=\"<AUTHORIZATION_CODE>\""
                      << std::endl;
            return 1;
        }

        std::cout << "Stored tokens:\n";
        std::cout << "  ACCESS_TOKEN=" << state.access_token << "\n";
        std::cout << "  REFRESH_TOKEN=" << (state.refresh_token.empty() ? "<none>" : state.refresh_token) << "\n";
        if (state.expires_at > 0) {
            long long remaining = state.expires_at - static_cast<long long>(std::time(nullptr));
            std::cout << "  EXPIRES_AT=" << state.expires_at << " (in " << remaining << "s)\n";
        }

        std::cout << "Token info from API:\n";
        auto [code, body] = http_get_with_status(cfg.api_root + "/oauth/token/info", state.access_token, false);
        (void)code;

        json info;
        try {
            info = json::parse(body);
            std::cout << info.dump(2) << std::endl;
        } catch (...) {
            std::cout << body << std::endl;
        }
        return 0;
    }

    print_usage(argv[0], paths);
    return 0;
}
