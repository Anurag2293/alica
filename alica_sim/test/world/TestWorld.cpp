#include "../TestIO.h"

#include <alica_sim/World.h>

#include <memory>
#include <random>
#include <vector>

namespace alica_sim_test
{

class TestWorld : public alica::sim::World
{
public:
    std::string name() const override { return "test_world"; }
    std::string rulesVersion() const override { return "1"; }

    void configure(const YAML::Node& params, uint64_t seed) override
    {
        _rng.seed(seed);
        _finishAt = params["finishAt"].as<double>(0);
    }

    std::any addAgent(const alica::sim::AgentSpec& spec) override
    {
        auto agent = std::make_unique<Agent>();
        agent->name = spec.name;
        agent->x = static_cast<double>(_rng() % 1000) / 100.0;
        _agents.push_back(std::move(agent));
        return &_agents.back()->io;
    }

    void sense(alica::AlicaTime) override
    {
        for (auto& agent : _agents) {
            agent->io.x = agent->x;
        }
    }

    void step(alica::AlicaTime now, alica::AlicaTime dt) override
    {
        for (auto& agent : _agents) {
            agent->x += agent->io.vx * static_cast<double>(dt.inNanoseconds()) / 1e9;
        }
        _now = static_cast<double>((now + dt).inNanoseconds()) / 1e9;
    }

    bool finished() const override { return _finishAt > 0 && _now >= _finishAt; }

    void writeHeader(alica::sim::JsonWriter& w) const override
    {
        w.key("agents").beginArray();
        for (const auto& agent : _agents) {
            w.value(agent->name);
        }
        w.endArray();
    }

    void writeFrame(alica::sim::JsonWriter& w) const override
    {
        w.key("x").beginArray();
        for (const auto& agent : _agents) {
            w.value(agent->x);
        }
        w.endArray();
    }

    void writeResult(alica::sim::JsonWriter& w) const override
    {
        for (const auto& agent : _agents) {
            w.key(agent->name).beginObject();
            w.field("x", agent->x);
            w.key("events").beginArray();
            for (const auto& [event, t] : agent->io.events) {
                w.beginArray().value(event).value(t).endArray();
            }
            w.endArray();
            w.endObject();
        }
    }

private:
    struct Agent
    {
        std::string name;
        double x = 0;
        TestIO io;
    };

    std::mt19937_64 _rng;
    double _finishAt = 0;
    double _now = 0;
    std::vector<std::unique_ptr<Agent>> _agents;
};

} // namespace alica_sim_test

extern "C" alica::sim::World* alica_sim_create_world()
{
    return new alica_sim_test::TestWorld();
}
