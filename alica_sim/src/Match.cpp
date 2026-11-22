#include "alica_sim/Match.h"
#include "alica_sim/PlanFiles.h"
#include "alica_sim/Sha256.h"

#include <unistd.h>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <set>
#include <stdexcept>

#ifndef ALICA_SIM_VERSION
#define ALICA_SIM_VERSION "unknown"
#endif
#ifndef ALICA_SIM_BUILD_ID
#define ALICA_SIM_BUILD_ID "unknown"
#endif

namespace alica::sim
{

namespace
{

namespace fs = std::filesystem;

void rejectEngineConfig(const TeamConfig& team)
{
    for (const std::string& folder : team.plans) {
        if (!fs::is_directory(folder)) {
            throw ConfigError("team " + team.name + ": plan folder " + folder + " does not exist");
        }
        for (const PlanFile& file : listFiles({folder})) {
            if (fs::path(file.relative).filename() == "Alica.yaml") {
                throw ConfigError("team " + team.name + ": " + file.path + " is not allowed, engine options belong in the match config");
            }
        }
    }
}

void validateTeam(const TeamConfig& team)
{
    rejectEngineConfig(team);
    if (findPlanFile(team.plans, team.masterPlan + ".pml").empty()) {
        throw ConfigError("team " + team.name + ": master plan " + team.masterPlan + ".pml is not in its plan folders");
    }
    try {
        planTreeLibraries(team.plans, team.masterPlan);
    } catch (const ConfigError& e) {
        throw ConfigError("team " + team.name + ": " + e.what());
    }
}

void checkRole(const TeamConfig& team, const PlayerConfig& player, const YAML::Node& engine)
{
    const std::string where = "team " + team.name + " player " + player.name;
    const YAML::Node role = engine["Local"]["DefaultRole"];
    if (!role || !role.IsScalar()) {
        throw ConfigError(where + ": 'local.DefaultRole' is missing");
    }
    const std::string roleSetFile = findRoleSetFile(team.plans, team.roleSet);
    if (roleSetFile.empty()) {
        throw ConfigError("team " + team.name + ": role set " + team.roleSet + ".rst is not in its plan folders and none of them has a default role set");
    }
    const std::set<std::string> roles = roleNames(roleSetFile);
    if (!roles.count(role.Scalar())) {
        std::string known;
        for (const std::string& r : roles) {
            known += (known.empty() ? "" : ", ") + r;
        }
        throw ConfigError(where + ": role " + role.Scalar() + " is not in " + roleSetFile + " (roles: " + known + ")");
    }
}

std::string makeTempDir()
{
    std::string pattern = (fs::temp_directory_path() / "alica_sim.XXXXXX").string();
    if (!mkdtemp(pattern.data())) {
        throw std::runtime_error("cannot create a temporary folder");
    }
    return pattern;
}

void writeFile(const fs::path& path, const std::string& content)
{
    fs::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary);
    out << content;
    if (!out) {
        throw std::runtime_error("cannot write " + path.string());
    }
}

void writeLibrary(JsonWriter& w, const std::string& key, const std::string& path)
{
    w.key(key).beginObject();
    w.field("path", path);
    w.field("sha256", Sha256::fileHex(path));
    w.endObject();
}

} // namespace

Match::Match(MatchConfig config, std::unique_ptr<World> world, TeamFactoryMaker makeTeamFactory, MatchOptions options)
        : _config(std::move(config))
        , _options(std::move(options))
        , _world(std::move(world))
        , _makeTeamFactory(std::move(makeTeamFactory))
{
    if (!_world) {
        throw std::invalid_argument("Match: no world");
    }
}

Match::~Match()
{
    _agents.clear();
    if (_ownsWorkDir) {
        std::error_code ec;
        fs::remove_all(_workDir, ec);
    }
}

void Match::start()
{
    if (_started) {
        return;
    }
    _started = true;
    std::vector<YAML::Node> engineConfigs;
    for (const TeamConfig& team : _config.teams) {
        validateTeam(team);
        for (const PlayerConfig& player : team.players) {
            engineConfigs.push_back(engineConfig(_config, player));
            validateEngineConfig(engineConfigs.back(), "team " + team.name + " player " + player.name);
            checkRole(team, player, engineConfigs.back());
        }
    }
    if (_options.workDir.empty()) {
        _workDir = makeTempDir();
        _ownsWorkDir = true;
    } else {
        _workDir = fs::absolute(_options.workDir).string();
        fs::create_directories(_workDir);
    }

    _world->configure(_config.worldParams, _config.seed);

    int owner = 0;
    for (size_t teamIndex = 0; teamIndex < _config.teams.size(); ++teamIndex) {
        const TeamConfig& team = _config.teams[teamIndex];
        _buses.push_back(std::make_unique<MessageBus>());
        _factories.push_back(_makeTeamFactory(team));
        for (size_t playerIndex = 0; playerIndex < team.players.size(); ++playerIndex) {
            const PlayerConfig& player = team.players[playerIndex];
            const YAML::Node& engine = engineConfigs[owner];
            YAML::Emitter emitter;
            emitter << engine;
            const fs::path configRoot = fs::path(_workDir) / std::to_string(owner);
            writeFile(configRoot / player.name / "Alica.yaml", std::string(emitter.c_str()) + "\n");
            _effectiveConfigs.emplace_back(player.name, engine);

            AgentSpec spec{team.name, static_cast<int>(teamIndex), player.name, player.id, static_cast<int>(playerIndex), player.params};
            std::any io = _world->addAgent(spec);
            _agents.push_back(std::make_unique<Agent>(spec, team, configRoot.string(), _time, _scheduler, owner++, *_buses.back(), std::move(io)));
            _agents.back()->start(_factories.back()->creators());
        }
    }

    writeHeader();
    recordFrame();
}

void Match::tick()
{
    const AlicaTime t = _config.dt * _tick;
    _time.set(t);
    for (auto& bus : _buses) {
        bus->deliver();
    }
    _world->sense(t);
    _scheduler.runUntil(t + _config.dt);
    _world->step(t, _config.dt);
    ++_tick;
    _time.set(_config.dt * _tick);
    if (_tick % _config.recordEvery == 0) {
        recordFrame();
    }
}

bool Match::finished() const
{
    return _world->finished() || _time.now() >= _config.duration;
}

void Match::stop()
{
    if (!_started || _stopped) {
        return;
    }
    _stopped = true;
    for (auto& agent : _agents) {
        agent->stop();
    }
    _writer.clear();
    _writer.beginObject();
    _writer.key("result").beginObject();
    _world->writeResult(_writer);
    _writer.endObject();
    _writer.field("ticks", _tick);
    _writer.endObject();
    _resultLine = _writer.str();
    writeLine();
    if (_options.replay) {
        _options.replay->flush();
    }
}

Agent& Match::agent(const std::string& name)
{
    for (auto& agent : _agents) {
        if (agent->name() == name) {
            return *agent;
        }
    }
    throw std::out_of_range("no agent " + name);
}

std::string Match::planHash(const std::vector<std::string>& planFolders)
{
    std::vector<PlanFile> files;
    for (const PlanFile& file : listFiles(planFolders)) {
        if (isPlanFile(file.relative)) {
            files.push_back(file);
        }
    }
    std::stable_sort(files.begin(), files.end(), [](const PlanFile& a, const PlanFile& b) { return a.relative < b.relative; });
    Sha256 sha;
    for (const PlanFile& file : files) {
        std::ifstream in(file.path, std::ios::binary);
        std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        sha.update(file.relative + "\n" + std::to_string(content.size()) + "\n");
        sha.update(content);
    }
    return sha.hexDigest();
}

void Match::writeHeader()
{
    JsonWriter& w = _writer;
    w.clear();
    w.beginObject();
    w.field("format", "alica-sim-replay");
    w.field("version", 1);

    w.key("match").beginObject();
    writeMatchConfigFields(w, _config);
    w.key("effectiveConfig").beginObject();
    for (const auto& [name, engine] : _effectiveConfigs) {
        w.field(name, engine);
    }
    w.endObject();
    w.endObject();

    w.key("provenance").beginObject();
    w.key("alicaSim").beginObject();
    w.field("version", ALICA_SIM_VERSION);
    w.field("build", ALICA_SIM_BUILD_ID);
    w.endObject();
    if (!_options.worldLibrary.empty()) {
        writeLibrary(w, "world", _options.worldLibrary);
    }
    w.key("teams").beginObject();
    for (size_t i = 0; i < _config.teams.size(); ++i) {
        w.key(_config.teams[i].name).beginObject();
        w.field("plans", planHash(_config.teams[i].plans));
        w.key("libraries").beginObject();
        for (const auto& [name, path] : _factories[i]->libraries()) {
            writeLibrary(w, name, path);
        }
        w.endObject();
        w.endObject();
    }
    w.endObject();
    w.endObject();

    w.key("world").beginObject();
    w.field("name", _world->name());
    w.field("rulesVersion", _world->rulesVersion());
    _world->writeHeader(w);
    w.endObject();
    w.endObject();
    writeLine();
}

void Match::recordFrame()
{
    _writer.clear();
    _writer.beginObject();
    _writer.key("t").number(formatSeconds(_time.now()));
    _writer.field("tick", _tick);
    _world->writeFrame(_writer);
    _writer.endObject();
    writeLine();
}

void Match::writeLine()
{
    if (_options.replay) {
        *_options.replay << _writer.str() << '\n';
    }
}

} // namespace alica::sim
