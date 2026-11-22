#include "alica_sim/SimLogger.h"

#include <iostream>
#include <sstream>
#include <vector>

namespace alica::sim
{

namespace
{

std::vector<std::string>& runningAgents()
{
    static std::vector<std::string> names;
    return names;
}

const char* levelName(Verbosity verbosity)
{
    switch (verbosity) {
    case Verbosity::DEBUG:
        return "DEBUG";
    case Verbosity::INFO:
        return "INFO";
    case Verbosity::WARNING:
        return "WARNING";
    case Verbosity::ERROR:
        return "ERROR";
    default:
        return "FATAL";
    }
}

} // namespace

SimLogger::SimLogger(Verbosity verbosity, const std::string& agentName)
        : _verbosity(verbosity)
        , _agentName(agentName)
{
}

void SimLogger::log(const std::string& msg, Verbosity verbosity, const std::string& logSpace)
{
    if (verbosity < _verbosity) {
        return;
    }
    const std::vector<std::string>& running = runningAgents();
    std::ostringstream line;
    line << "[" << levelName(verbosity) << "][" << (running.empty() ? _agentName : running.back()) << "]";
    if (!logSpace.empty()) {
        line << "[" << logSpace << "]";
    }
    line << ": " << msg << "\n";
    (verbosity < Verbosity::WARNING ? std::cout : std::cerr) << line.str() << std::flush;
}

SimLogger::AgentScope::AgentScope(const std::string& name)
        : _pushed(!name.empty())
{
    if (_pushed) {
        runningAgents().push_back(name);
    }
}

SimLogger::AgentScope::~AgentScope()
{
    if (_pushed) {
        runningAgents().pop_back();
    }
}

} // namespace alica::sim
