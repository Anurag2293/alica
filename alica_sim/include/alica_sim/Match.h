#pragma once

#include "alica_sim/Agent.h"
#include "alica_sim/JsonWriter.h"
#include "alica_sim/MatchConfig.h"
#include "alica_sim/MessageBus.h"
#include "alica_sim/Scheduler.h"
#include "alica_sim/SimClock.h"
#include "alica_sim/World.h"

#include <functional>
#include <memory>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

namespace alica::sim
{

using TeamFactoryMaker = std::function<std::unique_ptr<TeamFactory>(const TeamConfig&)>;

struct MatchOptions
{
    std::ostream* replay = nullptr;
    std::string workDir;
    std::string worldLibrary;
};

class Match
{
public:
    Match(MatchConfig config, std::unique_ptr<World> world, TeamFactoryMaker makeTeamFactory, MatchOptions options = {});
    ~Match();
    Match(const Match&) = delete;
    Match& operator=(const Match&) = delete;

    void start();
    void tick();
    bool finished() const;
    // Called during a tick, it halts the scheduler and the tick ends without further phases or a frame.
    void stop();

    AlicaTime now() const { return _time.now(); }
    int64_t ticks() const { return _tick; }
    const std::string& resultLine() const { return _resultLine; }
    const MatchConfig& config() const { return _config; }
    World& world() { return *_world; }
    Agent& agent(const std::string& name);

    static std::string planHash(const std::vector<std::string>& planFolders);

private:
    void writeHeader();
    void recordFrame();
    void writeLine();

    MatchConfig _config;
    MatchOptions _options;
    std::unique_ptr<World> _world;
    TeamFactoryMaker _makeTeamFactory;
    MatchTime _time;
    Scheduler _scheduler{_time};
    std::vector<std::unique_ptr<MessageBus>> _buses;
    std::vector<std::unique_ptr<TeamFactory>> _factories;
    std::vector<std::unique_ptr<Agent>> _agents;
    std::vector<std::pair<std::string, YAML::Node>> _effectiveConfigs;
    std::string _workDir;
    bool _ownsWorkDir = false;
    int64_t _tick = 0;
    bool _started = false;
    bool _stopped = false;
    bool _ticking = false;
    bool _stopRequested = false;
    std::string _resultLine;
    JsonWriter _writer;
};

} // namespace alica::sim
