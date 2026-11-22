#include "alica_sim/Agent.h"

#include <engine/blackboard/Blackboard.h>

#include <stdexcept>

namespace alica::sim
{

Agent::Agent(const AgentSpec& spec, const TeamConfig& team, const std::string& configDir, const MatchTime& time, Scheduler& scheduler, int owner,
        MessageBus& bus, std::any io)
        : _name(spec.name)
        , _scheduler(scheduler)
{
    std::vector<std::string> configPaths{configDir};
    configPaths.insert(configPaths.end(), team.plans.begin(), team.plans.end());
    _context = std::make_unique<AlicaContext>(AlicaContextParams(spec.name, configPaths, team.roleSet, team.masterPlan, false, spec.id));
    _context->setClock<SimClock>(time);
    _context->setTimerFactory<SimTimerFactory>(scheduler, owner);
    _context->setEngineTimerFactory<SimTimerFactory>(scheduler, owner);
    _context->setCommunicator<SimCommunication>(bus);
    LockedBlackboardRW(*_context->getGlobalBlackboardShared()).set("io", std::move(io));
}

Agent::~Agent()
{
    stopNow();
}

void Agent::start(AlicaCreators&& creators)
{
    _creators = std::move(creators);
    if (_context->init(std::move(_creators)) != 0) {
        throw std::runtime_error("agent " + _name + ": the engine failed to initialise");
    }
    _started = true;
}

void Agent::stop()
{
    _scheduler.defer([this]() { stopNow(); });
}

void Agent::stopNow()
{
    if (_started) {
        _started = false;
        _context->terminate();
    }
}

} // namespace alica::sim
