#pragma once

#include "alica_sim/MatchConfig.h"
#include "alica_sim/MessageBus.h"
#include "alica_sim/Scheduler.h"
#include "alica_sim/SimClock.h"
#include "alica_sim/World.h"

#include <engine/AlicaContext.h>

#include <any>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace alica::sim
{

class TeamFactory
{
public:
    virtual ~TeamFactory() = default;
    virtual AlicaCreators creators() = 0;
    // Library name and resolved file of every library the creators load, for the replay provenance.
    virtual std::vector<std::pair<std::string, std::string>> libraries() const { return {}; }
};

class Agent
{
public:
    Agent(const AgentSpec& spec, const TeamConfig& team, const std::string& configDir, const MatchTime& time, Scheduler& scheduler, int owner, MessageBus& bus,
            std::any io);
    ~Agent();

    void start(AlicaCreators&& creators);
    void stop();

    AlicaContext& context() { return *_context; }
    const std::string& name() const { return _name; }

private:
    std::string _name;
    AlicaCreators _creators;
    std::unique_ptr<AlicaContext> _context;
    bool _started = false;
};

} // namespace alica::sim
