#include <algorithm>
#include <array>
#include <cctype>
#include <cerrno>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <sstream>
#include <string>
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
    fs::path exports_root;
    fs::path state_file;
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
    if (path.empty()) {
        return;
    }
    std::error_code ec;
    fs::create_directories(path.parent_path(), ec);
}

bool is_executable_file(const fs::path& path) {
    return ::access(path.c_str(), X_OK) == 0;
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

    p.exports_root = p.runtime_dir / "exports";

    p.state_file = fs::path(getenv_or_default("STATE_FILE", (p.root_dir / ".oauth_state").string()));
    return p;
}

std::string read_api_root(const Paths& paths) {
    if (auto api_root = getenv_non_empty("API_ROOT"); api_root.has_value()) {
        return *api_root;
    }

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
        auto it = values.find("API_ROOT");
        if (it != values.end() && !trim(it->second).empty()) {
            return trim(it->second);
        }
    }

    return "https://api.intra.42.fr";
}

std::optional<std::string> load_access_token(const fs::path& state_file) {
    if (!fs::exists(state_file)) {
        return std::nullopt;
    }
    auto values = load_key_value_file(state_file);
    auto it = values.find("ACCESS_TOKEN");
    if (it == values.end()) {
        return std::nullopt;
    }
    const std::string token = trim(it->second);
    if (token.empty()) {
        return std::nullopt;
    }
    return token;
}

fs::path resolve_token_manager_tool(const fs::path& root_dir) {
    if (auto env_path = getenv_non_empty("TOKEN_MANAGER_BINARY"); env_path.has_value()) {
        fs::path candidate(*env_path);
        if (is_executable_file(candidate)) {
            return candidate;
        }
    }

    const fs::path system_binary = "/usr/local/bin/token-manager-agent";
    if (is_executable_file(system_binary)) {
        return system_binary;
    }

    fs::path runtime_dir = root_dir / ".." / "runtime";
    if (auto runtime_env = getenv_non_empty("RUNTIME_DIR"); runtime_env.has_value()) {
        runtime_dir = fs::path(*runtime_env);
    }
    const fs::path runtime_cache_binary = runtime_dir / "cache" / "bin" / "token-manager-agent";
    if (is_executable_file(runtime_cache_binary)) {
        return runtime_cache_binary;
    }

    throw std::runtime_error(
        "token-manager-agent not found. Set TOKEN_MANAGER_BINARY, install /usr/local/bin/token-manager-agent, "
        "or build runtime/cache/bin/token-manager-agent");
}

bool ensure_fresh_token(const fs::path& token_manager_tool) {
    int rc = 0;
    run_bash(shell_quote(token_manager_tool.string()) + " ensure-fresh >/dev/null 2>&1", &rc);
    return rc == 0;
}

std::pair<int, std::string> http_get_with_status(const std::string& url, const std::string& bearer_token) {
    std::string cmd =
        "curl -sS -g -w '\\n%{http_code}' -H " +
        shell_quote("Authorization: Bearer " + bearer_token) +
        " " + shell_quote(url);

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

std::string make_safe_export_name(const std::string& endpoint) {
    std::string safe = endpoint;
    std::replace(safe.begin(), safe.end(), '/', '_');
    while (!safe.empty() && safe.front() == '_') {
        safe.erase(safe.begin());
    }
    if (safe.empty()) {
        safe = "response";
    }
    return safe;
}

void print_usage(const std::string& argv0, const Paths& paths) {
    std::cout
        << "Usage: " << argv0 << " <command> [args]\n\n"
        << "Commands:\n"
        << "  call <endpoint>       Call API endpoint (e.g. /v2/me).\n"
        << "  call-http <endpoint>  Call API and output {http_code, ok, body} JSON.\n"
        << "  call-export <ep> [f]  Call API and save pretty JSON to file.\n\n"
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
    if (cmd != "call" && cmd != "call-http" && cmd != "call-export") {
        print_usage(argv[0], paths);
        return 1;
    }
    if (argc < 3) {
        print_usage(argv[0], paths);
        return 1;
    }

    const std::string endpoint = argv[2];
    const std::string api_root = read_api_root(paths);

    fs::path token_manager_tool;
    try {
        token_manager_tool = resolve_token_manager_tool(paths.root_dir);
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << std::endl;
        return 1;
    }

    if (!ensure_fresh_token(token_manager_tool)) {
        std::cerr << "Failed to refresh token via token-manager-agent ensure-fresh" << std::endl;
        return 1;
    }

    auto token = load_access_token(paths.state_file);
    if (!token.has_value()) {
        std::cerr << "No ACCESS_TOKEN in " << paths.state_file
                  << ". Run token exchange first." << std::endl;
        return 1;
    }

    auto [code, body] = http_get_with_status(api_root + endpoint, *token);
    if (code == 401) {
        if (!ensure_fresh_token(token_manager_tool)) {
            std::cerr << "401 received and token refresh failed." << std::endl;
            return 1;
        }
        token = load_access_token(paths.state_file);
        if (!token.has_value()) {
            std::cerr << "No ACCESS_TOKEN in " << paths.state_file << " after refresh." << std::endl;
            return 1;
        }
        std::tie(code, body) = http_get_with_status(api_root + endpoint, *token);
    }

    if (cmd == "call") {
        std::cout << body;
        if (!body.empty() && body.back() != '\n') {
            std::cout << '\n';
        }
        return 0;
    }

    if (cmd == "call-http") {
        json payload = {
            {"http_code", code},
            {"ok", code >= 200 && code < 300},
            {"body", body},
        };
        std::cout << payload.dump() << '\n';
        return 0;
    }

    fs::path outfile;
    if (argc >= 4) {
        outfile = argv[3];
    } else {
        outfile = paths.exports_root / (make_safe_export_name(endpoint) + ".json");
    }

    json payload;
    try {
        payload = json::parse(body);
    } catch (...) {
        std::cerr << "Invalid JSON response (HTTP " << code << "). Response:\n" << body << std::endl;
        return 1;
    }

    ensure_parent_dir(outfile);
    std::ofstream out(outfile, std::ios::out | std::ios::trunc);
    if (!out) {
        std::cerr << "Failed to write output file: " << outfile << std::endl;
        return 1;
    }
    out << payload.dump(2) << '\n';
    std::cout << "Saved to " << outfile << std::endl;
    return 0;
}
