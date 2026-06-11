#include "internal/orchestra_internal.hpp"

int main(int argc, char** argv) {
    try {
        if (argc < 2) {
            orchestra::print_usage();
            return 1;
        }

        const orchestra::RuntimePaths paths = orchestra::resolve_runtime_paths();
        ::setenv("ROOT_DIR", paths.root_dir.string().c_str(), 1);

        const std::string command = argv[1];
        if (command == "--help" || command == "-h") {
            orchestra::print_usage();
            return 0;
        }

        std::vector<std::string> args;
        args.reserve(static_cast<std::size_t>(argc > 2 ? argc - 2 : 0));
        for (int i = 2; i < argc; ++i) {
            args.emplace_back(argv[i]);
        }

        const std::filesystem::path log_file =
            paths.logs_root / (command + "_" + std::to_string(orchestra::now_epoch()) + ".log");

        if (command == "check_environment") {
            if (orchestra::has_help_flag(args)) {
                std::cout << "Usage: orchestration-agent check_environment\n";
                return 0;
            }
            return orchestra::cmd_check_environment(paths);
        }

        if (command == "orchestra") {
            if (orchestra::has_help_flag(args)) {
                std::cout << "Usage: orchestration-agent orchestra [--dry-run]\n";
                return 0;
            }
            return orchestra::cmd_orchestra(paths, args, log_file);
        }

        std::cerr << "Unknown command: " << command << "\n";
        orchestra::print_usage();
        return 1;
    } catch (const std::exception& ex) {
        std::cerr << "orchestration-agent error: " << ex.what() << "\n";
        return 1;
    }
}
