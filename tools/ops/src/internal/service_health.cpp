#include "ops_internal.hpp"

#include <cctype>
#include <map>
#include <sstream>

namespace ops {

namespace {

std::string to_lower(std::string value) {
    for (char& c : value) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return value;
}

std::string normalize_health_status(const std::string& value) {
    const std::string raw = to_lower(trim(value));
    if (raw == "healthy" || raw == "unhealthy" || raw == "starting") {
        return raw;
    }
    if (raw == "none") {
        return "unknown";
    }
    if (raw.empty()) {
        return "unknown";
    }
    return raw;
}

std::string inspect_container_health(const std::string& container_name) {
    int rc = 0;
    const std::string raw = trim(run_bash(
        "timeout 5 docker inspect --format '{{if .State.Health}}{{.State.Health.Status}}{{else}}none{{end}}' " +
            shell_quote(container_name) + " 2>/dev/null",
        &rc
    ));
    if (rc != 0) {
        return "unknown";
    }
    return normalize_health_status(raw);
}

}  // namespace

std::vector<ServiceHealth> collect_service_health(bool* docker_available) {
    const std::vector<ServiceHealth> expected = {
        {"db", "transcendence_db", false, "", "", ""},
        {"api", "transcendence_api", false, "", "", ""},
        {"web", "transcendence_web", false, "", "", ""},
        {"detector", "transcendence_detector", false, "", "", ""},
        {"fetcher_internal", "transcendence_fetcher_internal", false, "", "", ""},
        {"fetcher_external_1", "transcendence_fetcher_external_1", false, "", "", ""},
        {"fetcher_external_2", "transcendence_fetcher_external_2", false, "", "", ""},
        {"fetcher_external_3", "transcendence_fetcher_external_3", false, "", "", ""},
        {"upserter_users", "transcendence_upserter_users", false, "", "", ""},
        {"upserter_events", "transcendence_upserter_events", false, "", "", ""},
        {"maintenance_scheduler", "transcendence_maintenance_scheduler", false, "", "", ""},
    };

    std::vector<ServiceHealth> out = expected;
    if (!command_exists("docker")) {
        if (docker_available) {
            *docker_available = false;
        }
        for (auto& svc : out) {
            svc.status = "docker_unavailable";
            svc.health = "unknown";
            svc.ports = "-";
        }
        return out;
    }
    if (docker_available) {
        *docker_available = true;
    }

    int rc = 0;
    const std::string raw = run_bash(
        "timeout 5 docker ps --format '{{.Names}}|{{.Status}}|{{.Ports}}'",
        &rc
    );
    if (rc != 0) {
        for (auto& svc : out) {
            svc.status = "docker_unreachable";
            svc.health = "unknown";
            svc.ports = "-";
        }
        return out;
    }

    std::map<std::string, std::pair<std::string, std::string>> running;
    std::istringstream iss(raw);
    std::string line;
    while (std::getline(iss, line)) {
        if (trim(line).empty()) {
            continue;
        }
        std::string name;
        std::string status;
        std::string ports;
        std::istringstream lss(line);
        std::getline(lss, name, '|');
        std::getline(lss, status, '|');
        std::getline(lss, ports);
        running[trim(name)] = {trim(status), trim(ports)};
    }

    for (auto& svc : out) {
        const auto it = running.find(svc.container_name);
        if (it == running.end()) {
            svc.running = false;
            svc.status = "not_running";
            svc.health = "down";
            svc.ports = "-";
            continue;
        }
        svc.running = true;
        svc.status = it->second.first.empty() ? "running" : it->second.first;
        svc.ports = it->second.second.empty() ? "no ports" : it->second.second;

        svc.health = inspect_container_health(svc.container_name);
        if (svc.health.empty()) {
            svc.health = "unknown";
        }
    }
    return out;
}

}  // namespace ops
