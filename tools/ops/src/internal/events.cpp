#include "ops_internal.hpp"

#include <fstream>
#include <iostream>

#include "../../../../third_party/nlohmann/json.hpp"

namespace ops {

using json = nlohmann::json;

int cmd_events_diff(const RuntimePaths& paths, const std::vector<std::string>& args) {
    long long limit = 50;
    if (!args.empty()) {
        auto parsed = parse_int64(args[0]);
        if (!parsed.has_value() || *parsed <= 0) {
            std::cerr << "events_diff: limit must be a positive integer\n";
            return 1;
        }
        limit = *parsed;
    }

    fs::path queue_file = paths.backlog_dir / "events_queue.jsonl";
    if (auto env_queue = get_env_non_empty("EVENTS_QUEUE"); env_queue.has_value()) {
        queue_file = fs::path(*env_queue);
    }

    std::ifstream in(queue_file);
    if (!in) {
        std::cerr << "events_queue file not found: " << queue_file << "\n";
        return 1;
    }

    std::vector<std::string> lines;
    std::string line;
    while (std::getline(in, line)) {
        const std::string stripped = trim(line);
        if (!stripped.empty()) {
            lines.push_back(stripped);
        }
    }

    if (lines.empty()) {
        std::cout << "No events recorded yet.\n";
        return 0;
    }

    const std::size_t start =
        lines.size() > static_cast<std::size_t>(limit) ? (lines.size() - static_cast<std::size_t>(limit)) : 0;
    std::cout << "=== Last " << (lines.size() - start) << " events (tail of events_queue) ===\n";

    for (std::size_t i = start; i < lines.size(); ++i) {
        try {
            const json ev = json::parse(lines[i]);
            const std::string uid = ev.value("user_id", json(nullptr)).dump();
            const std::string login = ev.value("user_login", "");
            const std::string campus = ev.value("campus_id", json(nullptr)).dump();
            const std::string ts = ev.value("updated_at", "");
            const std::string primary = ev.value("primary_type", "");

            std::string types_repr = "[]";
            if (ev.contains("types")) {
                types_repr = ev["types"].dump();
            }

            std::cout << "- user=" << uid
                      << " (" << login << ")"
                      << " campus=" << campus
                      << " updated_at=" << ts
                      << " primary=" << primary
                      << " types=" << types_repr << "\n";

            if (ev.contains("changes") && ev["changes"].is_array()) {
                for (const auto& change : ev["changes"]) {
                    const std::string path = change.value("path", "");
                    std::string old_value = "null";
                    std::string new_value = "null";
                    if (change.contains("old")) {
                        old_value = change["old"].dump();
                    }
                    if (change.contains("new")) {
                        new_value = change["new"].dump();
                    }
                    std::cout << "    - " << path << ": " << old_value << " -> " << new_value << "\n";
                }
            }
        } catch (const std::exception&) {
            std::cout << "(invalid json) " << lines[i] << "\n";
        }
    }
    std::cout << "\n";
    return 0;
}

}  // namespace ops
