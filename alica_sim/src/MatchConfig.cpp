#include "alica_sim/MatchConfig.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <set>

namespace alica::sim
{

namespace
{

namespace fs = std::filesystem;

const char* const kEngineDefaults = R"(
Local: {}
Alica:
  TeamTimeOut: 10000
  AssignmentProtectionTime: 500
  AutoFailureHandling: true
  MinBroadcastFrequency: 10
  MaxBroadcastFrequency: 15
  EngineFrequency: 30
  MaxRuleApplications: 20
  AutoDiscovery: true
  AgentAnnouncementTimeInterval: 30
  AnnouncementRetries: 3
  SilentStart: false
  AllowIdling: false
  UseStaticRoles: true
  SimulatedTime: true
  StatusMessages:
    Enabled: false
    Frequency: 10
  Logging:
    Verbosity: WARNING
  CycleDetection:
    Enabled: true
    CycleCount: 5
    MinimalAuthorityTimeInterval: 800
    MaximalAuthorityTimeInterval: 5000
    IntervalIncreaseFactor: 1.5
    IntervalDecreaseFactor: 0.999
    MessageTimeInterval: 60
    MessageWaitTimeInterval: 200
    HistorySize: 45
  CSPSolving:
    EnableCommunication: true
    CommunicationFrequency: 10
    SeedMergingThreshold: 0.0015
    ConflictThreshold: 1000
    SeedTTL4Communication: 250
    SeedTTL4Usage: 5000
    MaxSolveTime: 10
    MaxFunctionEvaluations: 100000000
)";

YAML::Node required(const YAML::Node& node, const std::string& key, const std::string& where)
{
    YAML::Node value = node[key];
    if (!value || value.IsNull()) {
        throw ConfigError(where + ": '" + key + "' is missing");
    }
    return value;
}

template <class T>
T as(const YAML::Node& node, const std::string& key, const std::string& where)
{
    try {
        return node.as<T>();
    } catch (const YAML::Exception&) {
        throw ConfigError(where + ": '" + key + "' has the wrong type");
    }
}

template <class T>
T requiredAs(const YAML::Node& node, const std::string& key, const std::string& where)
{
    return as<T>(required(node, key, where), key, where);
}

template <class T>
T optionalAs(const YAML::Node& node, const std::string& key, const std::string& where, T fallback)
{
    YAML::Node value = node[key];
    if (!value || value.IsNull()) {
        return fallback;
    }
    return as<T>(value, key, where);
}

YAML::Node optionalMap(const YAML::Node& node, const std::string& key, const std::string& where)
{
    YAML::Node value = node[key];
    if (!value || value.IsNull()) {
        return YAML::Node(YAML::NodeType::Map);
    }
    if (!value.IsMap()) {
        throw ConfigError(where + ": '" + key + "' must be a mapping");
    }
    return YAML::Clone(value);
}

std::vector<std::string> pathList(const YAML::Node& node, const std::string& key, const std::string& where, const fs::path& baseDir, bool require)
{
    YAML::Node value = node[key];
    if (!value || value.IsNull()) {
        if (require) {
            throw ConfigError(where + ": '" + key + "' is missing");
        }
        return {};
    }
    std::vector<std::string> raw;
    if (value.IsSequence()) {
        for (const auto& item : value) {
            raw.push_back(as<std::string>(item, key, where));
        }
    } else {
        raw.push_back(as<std::string>(value, key, where));
    }
    std::vector<std::string> paths;
    for (const std::string& p : raw) {
        fs::path path(p);
        paths.push_back((path.is_absolute() ? path : baseDir / path).lexically_normal().string());
    }
    if (require && paths.empty()) {
        throw ConfigError(where + ": '" + key + "' is empty");
    }
    return paths;
}

AlicaTime seconds(double s)
{
    return AlicaTime::nanoseconds(std::llround(s * 1e9));
}

void flatten(const YAML::Node& node, const std::string& prefix, std::vector<std::pair<std::string, YAML::Node>>& out)
{
    for (const auto& kv : node) {
        std::string path = prefix + kv.first.as<std::string>();
        if (kv.second.IsMap()) {
            flatten(kv.second, path + ".", out);
        } else {
            out.emplace_back(path, YAML::Clone(kv.second));
        }
    }
}

void setPath(YAML::Node root, const std::string& path, const YAML::Node& value)
{
    YAML::Node current(root);
    size_t start = 0;
    size_t dot;
    while ((dot = path.find('.', start)) != std::string::npos) {
        current.reset(current[path.substr(start, dot - start)]);
        start = dot + 1;
    }
    current[path.substr(start)] = YAML::Clone(value);
}

bool validName(const std::string& name)
{
    return !name.empty() && std::all_of(name.begin(), name.end(), [](char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_'; });
}

std::string lower(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

double inSeconds(AlicaTime t)
{
    return static_cast<double>(t.inNanoseconds()) / 1e9;
}

} // namespace

MatchConfig loadMatchConfig(const std::string& path)
{
    YAML::Node root;
    try {
        root = YAML::LoadFile(path);
    } catch (const YAML::Exception& e) {
        throw ConfigError("cannot read match config " + path + ": " + e.what());
    }
    return parseMatchConfig(root, fs::absolute(path).parent_path().string());
}

MatchConfig parseMatchConfig(const YAML::Node& root, const std::string& baseDirString)
{
    const fs::path baseDir(baseDirString);
    MatchConfig config;
    if (!root.IsMap()) {
        throw ConfigError("match config: not a mapping");
    }

    YAML::Node world = required(root, "world", "match config");
    config.worldPlugin = requiredAs<std::string>(world, "plugin", "world");
    config.worldParams = optionalMap(world, "params", "world");

    config.seed = optionalAs<uint64_t>(root, "seed", "match config", 0);
    double dt = optionalAs<double>(root, "dt", "match config", 0.01);
    double duration = requiredAs<double>(root, "duration", "match config");
    if (!(dt > 0) || !(duration > 0)) {
        throw ConfigError("match config: 'dt' and 'duration' must be positive");
    }
    config.dt = seconds(dt);
    config.duration = seconds(duration);
    config.recordEvery = optionalAs<int>(root, "recordEvery", "match config", 1);
    if (config.recordEvery < 1) {
        throw ConfigError("match config: 'recordEvery' must be at least 1");
    }
    flatten(optionalMap(root, "engine", "match config"), "", config.engine);

    YAML::Node teams = required(root, "teams", "match config");
    if (!teams.IsSequence() || teams.size() == 0) {
        throw ConfigError("match config: 'teams' must be a non-empty list");
    }
    std::set<std::string> teamNames;
    std::set<std::string> playerNames;
    std::set<int64_t> ids;
    for (const auto& t : teams) {
        TeamConfig team;
        team.name = requiredAs<std::string>(t, "name", "team");
        const std::string where = "team " + team.name;
        if (!teamNames.insert(team.name).second) {
            throw ConfigError(where + ": team names must be unique");
        }
        team.plans = pathList(t, "plans", where, baseDir, true);
        team.libraries = pathList(t, "libraries", where, baseDir, false);
        team.roleSet = optionalAs<std::string>(t, "roleSet", where, "RoleSet");
        team.masterPlan = requiredAs<std::string>(t, "masterPlan", where);
        YAML::Node players = required(t, "players", where);
        if (!players.IsSequence() || players.size() == 0) {
            throw ConfigError(where + ": 'players' must be a non-empty list");
        }
        for (const auto& p : players) {
            PlayerConfig player;
            player.name = requiredAs<std::string>(p, "name", where + " player");
            const std::string playerWhere = where + " player " + player.name;
            player.id = requiredAs<int64_t>(p, "id", playerWhere);
            player.params = optionalMap(p, "params", playerWhere);
            player.local = optionalMap(p, "local", playerWhere);
            if (!validName(player.name) || !playerNames.insert(lower(player.name)).second) {
                throw ConfigError(playerWhere + ": player names must match [A-Za-z0-9_-]+ and be unique in the match, ignoring case");
            }
            if (player.id <= 0 || !ids.insert(player.id).second) {
                throw ConfigError(playerWhere + ": player ids must be positive and unique in the match");
            }
            team.players.push_back(std::move(player));
        }
        config.teams.push_back(std::move(team));
    }
    return config;
}

void writeMatchConfigFields(JsonWriter& w, const MatchConfig& config)
{
    w.key("world").beginObject();
    w.field("plugin", config.worldPlugin);
    w.field("params", config.worldParams);
    w.endObject();
    w.field("seed", config.seed);
    w.field("dt", inSeconds(config.dt));
    w.field("duration", inSeconds(config.duration));
    w.field("recordEvery", config.recordEvery);
    w.key("engine").beginObject();
    for (const auto& [path, value] : config.engine) {
        w.field(path, value);
    }
    w.endObject();
    w.key("teams").beginArray();
    for (const TeamConfig& team : config.teams) {
        w.beginObject();
        w.field("name", team.name);
        w.key("plans").beginArray();
        for (const std::string& p : team.plans) {
            w.value(p);
        }
        w.endArray();
        w.key("libraries").beginArray();
        for (const std::string& l : team.libraries) {
            w.value(l);
        }
        w.endArray();
        w.field("roleSet", team.roleSet);
        w.field("masterPlan", team.masterPlan);
        w.key("players").beginArray();
        for (const PlayerConfig& player : team.players) {
            w.beginObject();
            w.field("name", player.name);
            w.field("id", player.id);
            w.field("params", player.params);
            w.field("local", player.local);
            w.endObject();
        }
        w.endArray();
        w.endObject();
    }
    w.endArray();
}

YAML::Node engineConfig(const MatchConfig& config, const PlayerConfig& player)
{
    YAML::Node root = YAML::Load(kEngineDefaults);
    for (const auto& [path, value] : config.engine) {
        setPath(root, path, value);
    }
    std::vector<std::pair<std::string, YAML::Node>> local;
    flatten(player.local, "Local.", local);
    for (const auto& [path, value] : local) {
        setPath(root, path, value);
    }
    root["Alica"]["SimulatedTime"] = true;
    return root;
}

} // namespace alica::sim
