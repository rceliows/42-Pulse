#include "orchestra_internal.hpp"

namespace orchestra {
namespace {

const json* find_path(const json& root, std::initializer_list<const char*> path) {
    const json* current = &root;
    for (const char* key : path) {
        if (!current->is_object()) {
            return nullptr;
        }
        auto it = current->find(key);
        if (it == current->end()) {
            return nullptr;
        }
        current = &(*it);
    }
    return current;
}

const json* first_non_null_path(
    const json& root,
    std::initializer_list<std::initializer_list<const char*>> paths
) {
    for (const auto& path : paths) {
        const json* value = find_path(root, path);
        if (value && !value->is_null()) {
            return value;
        }
    }
    return nullptr;
}

std::optional<long long> json_to_i64_ptr(const json* value) {
    if (!value || value->is_null()) {
        return std::nullopt;
    }
    return json_to_int64(*value);
}

std::optional<bool> json_to_bool_ptr(const json* value) {
    if (!value || value->is_null()) {
        return std::nullopt;
    }
    if (value->is_boolean()) {
        return value->get<bool>();
    }
    if (value->is_number_integer() || value->is_number_unsigned()) {
        return value->get<long long>() != 0;
    }
    if (value->is_string()) {
        std::string text = trim(value->get<std::string>());
        std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        if (text == "true" || text == "1" || text == "yes" || text == "y") {
            return true;
        }
        if (text == "false" || text == "0" || text == "no" || text == "n") {
            return false;
        }
    }
    return std::nullopt;
}

std::optional<std::string> json_to_text_ptr(const json* value) {
    if (!value || value->is_null()) {
        return std::nullopt;
    }
    if (value->is_string()) {
        return value->get<std::string>();
    }
    if (value->is_boolean()) {
        return value->get<bool>() ? "true" : "false";
    }
    if (value->is_number_integer()) {
        return std::to_string(value->get<long long>());
    }
    if (value->is_number_unsigned()) {
        return std::to_string(value->get<unsigned long long>());
    }
    if (value->is_number_float()) {
        std::ostringstream out;
        out << value->get<double>();
        return out.str();
    }
    return value->dump();
}

std::string sql_escape(const std::string& value) {
    std::string out;
    out.reserve(value.size() + 8);
    for (char c : value) {
        if (c == '\'') {
            out += "''";
        } else if (c != '\0') {
            out.push_back(c);
        }
    }
    return out;
}

std::string sql_text_literal(const json* value) {
    auto text = json_to_text_ptr(value);
    if (!text.has_value()) {
        return "NULL";
    }
    return "'" + sql_escape(*text) + "'";
}

std::string sql_color_literal(const json* value) {
    auto text = json_to_text_ptr(value);
    if (!text.has_value()) {
        return "NULL";
    }

    std::string color = trim(*text);
    if (color.empty()) {
        return "NULL";
    }

    if (color.rfind("0x", 0) == 0 || color.rfind("0X", 0) == 0) {
        color = "#" + color.substr(2);
    }

    if (!color.empty() && color.front() == '#') {
        if (color.size() > 7) {
            color = color.substr(0, 7);
        }
    } else if (color.size() >= 6) {
        color = "#" + color.substr(0, 6);
    } else {
        return "NULL";
    }

    if (color.size() != 7 || color.front() != '#') {
        return "NULL";
    }
    for (std::size_t i = 1; i < color.size(); ++i) {
        if (!std::isxdigit(static_cast<unsigned char>(color[i]))) {
            return "NULL";
        }
        color[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(color[i])));
    }

    return "'" + sql_escape(color) + "'";
}

std::string sql_int_literal(const json* value) {
    auto number = json_to_i64_ptr(value);
    if (!number.has_value()) {
        return "NULL";
    }
    return std::to_string(*number);
}

std::string sql_bool_literal(const json* value) {
    auto b = json_to_bool_ptr(value);
    if (!b.has_value()) {
        return "NULL";
    }
    return *b ? "TRUE" : "FALSE";
}

std::string sql_timestamptz_literal(const json* value) {
    auto text = json_to_text_ptr(value);
    if (!text.has_value() || trim(*text).empty()) {
        return "NULL";
    }
    return "'" + sql_escape(*text) + "'::timestamptz";
}

std::string sql_jsonb_literal(const json* value) {
    if (!value || value->is_null()) {
        return "NULL";
    }
    return "'" + sql_escape(value->dump()) + "'::jsonb";
}

bool read_array_file(const fs::path& path, json* out, const fs::path& log_file) {
    if (!fs::exists(path)) {
        log_line(log_file, "Missing export file: " + path.string());
        return false;
    }
    if (!read_json_file(path, out, log_file)) {
        return false;
    }
    if (!out->is_array()) {
        log_line(log_file, "Expected JSON array in: " + path.string());
        return false;
    }
    return true;
}

int run_sql_script(
    const RuntimePaths& paths,
    const fs::path& log_file,
    const std::string& tag,
    const std::string& sql
) {
    static unsigned long long counter = 0;
    counter += 1;
    const fs::path tmp_dir = paths.runtime_dir / "tmp";
    ensure_directory(tmp_dir);

    const fs::path sql_file =
        tmp_dir / ("orchestra_" + tag + "_" + std::to_string(now_epoch()) + "_" + std::to_string(counter) + ".sql");
    if (!write_text_atomic(sql_file, sql, log_file)) {
        return 1;
    }

    const std::string cmd =
        "docker exec -i transcendence_db psql -U api42 -d api42 -v ON_ERROR_STOP=1 < " +
        shell_quote(sql_file.string());
    const int rc = run_logged(cmd, log_file);
    std::error_code ec;
    fs::remove(sql_file, ec);
    return rc;
}

int flush_insert_batch(
    const RuntimePaths& paths,
    const fs::path& log_file,
    const std::string& tag,
    const std::string& insert_prefix,
    const std::string& conflict_clause,
    std::vector<std::string>* values
) {
    if (!values || values->empty()) {
        return 0;
    }

    std::ostringstream sql;
    sql << "BEGIN;\n" << insert_prefix << "\n";
    for (std::size_t i = 0; i < values->size(); ++i) {
        sql << values->at(i);
        if (i + 1 < values->size()) {
            sql << ",\n";
        } else {
            sql << "\n";
        }
    }
    if (conflict_clause.empty()) {
        sql << ";\n";
    } else {
        sql << conflict_clause << ";\n";
    }
    sql << "COMMIT;\n";

    const int rc = run_sql_script(paths, log_file, tag, sql.str());
    values->clear();
    return rc;
}

int load_cursus_table(const RuntimePaths& paths, const fs::path& log_file) {
    json rows;
    if (!read_array_file(paths.exports_root / "01_cursus" / "all.json", &rows, log_file)) {
        return 1;
    }

    std::set<long long> seen_ids;
    std::vector<std::string> values;
    std::size_t loaded = 0;

    const std::string insert_prefix =
        "INSERT INTO cursus (id, name, slug, kind, created_at, ingested_at) VALUES";
    const std::string conflict_clause =
        "ON CONFLICT (id) DO UPDATE SET "
        "name = EXCLUDED.name, slug = EXCLUDED.slug, kind = EXCLUDED.kind, "
        "created_at = EXCLUDED.created_at, ingested_at = NOW()";

    for (const auto& row : rows) {
        if (!row.is_object()) {
            continue;
        }
        auto id = json_to_i64_ptr(find_path(row, {"id"}));
        if (!id.has_value() || !seen_ids.insert(*id).second) {
            continue;
        }

        std::ostringstream tuple;
        tuple << "("
              << *id << ","
              << sql_text_literal(find_path(row, {"name"})) << ","
              << sql_text_literal(find_path(row, {"slug"})) << ","
              << sql_text_literal(find_path(row, {"kind"})) << ","
              << sql_timestamptz_literal(find_path(row, {"created_at"})) << ","
              << "NOW())";
        values.push_back(tuple.str());
        loaded += 1;

        if (values.size() >= 250) {
            if (flush_insert_batch(paths, log_file, "load_cursus", insert_prefix, conflict_clause, &values) != 0) {
                return 1;
            }
        }
    }

    if (flush_insert_batch(paths, log_file, "load_cursus", insert_prefix, conflict_clause, &values) != 0) {
        return 1;
    }

    log_line(log_file, "load_metadata: cursus loaded=" + std::to_string(loaded));
    return 0;
}

int load_campuses_table(const RuntimePaths& paths, const fs::path& log_file) {
    json rows;
    if (!read_array_file(paths.exports_root / "02_campus" / "all.json", &rows, log_file)) {
        return 1;
    }

    std::set<long long> seen_ids;
    std::vector<std::string> values;
    std::size_t loaded = 0;

    const std::string insert_prefix =
        "INSERT INTO campuses ("
        "id, name, time_zone, language_id, language_name, language_identifier, users_count, "
        "vogsphere_id, country, address, zip, city, website, facebook, twitter, public, active, "
        "email_extension, default_hidden_phone, ingested_at"
        ") VALUES";
    const std::string conflict_clause =
        "ON CONFLICT (id) DO UPDATE SET "
        "name = EXCLUDED.name, time_zone = EXCLUDED.time_zone, language_id = EXCLUDED.language_id, "
        "language_name = EXCLUDED.language_name, language_identifier = EXCLUDED.language_identifier, "
        "users_count = EXCLUDED.users_count, vogsphere_id = EXCLUDED.vogsphere_id, "
        "country = EXCLUDED.country, address = EXCLUDED.address, zip = EXCLUDED.zip, city = EXCLUDED.city, "
        "website = EXCLUDED.website, facebook = EXCLUDED.facebook, twitter = EXCLUDED.twitter, "
        "public = EXCLUDED.public, active = EXCLUDED.active, email_extension = EXCLUDED.email_extension, "
        "default_hidden_phone = EXCLUDED.default_hidden_phone, ingested_at = NOW()";

    for (const auto& row : rows) {
        if (!row.is_object()) {
            continue;
        }
        auto id = json_to_i64_ptr(find_path(row, {"id"}));
        if (!id.has_value() || !seen_ids.insert(*id).second) {
            continue;
        }
        std::ostringstream tuple;
        tuple << "("
              << *id << ","
              << sql_text_literal(find_path(row, {"name"})) << ","
              << sql_text_literal(find_path(row, {"time_zone"})) << ","
              << sql_int_literal(first_non_null_path(row, {{"language", "id"}, {"language_id"}})) << ","
              << sql_text_literal(first_non_null_path(row, {{"language", "name"}, {"language_name"}})) << ","
              << sql_text_literal(first_non_null_path(row, {{"language", "identifier"}, {"language_identifier"}})) << ","
              << sql_int_literal(find_path(row, {"users_count"})) << ","
              << sql_int_literal(find_path(row, {"vogsphere_id"})) << ","
              << sql_text_literal(find_path(row, {"country"})) << ","
              << sql_text_literal(find_path(row, {"address"})) << ","
              << sql_text_literal(find_path(row, {"zip"})) << ","
              << sql_text_literal(find_path(row, {"city"})) << ","
              << sql_text_literal(find_path(row, {"website"})) << ","
              << sql_text_literal(find_path(row, {"facebook"})) << ","
              << sql_text_literal(find_path(row, {"twitter"})) << ","
              << sql_bool_literal(find_path(row, {"public"})) << ","
              << sql_bool_literal(find_path(row, {"active"})) << ","
              << sql_text_literal(find_path(row, {"email_extension"})) << ","
              << sql_bool_literal(find_path(row, {"default_hidden_phone"})) << ","
              << "NOW())";
        values.push_back(tuple.str());
        loaded += 1;

        if (values.size() >= 200) {
            if (flush_insert_batch(paths, log_file, "load_campuses", insert_prefix, conflict_clause, &values) != 0) {
                return 1;
            }
        }
    }

    if (flush_insert_batch(paths, log_file, "load_campuses", insert_prefix, conflict_clause, &values) != 0) {
        return 1;
    }

    log_line(log_file, "load_metadata: campuses loaded=" + std::to_string(loaded));
    return 0;
}

int load_projects_table(const RuntimePaths& paths, const fs::path& log_file) {
    json rows;
    if (!read_array_file(paths.exports_root / "05_projects" / "raw_all.json", &rows, log_file)) {
        return 1;
    }

    std::set<long long> seen_ids;
    std::vector<std::string> values;
    std::size_t loaded = 0;

    const std::string insert_prefix =
        "INSERT INTO projects ("
        "id, name, slug, parent_id, difficulty, exam, git_id, repository, recommendation, "
        "created_at, updated_at, ingested_at"
        ") VALUES";
    const std::string conflict_clause =
        "ON CONFLICT (id) DO UPDATE SET "
        "name = EXCLUDED.name, slug = EXCLUDED.slug, parent_id = EXCLUDED.parent_id, "
        "difficulty = EXCLUDED.difficulty, exam = EXCLUDED.exam, git_id = EXCLUDED.git_id, "
        "repository = EXCLUDED.repository, recommendation = EXCLUDED.recommendation, "
        "created_at = EXCLUDED.created_at, updated_at = EXCLUDED.updated_at, ingested_at = NOW()";

    for (const auto& row : rows) {
        if (!row.is_object()) {
            continue;
        }
        auto id = json_to_i64_ptr(find_path(row, {"id"}));
        if (!id.has_value() || !seen_ids.insert(*id).second) {
            continue;
        }

        std::ostringstream tuple;
        tuple << "("
              << *id << ","
              << sql_text_literal(find_path(row, {"name"})) << ","
              << sql_text_literal(find_path(row, {"slug"})) << ","
              << sql_int_literal(find_path(row, {"parent_id"})) << ","
              << sql_int_literal(find_path(row, {"difficulty"})) << ","
              << sql_bool_literal(find_path(row, {"exam"})) << ","
              << sql_int_literal(find_path(row, {"git_id"})) << ","
              << sql_text_literal(find_path(row, {"repository"})) << ","
              << sql_text_literal(find_path(row, {"recommendation"})) << ","
              << sql_timestamptz_literal(find_path(row, {"created_at"})) << ","
              << sql_timestamptz_literal(find_path(row, {"updated_at"})) << ","
              << "NOW())";
        values.push_back(tuple.str());
        loaded += 1;

        if (values.size() >= 200) {
            if (flush_insert_batch(paths, log_file, "load_projects", insert_prefix, conflict_clause, &values) != 0) {
                return 1;
            }
        }
    }

    if (flush_insert_batch(paths, log_file, "load_projects", insert_prefix, conflict_clause, &values) != 0) {
        return 1;
    }

    log_line(log_file, "load_metadata: projects loaded=" + std::to_string(loaded));
    return 0;
}

int load_campus_projects_table(const RuntimePaths& paths, const fs::path& log_file) {
    json rows;
    if (!read_array_file(paths.exports_root / "06_campus_projects" / "all.json", &rows, log_file)) {
        return 1;
    }

    std::set<std::string> seen_keys;
    std::vector<std::string> values;
    std::size_t loaded = 0;

    const std::string insert_prefix =
        "INSERT INTO campus_projects (campus_id, project_id, ingested_at) VALUES";
    const std::string conflict_clause =
        "ON CONFLICT (campus_id, project_id) DO UPDATE SET ingested_at = NOW()";

    for (const auto& row : rows) {
        if (!row.is_object()) {
            continue;
        }
        auto campus_id = json_to_i64_ptr(first_non_null_path(row, {{"campus_id"}, {"campus", "id"}}));
        auto project_id = json_to_i64_ptr(first_non_null_path(row, {{"project_id"}, {"project", "id"}}));
        if (!campus_id.has_value() || !project_id.has_value()) {
            continue;
        }
        const std::string key = std::to_string(*campus_id) + ":" + std::to_string(*project_id);
        if (!seen_keys.insert(key).second) {
            continue;
        }

        std::ostringstream tuple;
        tuple << "(" << *campus_id << "," << *project_id << ",NOW())";
        values.push_back(tuple.str());
        loaded += 1;

        if (values.size() >= 500) {
            if (flush_insert_batch(paths, log_file, "load_campus_projects", insert_prefix, conflict_clause, &values) != 0) {
                return 1;
            }
        }
    }

    if (flush_insert_batch(paths, log_file, "load_campus_projects", insert_prefix, conflict_clause, &values) != 0) {
        return 1;
    }

    log_line(log_file, "load_metadata: campus_projects loaded=" + std::to_string(loaded));
    return 0;
}

int load_project_sessions_table(const RuntimePaths& paths, const fs::path& log_file) {
    json rows;
    if (!read_array_file(paths.exports_root / "07_project_sessions" / "all.json", &rows, log_file)) {
        return 1;
    }

    std::set<long long> seen_ids;
    std::vector<std::string> values;
    std::size_t loaded = 0;

    const std::string insert_prefix =
        "INSERT INTO project_sessions ("
        "id, project_id, campus_id, cursus_id, begin_at, end_at, difficulty, estimate_time, exam, marked, "
        "max_project_submissions, max_people, duration_days, commit, description, is_subscriptable, objectives, "
        "scales, terminating_after, uploads, solo, team_behaviour, created_at, updated_at, ingested_at"
        ") VALUES";
    const std::string conflict_clause =
        "ON CONFLICT (id) DO UPDATE SET "
        "project_id = EXCLUDED.project_id, campus_id = EXCLUDED.campus_id, cursus_id = EXCLUDED.cursus_id, "
        "begin_at = EXCLUDED.begin_at, end_at = EXCLUDED.end_at, difficulty = EXCLUDED.difficulty, "
        "estimate_time = EXCLUDED.estimate_time, exam = EXCLUDED.exam, marked = EXCLUDED.marked, "
        "max_project_submissions = EXCLUDED.max_project_submissions, max_people = EXCLUDED.max_people, "
        "duration_days = EXCLUDED.duration_days, commit = EXCLUDED.commit, description = EXCLUDED.description, "
        "is_subscriptable = EXCLUDED.is_subscriptable, objectives = EXCLUDED.objectives, "
        "scales = EXCLUDED.scales, terminating_after = EXCLUDED.terminating_after, uploads = EXCLUDED.uploads, "
        "solo = EXCLUDED.solo, team_behaviour = EXCLUDED.team_behaviour, "
        "created_at = EXCLUDED.created_at, updated_at = EXCLUDED.updated_at, ingested_at = NOW()";

    for (const auto& row : rows) {
        if (!row.is_object()) {
            continue;
        }
        auto id = json_to_i64_ptr(find_path(row, {"id"}));
        if (!id.has_value() || !seen_ids.insert(*id).second) {
            continue;
        }

        std::ostringstream tuple;
        tuple << "("
              << *id << ","
              << sql_int_literal(first_non_null_path(row, {{"project_id"}, {"project", "id"}})) << ","
              << sql_int_literal(first_non_null_path(row, {{"campus_id"}, {"campus", "id"}})) << ","
              << sql_int_literal(first_non_null_path(row, {{"cursus_id"}, {"cursus", "id"}})) << ","
              << sql_timestamptz_literal(find_path(row, {"begin_at"})) << ","
              << sql_timestamptz_literal(find_path(row, {"end_at"})) << ","
              << sql_int_literal(find_path(row, {"difficulty"})) << ","
              << sql_text_literal(find_path(row, {"estimate_time"})) << ","
              << sql_bool_literal(find_path(row, {"exam"})) << ","
              << sql_bool_literal(find_path(row, {"marked"})) << ","
              << sql_int_literal(find_path(row, {"max_project_submissions"})) << ","
              << sql_int_literal(find_path(row, {"max_people"})) << ","
              << sql_int_literal(find_path(row, {"duration_days"})) << ","
              << sql_text_literal(find_path(row, {"commit"})) << ","
              << sql_text_literal(find_path(row, {"description"})) << ","
              << sql_bool_literal(find_path(row, {"is_subscriptable"})) << ","
              << sql_jsonb_literal(find_path(row, {"objectives"})) << ","
              << sql_jsonb_literal(find_path(row, {"scales"})) << ","
              << sql_text_literal(find_path(row, {"terminating_after"})) << ","
              << sql_jsonb_literal(find_path(row, {"uploads"})) << ","
              << sql_bool_literal(find_path(row, {"solo"})) << ","
              << sql_text_literal(find_path(row, {"team_behaviour"})) << ","
              << sql_timestamptz_literal(find_path(row, {"created_at"})) << ","
              << sql_timestamptz_literal(find_path(row, {"updated_at"})) << ","
              << "NOW())";
        values.push_back(tuple.str());
        loaded += 1;

        if (values.size() >= 150) {
            if (flush_insert_batch(paths, log_file, "load_project_sessions", insert_prefix, conflict_clause, &values) != 0) {
                return 1;
            }
        }
    }

    if (flush_insert_batch(paths, log_file, "load_project_sessions", insert_prefix, conflict_clause, &values) != 0) {
        return 1;
    }

    log_line(log_file, "load_metadata: project_sessions loaded=" + std::to_string(loaded));
    return 0;
}

int load_coalitions_table(const RuntimePaths& paths, const fs::path& log_file) {
    json rows;
    if (!read_array_file(paths.exports_root / "08_coalitions" / "all.json", &rows, log_file)) {
        return 1;
    }

    auto coalition_is_better = [&](const json& candidate, const json& current) -> bool {
        const auto cand_score = json_to_i64_ptr(find_path(candidate, {"score"}));
        const auto curr_score = json_to_i64_ptr(find_path(current, {"score"}));
        if (cand_score.has_value() && (!curr_score.has_value() || *cand_score > *curr_score)) {
            return true;
        }
        if (curr_score.has_value() && (!cand_score.has_value() || *cand_score < *curr_score)) {
            return false;
        }

        const auto cand_id = json_to_i64_ptr(find_path(candidate, {"id"}));
        const auto curr_id = json_to_i64_ptr(find_path(current, {"id"}));
        if (cand_id.has_value() && curr_id.has_value()) {
            return *cand_id > *curr_id;
        }
        return false;
    };

    std::map<std::string, std::size_t> slug_to_index;
    std::vector<const json*> canonical_rows;
    std::size_t duplicate_slug_rows = 0;

    for (const auto& row : rows) {
        if (!row.is_object()) {
            continue;
        }
        const auto id = json_to_i64_ptr(find_path(row, {"id"}));
        if (!id.has_value()) {
            continue;
        }
        const auto slug_opt = json_to_text_ptr(find_path(row, {"slug"}));
        const std::string slug = slug_opt.has_value() ? trim(*slug_opt) : "";
        if (slug.empty()) {
            continue;
        }

        const auto it = slug_to_index.find(slug);
        if (it == slug_to_index.end()) {
            slug_to_index[slug] = canonical_rows.size();
            canonical_rows.push_back(&row);
            continue;
        }

        duplicate_slug_rows += 1;
        const json& current = *canonical_rows[it->second];
        if (coalition_is_better(row, current)) {
            canonical_rows[it->second] = &row;
        }
    }

    if (duplicate_slug_rows > 0) {
        log_line(
            log_file,
            "load_coalitions: deduplicated " + std::to_string(duplicate_slug_rows) +
                " row(s) with duplicate slug"
        );
    }

    std::set<long long> seen_ids;
    std::vector<std::string> values;
    std::size_t loaded = 0;

    const std::string insert_prefix =
        "INSERT INTO coalitions ("
        "id, name, slug, image_url, cover_url, color, score, user_id, created_at, updated_at, ingested_at"
        ") VALUES";
    const std::string conflict_clause =
        "ON CONFLICT (id) DO UPDATE SET "
        "name = EXCLUDED.name, slug = EXCLUDED.slug, image_url = EXCLUDED.image_url, "
        "cover_url = EXCLUDED.cover_url, color = EXCLUDED.color, score = EXCLUDED.score, "
        "user_id = EXCLUDED.user_id, created_at = EXCLUDED.created_at, updated_at = EXCLUDED.updated_at, "
        "ingested_at = NOW()";

    for (const json* row_ptr : canonical_rows) {
        if (row_ptr == nullptr) {
            continue;
        }
        const json& row = *row_ptr;
        if (!row.is_object()) {
            continue;
        }
        auto id = json_to_i64_ptr(find_path(row, {"id"}));
        if (!id.has_value() || !seen_ids.insert(*id).second) {
            continue;
        }
        std::string created_at = sql_timestamptz_literal(find_path(row, {"created_at"}));
        if (created_at == "NULL") {
            created_at = "NOW()";
        }
        std::string updated_at = sql_timestamptz_literal(find_path(row, {"updated_at"}));
        if (updated_at == "NULL") {
            updated_at = created_at;
        }

        std::ostringstream tuple;
        tuple << "("
              << *id << ","
              << sql_text_literal(find_path(row, {"name"})) << ","
              << sql_text_literal(find_path(row, {"slug"})) << ","
              << sql_text_literal(first_non_null_path(row, {{"image_url"}, {"image", "url"}})) << ","
              << sql_text_literal(first_non_null_path(row, {{"cover_url"}, {"cover", "url"}})) << ","
              << sql_color_literal(find_path(row, {"color"})) << ","
              << sql_int_literal(find_path(row, {"score"})) << ","
              << sql_int_literal(first_non_null_path(row, {{"user_id"}, {"user", "id"}})) << ","
              << created_at << ","
              << updated_at << ","
              << "NOW())";
        values.push_back(tuple.str());
        loaded += 1;

        if (values.size() >= 200) {
            if (flush_insert_batch(paths, log_file, "load_coalitions", insert_prefix, conflict_clause, &values) != 0) {
                return 1;
            }
        }
    }

    if (flush_insert_batch(paths, log_file, "load_coalitions", insert_prefix, conflict_clause, &values) != 0) {
        return 1;
    }

    log_line(log_file, "load_metadata: coalitions loaded=" + std::to_string(loaded));
    return 0;
}

int load_achievements_table(const RuntimePaths& paths, const fs::path& log_file) {
    json rows;
    if (!read_array_file(paths.exports_root / "03_achievements" / "all.json", &rows, log_file)) {
        return 1;
    }

    std::set<long long> seen_ids;
    std::vector<std::string> values;
    std::size_t loaded = 0;

    const std::string insert_prefix =
        "INSERT INTO achievements ("
        "id, name, description, tier, kind, visible, image, nbr_of_success, users_url, parent_id, title, ingested_at"
        ") VALUES";
    const std::string conflict_clause =
        "ON CONFLICT (id) DO UPDATE SET "
        "name = EXCLUDED.name, description = EXCLUDED.description, tier = EXCLUDED.tier, "
        "kind = EXCLUDED.kind, visible = EXCLUDED.visible, image = EXCLUDED.image, "
        "nbr_of_success = EXCLUDED.nbr_of_success, users_url = EXCLUDED.users_url, "
        "parent_id = EXCLUDED.parent_id, title = EXCLUDED.title, ingested_at = NOW()";

    for (const auto& row : rows) {
        if (!row.is_object()) {
            continue;
        }
        auto id = json_to_i64_ptr(find_path(row, {"id"}));
        if (!id.has_value() || !seen_ids.insert(*id).second) {
            continue;
        }

        std::ostringstream tuple;
        tuple << "("
              << *id << ","
              << sql_text_literal(find_path(row, {"name"})) << ","
              << sql_text_literal(find_path(row, {"description"})) << ","
              << sql_text_literal(find_path(row, {"tier"})) << ","
              << sql_text_literal(find_path(row, {"kind"})) << ","
              << sql_bool_literal(find_path(row, {"visible"})) << ","
              << sql_text_literal(first_non_null_path(row, {{"image", "link"}, {"image_url"}, {"image"}})) << ","
              << sql_int_literal(find_path(row, {"nbr_of_success"})) << ","
              << sql_text_literal(find_path(row, {"users_url"})) << ","
              << sql_int_literal(find_path(row, {"parent_id"})) << ","
              << sql_text_literal(find_path(row, {"title"})) << ","
              << "NOW())";
        values.push_back(tuple.str());
        loaded += 1;

        if (values.size() >= 250) {
            if (flush_insert_batch(paths, log_file, "load_achievements", insert_prefix, conflict_clause, &values) != 0) {
                return 1;
            }
        }
    }

    if (flush_insert_batch(paths, log_file, "load_achievements", insert_prefix, conflict_clause, &values) != 0) {
        return 1;
    }

    log_line(log_file, "load_metadata: achievements loaded=" + std::to_string(loaded));
    return 0;
}

int load_campus_achievements_table(const RuntimePaths& paths, const fs::path& log_file) {
    json rows;
    if (!read_array_file(paths.exports_root / "04_campus_achievements" / "raw_all.json", &rows, log_file)) {
        return 1;
    }

    std::set<std::string> seen_keys;
    std::vector<std::string> values;
    std::size_t loaded = 0;

    const std::string insert_prefix =
        "INSERT INTO campus_achievements (campus_id, achievement_id, ingested_at) VALUES";
    const std::string conflict_clause =
        "ON CONFLICT (campus_id, achievement_id) DO UPDATE SET ingested_at = NOW()";

    for (const auto& row : rows) {
        if (!row.is_object()) {
            continue;
        }
        auto campus_id = json_to_i64_ptr(first_non_null_path(row, {{"campus_id"}, {"campus", "id"}}));
        auto achievement_id = json_to_i64_ptr(first_non_null_path(row, {{"achievement_id"}, {"id"}}));
        if (!campus_id.has_value() || !achievement_id.has_value()) {
            continue;
        }
        const std::string key = std::to_string(*campus_id) + ":" + std::to_string(*achievement_id);
        if (!seen_keys.insert(key).second) {
            continue;
        }

        std::ostringstream tuple;
        tuple << "(" << *campus_id << "," << *achievement_id << ",NOW())";
        values.push_back(tuple.str());
        loaded += 1;

        if (values.size() >= 500) {
            if (flush_insert_batch(paths, log_file, "load_campus_achievements", insert_prefix, conflict_clause, &values) != 0) {
                return 1;
            }
        }
    }

    if (flush_insert_batch(paths, log_file, "load_campus_achievements", insert_prefix, conflict_clause, &values) != 0) {
        return 1;
    }

    log_line(log_file, "load_metadata: campus_achievements loaded=" + std::to_string(loaded));
    return 0;
}

std::optional<long long> extract_user_campus_id(const json& row) {
    if (auto direct = json_to_i64_ptr(first_non_null_path(row, {{"campus_id"}, {"campus", "id"}})); direct.has_value()) {
        return direct;
    }
    const json* campuses = find_path(row, {"campus"});
    if (campuses && campuses->is_array() && !campuses->empty()) {
        const json& first = campuses->front();
        if (auto from_array = json_to_i64_ptr(find_path(first, {"id"})); from_array.has_value()) {
            return from_array;
        }
    }
    return std::nullopt;
}

int load_users_table(
    const RuntimePaths& paths,
    const fs::path& log_file,
    const fs::path& users_file,
    std::optional<long long> campus_fallback
) {
    json rows;
    if (!read_array_file(users_file, &rows, log_file)) {
        return 1;
    }

    std::set<long long> seen_ids;
    std::vector<std::string> values;
    std::size_t loaded = 0;

    const std::string insert_prefix =
        "INSERT INTO users ("
        "id, email, login, first_name, last_name, usual_full_name, usual_first_name, url, phone, displayname, "
        "kind, image_link, image_large, image_medium, image_small, image_micro, image, staff, correction_point, "
        "pool_month, pool_year, location, wallet, anonymize_date, data_erasure_date, created_at, updated_at, "
        "alumnized_at, alumni, active, campus_id, ingested_at"
        ") VALUES";
    const std::string conflict_clause =
        "ON CONFLICT (id) DO UPDATE SET "
        "email = EXCLUDED.email, login = EXCLUDED.login, first_name = EXCLUDED.first_name, "
        "last_name = EXCLUDED.last_name, usual_full_name = EXCLUDED.usual_full_name, "
        "usual_first_name = EXCLUDED.usual_first_name, url = EXCLUDED.url, phone = EXCLUDED.phone, "
        "displayname = EXCLUDED.displayname, kind = EXCLUDED.kind, image_link = EXCLUDED.image_link, "
        "image_large = EXCLUDED.image_large, image_medium = EXCLUDED.image_medium, image_small = EXCLUDED.image_small, "
        "image_micro = EXCLUDED.image_micro, image = EXCLUDED.image, staff = EXCLUDED.staff, "
        "correction_point = EXCLUDED.correction_point, pool_month = EXCLUDED.pool_month, "
        "pool_year = EXCLUDED.pool_year, location = EXCLUDED.location, wallet = EXCLUDED.wallet, "
        "anonymize_date = EXCLUDED.anonymize_date, data_erasure_date = EXCLUDED.data_erasure_date, "
        "created_at = EXCLUDED.created_at, updated_at = EXCLUDED.updated_at, alumnized_at = EXCLUDED.alumnized_at, "
        "alumni = EXCLUDED.alumni, active = EXCLUDED.active, campus_id = EXCLUDED.campus_id, ingested_at = NOW()";

    for (const auto& row : rows) {
        if (!row.is_object()) {
            continue;
        }

        auto id = json_to_i64_ptr(find_path(row, {"id"}));
        if (!id.has_value() || !seen_ids.insert(*id).second) {
            continue;
        }

        std::optional<long long> campus_id = extract_user_campus_id(row);
        if (!campus_id.has_value()) {
            campus_id = campus_fallback;
        }

        std::ostringstream tuple;
        tuple << "("
              << *id << ","
              << sql_text_literal(find_path(row, {"email"})) << ","
              << sql_text_literal(find_path(row, {"login"})) << ","
              << sql_text_literal(find_path(row, {"first_name"})) << ","
              << sql_text_literal(find_path(row, {"last_name"})) << ","
              << sql_text_literal(find_path(row, {"usual_full_name"})) << ","
              << sql_text_literal(find_path(row, {"usual_first_name"})) << ","
              << sql_text_literal(find_path(row, {"url"})) << ","
              << sql_text_literal(find_path(row, {"phone"})) << ","
              << sql_text_literal(find_path(row, {"displayname"})) << ","
              << sql_text_literal(find_path(row, {"kind"})) << ","
              << sql_text_literal(first_non_null_path(row, {{"image", "link"}, {"image_link"}, {"image_url"}})) << ","
              << sql_text_literal(first_non_null_path(row, {{"image", "versions", "large"}, {"image_large"}})) << ","
              << sql_text_literal(first_non_null_path(row, {{"image", "versions", "medium"}, {"image_medium"}})) << ","
              << sql_text_literal(first_non_null_path(row, {{"image", "versions", "small"}, {"image_small"}})) << ","
              << sql_text_literal(first_non_null_path(row, {{"image", "versions", "micro"}, {"image_micro"}})) << ","
              << sql_jsonb_literal(first_non_null_path(row, {{"image"}})) << ","
              << sql_bool_literal(first_non_null_path(row, {{"staff"}, {"staff?"}})) << ","
              << sql_int_literal(find_path(row, {"correction_point"})) << ","
              << sql_text_literal(find_path(row, {"pool_month"})) << ","
              << sql_text_literal(find_path(row, {"pool_year"})) << ","
              << sql_text_literal(find_path(row, {"location"})) << ","
              << sql_int_literal(find_path(row, {"wallet"})) << ","
              << sql_timestamptz_literal(find_path(row, {"anonymize_date"})) << ","
              << sql_timestamptz_literal(find_path(row, {"data_erasure_date"})) << ","
              << sql_timestamptz_literal(find_path(row, {"created_at"})) << ","
              << sql_timestamptz_literal(find_path(row, {"updated_at"})) << ","
              << sql_timestamptz_literal(find_path(row, {"alumnized_at"})) << ","
              << sql_bool_literal(first_non_null_path(row, {{"alumni"}, {"alumni?"}})) << ","
              << sql_bool_literal(first_non_null_path(row, {{"active"}, {"active?"}})) << ","
              << (campus_id.has_value() ? std::to_string(*campus_id) : "NULL") << ","
              << "NOW())";

        values.push_back(tuple.str());
        loaded += 1;

        if (values.size() >= 120) {
            if (flush_insert_batch(paths, log_file, "load_users", insert_prefix, conflict_clause, &values) != 0) {
                return 1;
            }
        }
    }

    if (flush_insert_batch(paths, log_file, "load_users", insert_prefix, conflict_clause, &values) != 0) {
        return 1;
    }

    log_line(log_file, "load_users: users loaded=" + std::to_string(loaded));
    return 0;
}

} // namespace

int cmd_load_metadata(const RuntimePaths& paths, const fs::path& log_file) {
    log_line(log_file, "Loading metadata into database (native loaders)");

    if (load_cursus_table(paths, log_file) != 0) {
        return 1;
    }
    if (load_campuses_table(paths, log_file) != 0) {
        return 1;
    }
    if (load_projects_table(paths, log_file) != 0) {
        return 1;
    }
    if (load_campus_projects_table(paths, log_file) != 0) {
        return 1;
    }
    if (load_project_sessions_table(paths, log_file) != 0) {
        return 1;
    }
    if (load_coalitions_table(paths, log_file) != 0) {
        return 1;
    }
    if (load_achievements_table(paths, log_file) != 0) {
        return 1;
    }
    if (load_campus_achievements_table(paths, log_file) != 0) {
        return 1;
    }

    log_line(log_file, "Metadata database load completed");
    return 0;
}

int cmd_load_internal_users(const RuntimePaths& paths, const fs::path& log_file) {
    const std::string campus_raw = get_env_or_default("CAMPUS_ID", "21");
    std::string campus_upper = campus_raw;
    std::transform(campus_upper.begin(), campus_upper.end(), campus_upper.begin(), [](unsigned char c) {
        return static_cast<char>(std::toupper(c));
    });

    std::optional<long long> campus_fallback;
    fs::path users_file;
    if (campus_upper == "ALL") {
        users_file = paths.exports_root / "09_users" / "campus_all" / "all.json";
    } else {
        campus_fallback = parse_int64(campus_raw);
        if (!campus_fallback.has_value()) {
            log_line(log_file, "cmd_load_internal_users: CAMPUS_ID must be numeric or ALL");
            return 1;
        }
        users_file = paths.exports_root / "09_users" / ("campus_" + std::to_string(*campus_fallback)) / "all.json";
    }

    if (!fs::exists(users_file)) {
        const fs::path fallback_file = paths.exports_root / "09_users" / "all.json";
        if (fs::exists(fallback_file)) {
            users_file = fallback_file;
        }
    }

    log_line(log_file, "Loading internal users from " + users_file.string());
    return load_users_table(paths, log_file, users_file, campus_fallback);
}

} // namespace orchestra
