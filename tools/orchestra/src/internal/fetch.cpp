#include "orchestra_internal.hpp"

namespace orchestra {

int cmd_fetch_cursus_21_users_simple(const RuntimePaths& paths, const fs::path& log_file) {
    const fs::path exports_dir = paths.exports_root / "09_users";
    ensure_directory(exports_dir);
    ensure_directory(paths.logs_root);

    auto api_client_bin = resolve_binary(paths, "API42_CLIENT_BINARY", "/usr/local/bin/api42-client-agent");
    if (!api_client_bin.has_value()) {
        api_client_bin = resolve_binary(paths, "API_CLIENT_BINARY", "/usr/local/bin/api42-client-agent");
    }
    if (!api_client_bin.has_value()) {
        log_line(log_file, "api42-client-agent binary not found");
        return 1;
    }

    const fs::path output_file = exports_dir / "all.json";
    const fs::path raw_output_file = exports_dir / "raw_all.json";
    const fs::path temp_file = exports_dir / "all.json.tmp";
    const fs::path raw_temp_file = exports_dir / "raw_all.json.tmp";
    const fs::path last_fetch_file = exports_dir / ".last_fetch_epoch";
    const fs::path stats_file = exports_dir / ".last_fetch_stats";

    const std::string campus_raw = get_env_or_default("CAMPUS_ID", "21");
    auto campus_opt = parse_int64(campus_raw);
    if (!campus_opt.has_value()) {
        log_line(log_file, "CAMPUS_ID must be numeric for campus users fetch");
        return 1;
    }
    const long long campus_id = *campus_opt;
    const long long fetch_epoch = now_epoch();

    log_line(log_file, "Starting fetch_cursus_21_users_simple (campus endpoint)");
    log_line(log_file, "Campus ID: " + std::to_string(campus_id));

    const int per_page = 100;
    json raw = json::array();
    int page = 1;

    while (true) {
        std::ostringstream endpoint;
        endpoint << "/v2/campus/" << campus_id << "/users"
                 << "?filter%5Bkind%5D=student"
                 << "&filter%5Balumni%3F%5D=false"
                 << "&sort=id"
                 << "&per_page=" << per_page
                 << "&page=" << page;

        std::string cmd = shell_quote(api_client_bin->string()) + " call " + shell_quote(endpoint.str()) + " 2>/dev/null";
        int rc = 0;
        std::string body = run_bash_capture(cmd, &rc);
        body = trim(body);

        if (rc != 0 || body.empty()) {
            log_line(log_file, "Warning: request failed or empty response on page " + std::to_string(page) + ", stopping");
            break;
        }

        json page_json;
        try {
            page_json = json::parse(body);
        } catch (...) {
            log_line(log_file, "Warning: invalid JSON on page " + std::to_string(page) + ", stopping");
            break;
        }

        if (!page_json.is_array()) {
            log_line(log_file, "Warning: non-array response on page " + std::to_string(page) + ", stopping");
            break;
        }
        if (page_json.empty()) {
            break;
        }

        std::size_t raw_page_total = 0;
        for (const auto& user : page_json) {
            raw.push_back(user);
            raw_page_total += 1;
        }

        log_line(
            log_file,
            "Page " + std::to_string(page) +
                ": users=" + std::to_string(raw_page_total)
        );

        if (raw_page_total < static_cast<std::size_t>(per_page)) {
            break;
        }

        page += 1;
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    {
        std::ofstream out(temp_file, std::ios::out | std::ios::trunc);
        std::ofstream out_raw(raw_temp_file, std::ios::out | std::ios::trunc);
        if (!out || !out_raw) {
            log_line(log_file, "Cannot write temp output files");
            return 1;
        }
        out << raw.dump(2) << "\n";
        out_raw << raw.dump(2) << "\n";
    }

    std::error_code ec1;
    fs::rename(temp_file, output_file, ec1);
    if (ec1) {
        fs::remove(output_file, ec1);
        fs::rename(temp_file, output_file, ec1);
        if (ec1) {
            log_line(log_file, "Failed to move filtered output file");
            return 1;
        }
    }
    std::error_code ec2;
    fs::rename(raw_temp_file, raw_output_file, ec2);
    if (ec2) {
        fs::remove(raw_output_file, ec2);
        fs::rename(raw_temp_file, raw_output_file, ec2);
        if (ec2) {
            log_line(log_file, "Failed to move raw output file");
            return 1;
        }
    }

    {
        std::ofstream stamp(last_fetch_file, std::ios::out | std::ios::trunc);
        if (stamp) {
            stamp << fetch_epoch << "\n";
        }
    }
    {
        std::ofstream stats(stats_file, std::ios::out | std::ios::trunc);
        if (stats) {
            stats << "items=" << raw.size() << "\n";
            stats << "timestamp=" << utc_iso8601_from_epoch(fetch_epoch) << "\n";
            stats << "campus_id=" << campus_id << "\n";
        }
    }

    log_line(
        log_file,
        "Fetch complete: users=" + std::to_string(raw.size())
    );
    return 0;
}

int fetch_cursus_metadata_native(const RuntimePaths& paths, const fs::path& log_file, const fs::path& api_client_bin) {
    const std::string cursus_raw = get_env_or_default("CURSUS_ID", "21");
    auto cursus_opt = parse_int64(cursus_raw);
    if (!cursus_opt.has_value()) {
        log_line(log_file, "fetch_cursus: CURSUS_ID must be numeric");
        return 1;
    }
    const long long cursus_id = *cursus_opt;

    auto payload = call_api_json(
        api_client_bin,
        "/v2/cursus/" + std::to_string(cursus_id),
        log_file,
        "fetch_cursus"
    );
    if (!payload.has_value() || !payload->is_object()) {
        log_line(log_file, "fetch_cursus: expected object response");
        return 1;
    }

    const fs::path export_dir = paths.exports_root / "01_cursus";
    const fs::path all_file = export_dir / "all.json";
    ensure_directory(export_dir);

    json merged = json::array();
    merged.push_back(*payload);

    if (!write_json_atomic(all_file, merged, log_file)) {
        return 1;
    }

    const long long epoch = now_epoch();
    if (!write_epoch_and_stats(
            export_dir,
            epoch,
            {
                {"timestamp", std::to_string(epoch)},
                {"pages", "1"},
                {"items_merged", "1"},
                {"cursus_id", std::to_string(cursus_id)}
            },
            log_file
        )) {
        return 1;
    }

    log_line(log_file, "fetch_cursus: wrote " + all_file.string());
    return 0;
}

int fetch_campuses_metadata_native(const RuntimePaths& paths, const fs::path& log_file, const fs::path& api_client_bin) {
    const fs::path export_dir = paths.exports_root / "02_campus";
    const fs::path all_file = export_dir / "all.json";
    ensure_directory(export_dir);

    const int per_page = 100;
    int page = 1;
    std::size_t raw_total = 0;
    std::size_t filtered_total = 0;
    json merged = json::array();

    while (true) {
        std::ostringstream endpoint;
        endpoint << "/v2/campus"
                 << "?filter%5Bactive%5D=true"
                 << "&filter%5Bpublic%5D=true"
                 << "&per_page=" << per_page
                 << "&page=" << page;

        auto payload = call_api_json(api_client_bin, endpoint.str(), log_file, "fetch_campuses");
        if (!payload.has_value() || !payload->is_array()) {
            log_line(log_file, "fetch_campuses: expected array on page " + std::to_string(page));
            return 1;
        }

        const std::size_t raw_count = payload->size();
        raw_total += raw_count;

        std::size_t page_filtered = 0;
        for (const auto& campus : *payload) {
            if (!campus.is_object()) {
                continue;
            }
            long long users_count = 0;
            if (campus.contains("users_count")) {
                users_count = json_to_int64(campus["users_count"]).value_or(0);
            }
            if (users_count > 1) {
                merged.push_back(campus);
                filtered_total += 1;
                page_filtered += 1;
            }
        }

        log_line(
            log_file,
            "fetch_campuses: page " + std::to_string(page) +
                " raw=" + std::to_string(raw_count) +
                " filtered=" + std::to_string(page_filtered)
        );

        if (raw_count < static_cast<std::size_t>(per_page)) {
            break;
        }
        page += 1;
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    if (!write_json_atomic(all_file, merged, log_file)) {
        return 1;
    }

    const long long epoch = now_epoch();
    if (!write_epoch_and_stats(
            export_dir,
            epoch,
            {
                {"timestamp", std::to_string(epoch)},
                {"pages", std::to_string(page)},
                {"items_raw", std::to_string(raw_total)},
                {"items_filtered", std::to_string(filtered_total)},
                {"items_merged", std::to_string(merged.size())}
            },
            log_file
        )) {
        return 1;
    }

    log_line(log_file, "fetch_campuses: wrote " + all_file.string());
    return 0;
}

int fetch_projects_metadata_native(const RuntimePaths& paths, const fs::path& log_file, const fs::path& api_client_bin) {
    const std::string cursus_raw = get_env_or_default("CURSUS_ID", "21");
    auto cursus_opt = parse_int64(cursus_raw);
    if (!cursus_opt.has_value()) {
        log_line(log_file, "fetch_projects: CURSUS_ID must be numeric");
        return 1;
    }
    const long long cursus_id = *cursus_opt;

    const fs::path projects_dir = paths.exports_root / "05_projects";
    const fs::path raw_all_file = projects_dir / "raw_all.json";
    ensure_directory(projects_dir);

    const int per_page = 100;
    int page = 1;
    std::size_t total_items = 0;
    json raw_projects = json::array();

    while (true) {
        std::ostringstream endpoint;
        endpoint << "/v2/cursus/" << cursus_id << "/projects"
                 << "?per_page=" << per_page
                 << "&page=" << page;

        auto payload = call_api_json(api_client_bin, endpoint.str(), log_file, "fetch_projects");
        if (!payload.has_value() || !payload->is_array()) {
            log_line(log_file, "fetch_projects: expected array on page " + std::to_string(page));
            return 1;
        }

        const std::size_t page_count = payload->size();
        total_items += page_count;
        for (const auto& item : *payload) {
            raw_projects.push_back(item);
        }

        log_line(
            log_file,
            "fetch_projects: page " + std::to_string(page) +
                " items=" + std::to_string(page_count)
        );

        if (page_count < static_cast<std::size_t>(per_page)) {
            break;
        }
        page += 1;
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    if (!write_json_atomic(raw_all_file, raw_projects, log_file)) {
        return 1;
    }

    const long long epoch = now_epoch();
    if (!write_epoch_and_stats(
            projects_dir,
            epoch,
            {
                {"timestamp", std::to_string(epoch)},
                {"cursus_id", std::to_string(cursus_id)},
                {"pages", std::to_string(page)},
                {"items_merged", std::to_string(raw_projects.size())}
            },
            log_file
        )) {
        return 1;
    }

    json campus_projects = json::array();
    json project_sessions = json::array();
    for (const auto& project : raw_projects) {
        if (!project.is_object()) {
            continue;
        }

        const auto project_id_opt = project.contains("id") ? json_to_int64(project["id"]) : std::nullopt;

        if (project.contains("campus") && project["campus"].is_array() && project_id_opt.has_value()) {
            for (const auto& campus : project["campus"]) {
                if (!campus.is_object() || !campus.contains("id")) {
                    continue;
                }
                const auto campus_id_opt = json_to_int64(campus["id"]);
                if (!campus_id_opt.has_value()) {
                    continue;
                }

                json link = json::object();
                link["project_id"] = *project_id_opt;
                link["campus_id"] = *campus_id_opt;
                campus_projects.push_back(link);
            }
        }

        if (project.contains("project_sessions") && project["project_sessions"].is_array()) {
            for (const auto& session : project["project_sessions"]) {
                project_sessions.push_back(session);
            }
        }
    }

    const fs::path campus_projects_dir = paths.exports_root / "06_campus_projects";
    const fs::path project_sessions_dir = paths.exports_root / "07_project_sessions";
    ensure_directory(campus_projects_dir);
    ensure_directory(project_sessions_dir);

    if (!write_json_atomic(campus_projects_dir / "all.json", campus_projects, log_file)) {
        return 1;
    }
    if (!write_json_atomic(project_sessions_dir / "all.json", project_sessions, log_file)) {
        return 1;
    }

    if (!write_epoch_and_stats(
            campus_projects_dir,
            epoch,
            {
                {"timestamp", std::to_string(epoch)},
                {"type", "campus_projects"},
                {"count", std::to_string(campus_projects.size())},
                {"source", "05_projects"}
            },
            log_file
        )) {
        return 1;
    }
    if (!write_epoch_and_stats(
            project_sessions_dir,
            epoch,
            {
                {"timestamp", std::to_string(epoch)},
                {"type", "project_sessions"},
                {"count", std::to_string(project_sessions.size())},
                {"source", "05_projects"}
            },
            log_file
        )) {
        return 1;
    }

    log_line(log_file, "fetch_projects: wrote " + raw_all_file.string());
    log_line(log_file, "extract_campus_projects: count=" + std::to_string(campus_projects.size()));
    log_line(log_file, "extract_project_sessions: count=" + std::to_string(project_sessions.size()));
    (void)total_items;
    return 0;
}

int fetch_coalitions_metadata_native(const RuntimePaths& paths, const fs::path& log_file, const fs::path& api_client_bin) {
    const fs::path export_dir = paths.exports_root / "08_coalitions";
    const fs::path all_file = export_dir / "all.json";
    ensure_directory(export_dir);

    const int per_page = 100;
    int page = 1;
    std::size_t total_items = 0;
    json merged = json::array();

    while (true) {
        std::ostringstream endpoint;
        endpoint << "/v2/coalitions?per_page=" << per_page << "&page=" << page;

        auto payload = call_api_json(api_client_bin, endpoint.str(), log_file, "fetch_coalitions");
        if (!payload.has_value() || !payload->is_array()) {
            log_line(log_file, "fetch_coalitions: expected array on page " + std::to_string(page));
            return 1;
        }

        const std::size_t page_count = payload->size();
        total_items += page_count;
        for (const auto& item : *payload) {
            merged.push_back(item);
        }

        log_line(
            log_file,
            "fetch_coalitions: page " + std::to_string(page) +
                " items=" + std::to_string(page_count)
        );

        if (page_count < static_cast<std::size_t>(per_page)) {
            break;
        }
        page += 1;
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    if (!write_json_atomic(all_file, merged, log_file)) {
        return 1;
    }

    const long long epoch = now_epoch();
    if (!write_epoch_and_stats(
            export_dir,
            epoch,
            {
                {"timestamp", std::to_string(epoch)},
                {"type", "coalitions"},
                {"pages", std::to_string(page)},
                {"count", std::to_string(total_items)},
                {"source", "/v2/coalitions"}
            },
            log_file
        )) {
        return 1;
    }

    log_line(log_file, "fetch_coalitions: wrote " + all_file.string());
    return 0;
}

int fetch_campus_achievements_metadata_native(
    const RuntimePaths& paths,
    const fs::path& log_file,
    const fs::path& api_client_bin
) {
    const std::string campus_raw = get_env_or_default("CAMPUS_ID", "21");
    auto campus_opt = parse_int64(campus_raw);
    if (!campus_opt.has_value()) {
        log_line(log_file, "fetch_campus_achievements: CAMPUS_ID must be numeric");
        return 1;
    }
    const long long campus_id = *campus_opt;

    const fs::path root_dir = paths.exports_root / "04_campus_achievements";
    const fs::path campus_dir = root_dir / ("campus_" + std::to_string(campus_id));
    const fs::path all_file = campus_dir / "all.json";
    ensure_directory(campus_dir);

    const int per_page = 100;
    int page = 1;
    std::size_t total_items = 0;
    json merged = json::array();

    while (true) {
        std::ostringstream endpoint;
        endpoint << "/v2/campus/" << campus_id << "/achievements"
                 << "?per_page=" << per_page
                 << "&page=" << page;

        auto payload = call_api_json(api_client_bin, endpoint.str(), log_file, "fetch_campus_achievements");
        if (!payload.has_value() || !payload->is_array()) {
            log_line(
                log_file,
                "fetch_campus_achievements: expected array on page " + std::to_string(page)
            );
            return 1;
        }

        const std::size_t page_count = payload->size();
        total_items += page_count;

        for (const auto& item : *payload) {
            if (!item.is_object()) {
                continue;
            }
            json enriched = item;
            enriched["campus_id"] = campus_id;
            merged.push_back(enriched);
        }

        log_line(
            log_file,
            "fetch_campus_achievements: page " + std::to_string(page) +
                " items=" + std::to_string(page_count)
        );

        if (page_count < static_cast<std::size_t>(per_page)) {
            break;
        }
        page += 1;
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    if (!write_json_atomic(all_file, merged, log_file)) {
        return 1;
    }

    const long long epoch = now_epoch();
    if (!write_epoch_and_stats(
            root_dir,
            epoch,
            {
                {"raw", std::to_string(total_items)},
                {"pages", std::to_string(page)},
                {"campus_id", std::to_string(campus_id)}
            },
            log_file
        )) {
        return 1;
    }

    log_line(log_file, "fetch_campus_achievements: wrote " + all_file.string());
    return 0;
}

int merge_campus_achievements_native(const RuntimePaths& paths, const fs::path& log_file) {
    const fs::path campus_root = paths.exports_root / "04_campus_achievements";
    const fs::path norm_root = paths.exports_root / "03_achievements";
    ensure_directory(campus_root);
    ensure_directory(norm_root);

    std::vector<fs::path> source_files;
    for (const auto& entry : fs::directory_iterator(campus_root)) {
        if (!entry.is_directory()) {
            continue;
        }
        const std::string name = entry.path().filename().string();
        if (name.rfind("campus_", 0) != 0) {
            continue;
        }
        const fs::path candidate = entry.path() / "all.json";
        if (fs::exists(candidate)) {
            source_files.push_back(candidate);
        }
    }
    std::sort(source_files.begin(), source_files.end());

    if (source_files.empty()) {
        log_line(log_file, "merge_campus_achievements: no campus_*/all.json found");
        return 1;
    }

    json merged = json::array();
    for (const auto& file : source_files) {
        json part;
        if (!read_json_file(file, &part, log_file)) {
            return 1;
        }
        if (!part.is_array()) {
            log_line(log_file, "merge_campus_achievements: expected array in " + file.string());
            return 1;
        }
        for (const auto& item : part) {
            merged.push_back(item);
        }
    }

    const fs::path raw_all_file = campus_root / "raw_all.json";
    if (!write_json_atomic(raw_all_file, merged, log_file)) {
        return 1;
    }

    json normalized = json::array();
    std::set<std::string> seen_ids;
    for (const auto& item : merged) {
        if (!item.is_object() || !item.contains("id") || item["id"].is_null()) {
            continue;
        }
        const std::string id_key = item["id"].dump();
        if (!seen_ids.insert(id_key).second) {
            continue;
        }
        json clean = item;
        clean.erase("campus_id");
        normalized.push_back(clean);
    }

    const fs::path normalized_file = norm_root / "all.json";
    if (!write_json_atomic(normalized_file, normalized, log_file)) {
        return 1;
    }

    const long long epoch = now_epoch();
    if (!write_epoch_and_stats(
            campus_root,
            epoch,
            {
                {"type", "campus_achievements"},
                {"count", std::to_string(merged.size())},
                {"campuses", std::to_string(source_files.size())},
                {"method", "merge_from_campus_dirs"},
                {"timestamp", std::to_string(epoch)}
            },
            log_file
        )) {
        return 1;
    }

    if (!write_epoch_and_stats(
            norm_root,
            epoch,
            {
                {"type", "achievements"},
                {"count", std::to_string(normalized.size())},
                {"method", "deduplicate_from_campus_achievements"},
                {"source", "04_campus_achievements"},
                {"timestamp", std::to_string(epoch)}
            },
            log_file
        )) {
        return 1;
    }

    log_line(
        log_file,
        "merge_campus_achievements: merged=" + std::to_string(merged.size()) +
            " normalized=" + std::to_string(normalized.size())
    );
    return 0;
}

int cmd_fetch_metadata(const RuntimePaths& paths, const fs::path& log_file) {
    auto api_client_bin = resolve_binary(paths, "API42_CLIENT_BINARY", "/usr/local/bin/api42-client-agent");
    if (!api_client_bin.has_value()) {
        api_client_bin = resolve_binary(paths, "API_CLIENT_BINARY", "/usr/local/bin/api42-client-agent");
    }
    if (!api_client_bin.has_value()) {
        log_line(log_file, "api42-client-agent binary not found");
        return 1;
    }

    const std::string campus_id = get_env_or_default("CAMPUS_ID", "21");
    const std::string cursus_id = get_env_or_default("CURSUS_ID", "21");
    log_line(
        log_file,
        "Fetching metadata (campus=" + campus_id + ", cursus=" + cursus_id + ")"
    );

    int failures = 0;
    auto run_step = [&](const std::string& name, int (*fn)(const RuntimePaths&, const fs::path&, const fs::path&)) {
        const int rc = fn(paths, log_file, *api_client_bin);
        if (rc == 0) {
            log_line(log_file, "Step complete: " + name);
        } else {
            log_line(log_file, "Step failed: " + name);
            failures += 1;
        }
    };

    run_step("fetch_cursus", fetch_cursus_metadata_native);
    run_step("fetch_campuses", fetch_campuses_metadata_native);
    run_step("fetch_projects", fetch_projects_metadata_native);
    run_step("fetch_coalitions", fetch_coalitions_metadata_native);
    run_step("fetch_campus_achievements", fetch_campus_achievements_metadata_native);

    if (merge_campus_achievements_native(paths, log_file) == 0) {
        log_line(log_file, "Step complete: merge_campus_achievements");
    } else {
        log_line(log_file, "Step failed: merge_campus_achievements");
        failures += 1;
    }

    if (failures > 0) {
        log_line(log_file, "Metadata fetch completed with failures");
        return 1;
    }
    log_line(log_file, "Metadata fetch completed");
    return 0;
}

int cmd_fetch_users(const RuntimePaths& paths, const fs::path& log_file, const std::vector<std::string>& args) {
    bool dry_run = false;
    bool internal_only = false;
    for (const auto& arg : args) {
        if (arg == "--dry-run") {
            dry_run = true;
        } else if (arg == "--internal-only") {
            internal_only = true;
        } else {
            log_line(log_file, "Unknown fetch_users arg: " + arg);
            return 1;
        }
    }

    const std::string campus_raw = get_env_or_default("CAMPUS_ID", "76");
    std::string campus_upper = campus_raw;
    std::transform(campus_upper.begin(), campus_upper.end(), campus_upper.begin(), [](unsigned char c) {
        return static_cast<char>(std::toupper(c));
    });
    const bool all_mode = (campus_upper == "ALL");

    if (internal_only && all_mode) {
        log_line(log_file, "Internal-only users fetch requires numeric CAMPUS_ID (not ALL)");
        return 1;
    }

    if (cmd_fetch_cursus_21_users_simple(paths, log_file) != 0) {
        log_line(log_file, "fetch_cursus_21_users_simple failed");
        return 1;
    }

    const fs::path source_file = paths.exports_root / "09_users" / "all.json";
    if (!fs::exists(source_file)) {
        log_line(log_file, "Missing source users file: " + source_file.string());
        return 1;
    }

    json users;
    {
        std::ifstream in(source_file);
        if (!in) {
            log_line(log_file, "Cannot open source users file");
            return 1;
        }
        try {
            in >> users;
        } catch (const std::exception& ex) {
            log_line(log_file, std::string("Invalid users JSON: ") + ex.what());
            return 1;
        }
    }

    if (!users.is_array()) {
        log_line(log_file, "Source users JSON is not an array");
        return 1;
    }

    fs::path target_dir;
    if (all_mode) {
        target_dir = paths.exports_root / "09_users" / "campus_all";
    } else {
        auto campus_opt = parse_int64(campus_raw);
        if (!campus_opt.has_value()) {
            log_line(log_file, "CAMPUS_ID must be numeric or ALL");
            return 1;
        }
        const long long campus_id = *campus_opt;
        target_dir = paths.exports_root / "09_users" / ("campus_" + std::to_string(campus_id));
    }

    ensure_directory(target_dir);
    const fs::path target_file = target_dir / "all.json";
    if (dry_run) {
        log_line(log_file, "Dry run: would write " + std::to_string(users.size()) + " users to " + target_file.string());
        return 0;
    }

    {
        std::ofstream out(target_file, std::ios::out | std::ios::trunc);
        if (!out) {
            log_line(log_file, "Cannot write target users file");
            return 1;
        }
        out << users.dump(2);
    }

    std::ofstream stamp(target_dir / ".last_fetch_epoch", std::ios::out | std::ios::trunc);
    if (stamp) {
        stamp << now_epoch() << "\n";
    }

    log_line(log_file, "Users fetch complete: " + std::to_string(users.size()) + " users -> " + target_file.string());
    return 0;
}

} // namespace orchestra
