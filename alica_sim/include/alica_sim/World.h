#pragma once

#include "alica_sim/JsonWriter.h"

#include <engine/AlicaClock.h>
#include <yaml-cpp/yaml.h>

#include <any>
#include <cstdint>
#include <string>

namespace alica::sim
{

struct AgentSpec
{
    std::string team;
    int teamIndex;
    std::string name;
    int64_t id;
    int indexInTeam;
    YAML::Node params;
};

class World
{
public:
    virtual ~World() = default;
    virtual std::string name() const = 0;
    virtual std::string rulesVersion() const = 0;
    virtual void configure(const YAML::Node& params, uint64_t seed) = 0;
    virtual std::any addAgent(const AgentSpec& spec) = 0;
    virtual void sense(AlicaTime now) = 0;
    virtual void step(AlicaTime now, AlicaTime dt) = 0;
    virtual bool finished() const = 0;
    virtual void writeHeader(JsonWriter& w) const = 0;
    virtual void writeFrame(JsonWriter& w) const = 0;
    virtual void writeResult(JsonWriter& w) const = 0;
};

using CreateWorldFn = World* (*) ();

} // namespace alica::sim

#define ALICA_SIM_WORLD_SYMBOL "alica_sim_create_world"
