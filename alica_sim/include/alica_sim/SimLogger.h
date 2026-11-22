#pragma once

#include <engine/logging/IAlicaLogger.h>

#include <string>

namespace alica::sim
{

// The engine keeps one process-wide logger, stamped with the name of the last agent that set it. This one
// stamps each line with the agent whose code is running, named by the innermost AgentScope.
class SimLogger : public IAlicaLogger
{
public:
    SimLogger(Verbosity verbosity, const std::string& agentName);
    void log(const std::string& msg, Verbosity verbosity, const std::string& logSpace) override;

    class AgentScope
    {
    public:
        explicit AgentScope(const std::string& name);
        ~AgentScope();
        AgentScope(const AgentScope&) = delete;
        AgentScope& operator=(const AgentScope&) = delete;

    private:
        bool _pushed;
    };

private:
    Verbosity _verbosity;
    std::string _agentName;
};

} // namespace alica::sim
