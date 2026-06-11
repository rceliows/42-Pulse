#include "internal/ops_internal.hpp"

#include <cstdlib>
#include <exception>
#include <iostream>
#include <string>
#include <vector>

int main(int argc, char** argv) {
    try {
        if (argc < 2) {
            ops::print_usage();
            return 1;
        }

        const ops::RuntimePaths paths = ops::resolve_runtime_paths();
        ::setenv("ROOT_DIR", paths.root_dir.string().c_str(), 1);

        const std::string command = argv[1];
        std::vector<std::string> args;
        args.reserve(static_cast<std::size_t>(argc > 2 ? argc - 2 : 0));
        for (int i = 2; i < argc; ++i) {
            args.emplace_back(argv[i]);
        }

        if (command == "--help" || command == "-h") {
            ops::print_usage();
            return 0;
        }
        if (command == "system_health") {
            return ops::cmd_system_health(paths);
        }
        if (command == "events_diff") {
            return ops::cmd_events_diff(paths, args);
        }
        if (command == "refresh_token") {
            return ops::cmd_refresh_token(paths);
        }
        if (command == "backup") {
            return ops::cmd_backup(paths);
        }
        if (command == "cleanup") {
            return ops::cmd_cleanup(paths);
        }
        if (command == "maintenance") {
            return ops::cmd_maintenance(paths);
        }

        std::cerr << "Unknown command: " << command << "\n";
        ops::print_usage();
        return 1;
    } catch (const std::exception& ex) {
        std::cerr << "ops-agent error: " << ex.what() << "\n";
        return 1;
    }
}
