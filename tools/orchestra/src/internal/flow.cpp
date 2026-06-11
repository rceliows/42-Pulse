#include "orchestra_internal.hpp"

namespace orchestra {

int cmd_orchestra(const RuntimePaths& paths, const std::vector<std::string>& args, const fs::path& log_file) {
    bool dry_run = false;
    for (const auto& arg : args) {
        if (arg == "--dry-run") {
            dry_run = true;
        } else {
            log_line(log_file, "Unknown orchestra arg: " + arg);
            return 1;
        }
    }

    const fs::path orchestra_conf = paths.root_dir / "tools" / "orchestra" / "config" / "orchestra.conf";

    log_line(log_file, "Starting orchestra");

    if (!ensure_jq_available(log_file)) {
        return 1;
    }

    auto token_bin = resolve_binary(paths, "TOKEN_MANAGER_BINARY", "/usr/local/bin/token-manager-agent");
    if (token_bin.has_value()) {
        if (run_logged(shell_quote(token_bin->string()) + " refresh", log_file) != 0) {
            log_line(log_file, "Token refresh failed");
            return 1;
        }
    } else {
        log_line(log_file, "token-manager-agent not found; skipping refresh");
    }

    const std::string metadata_fetch_cfg = get_env_or_default(
        "ORCHESTRA_METADATA_FETCH",
        read_config_value(orchestra_conf, "ORCHESTRA_METADATA_FETCH").empty() ? "1" : read_config_value(orchestra_conf, "ORCHESTRA_METADATA_FETCH")
    );
    const bool run_metadata = metadata_fetch_cfg != "0";
    if (run_metadata) {
        if (cmd_fetch_metadata(paths, log_file) != 0) {
            log_line(log_file, "Metadata phase failed");
            return 1;
        }
    } else {
        log_line(log_file, "Metadata phase skipped");
    }

    if (!dry_run) {
        if (cmd_init_db(paths) != 0) {
            log_line(log_file, "Database initialization failed");
            return 1;
        }
        if (cmd_load_metadata(paths, log_file) != 0) {
            log_line(log_file, "Metadata database load failed");
            return 1;
        }
    } else {
        log_line(log_file, "Dry run: skipping database loaders");
    }

    if (!dry_run) {
        log_line(log_file, "Backlog worker lifecycle is managed by docker compose");
    }

    log_line(log_file, "Orchestra completed");
    return 0;
}

void print_usage() {
    std::cerr
        << "Usage: orchestration-agent <command> [args...]\n"
        << "Commands:\n"
        << "  check_environment\n"
        << "  orchestra [--dry-run]\n";
}

bool has_help_flag(const std::vector<std::string>& args) {
    for (const auto& arg : args) {
        if (arg == "--help" || arg == "-h") {
            return true;
        }
    }
    return false;
}

} // namespace orchestra
