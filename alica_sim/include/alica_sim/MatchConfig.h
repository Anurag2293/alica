#pragma once

#include "alica_sim/JsonWriter.h"

#include <engine/AlicaClock.h>
#include <yaml-cpp/yaml.h>

#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace alica::sim
{

struct ConfigError : std::runtime_error
{
    using std::runtime_error::runtime_error;
};

struct PlayerConfig
{
    std::string name;
    int64_t id = 0;
    YAML::Node params;
    YAML::Node local;
};

struct TeamConfig
{
    std::string name;
    std::vector<std::string> plans;
    std::vector<std::string> libraries;
    std::string roleSet = "RoleSet";
    std::string masterPlan;
    std::vector<PlayerConfig> players;
};

struct MatchConfig
{
    std::string worldPlugin;
    YAML::Node worldParams;
    uint64_t seed = 0;
    AlicaTime dt = AlicaTime::milliseconds(10);
    AlicaTime duration;
    int recordEvery = 1;
    std::vector<std::pair<std::string, YAML::Node>> engine;
    std::vector<TeamConfig> teams;
};

MatchConfig loadMatchConfig(const std::string& path);
MatchConfig parseMatchConfig(const YAML::Node& root, const std::string& baseDir);
void writeMatchConfigFields(JsonWriter& w, const MatchConfig& config);
YAML::Node engineConfig(const MatchConfig& config, const PlayerConfig& player);
void validateEngineConfig(const YAML::Node& engine, const std::string& where);

} // namespace alica::sim
