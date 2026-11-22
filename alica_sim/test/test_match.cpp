#include "TestIO.h"

#include <alica_sim/Match.h>
#include <alica_sim/Runner.h>

#include <engine/IUtilityCreator.h>
#include <engine/blackboard/Blackboard.h>

#include <gtest/gtest.h>
#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#ifdef __APPLE__
#include <mach/mach.h>
#endif

namespace alica::sim
{
namespace
{

namespace fs = std::filesystem;
using alica_sim_test::TestIO;
using Events = std::vector<std::pair<std::string, int64_t>>;

constexpr int64_t kPeriod = 33333333;

MatchConfig testConfig(int teams, int playersPerTeam, double duration, const std::string& plans = "team/plans")
{
    std::ostringstream yaml;
    yaml << "{world: {plugin: alica_sim_test_world}, seed: 3, dt: 0.01, duration: " << duration
         << ", recordEvery: 5, engine: {Alica.EngineFrequency: 30}, teams: [";
    int id = 1;
    for (int t = 0; t < teams; ++t) {
        yaml << (t ? "," : "") << "{name: T" << t << ", plans: " << plans << ", libraries: [" << ALICA_SIM_TEST_TEAM_DIR
             << "], masterPlan: TestMaster, players: [";
        for (int p = 0; p < playersPerTeam; ++p, ++id) {
            yaml << (p ? "," : "") << "{name: t" << t << "p" << p << ", id: " << id << ", params: {n: " << id << "}, local: {DefaultRole: Player}}";
        }
        yaml << "]}";
    }
    yaml << "]}";
    return parseMatchConfig(YAML::Load(yaml.str()), ALICA_SIM_TEST_DIR);
}

std::unique_ptr<Match> makeMatch(const MatchConfig& config, std::ostream* replay = nullptr, TeamFactoryMaker makeTeamFactory = nullptr)
{
    LoadedWorld world = loadWorld(config.worldPlugin, {ALICA_SIM_TEST_WORLD_DIR});
    MatchOptions options;
    options.replay = replay;
    options.worldLibrary = world.file;
    if (!makeTeamFactory) {
        makeTeamFactory = [](const TeamConfig& team) { return std::make_unique<DynamicTeamFactory>(team); };
    }
    return std::make_unique<Match>(config, std::move(world.world), std::move(makeTeamFactory), options);
}

TestIO& io(Match& match, const std::string& agent)
{
    LockedBlackboardRO bb(*match.agent(agent).context().getGlobalBlackboardShared());
    return *std::any_cast<TestIO*>(bb.get<std::any>("io"));
}

std::string runToReplay(const MatchConfig& config, double speed)
{
    std::ostringstream replay;
    auto match = makeMatch(config, &replay);
    runMatch(*match, speed);
    return replay.str();
}

int threadCount()
{
#ifdef __APPLE__
    thread_act_array_t threads;
    mach_msg_type_number_t count = 0;
    if (task_threads(mach_task_self(), &threads, &count) != KERN_SUCCESS) {
        return -1;
    }
    for (mach_msg_type_number_t i = 0; i < count; ++i) {
        mach_port_deallocate(mach_task_self(), threads[i]);
    }
    vm_deallocate(mach_task_self(), reinterpret_cast<vm_address_t>(threads), count * sizeof(thread_t));
    return static_cast<int>(count);
#else
    int count = 0;
    for ([[maybe_unused]] const auto& entry : fs::directory_iterator("/proc/self/task")) {
        ++count;
    }
    return count;
#endif
}

TEST(MatchTest, PlanTransitionsHappenAtTheExpectedMatchTimes)
{
    auto match = makeMatch(testConfig(1, 1, 1.5));
    runMatch(*match, 0);
    EXPECT_EQ(match->ticks(), 150);
    EXPECT_EQ(match->now(), AlicaTime::milliseconds(1500));
    EXPECT_EQ(io(*match, "t0p0").events,
            (Events{{"SucceedFirst.init", 0}, {"SucceedFirst.run", 0}, {"SucceedFirst.run", 33000000}, {"Walk.init", kPeriod}, {"Rest.init", 31 * kPeriod}}));
}

TEST(MatchTest, ABehaviourSucceedingInItsFirstRunTriggersItsTransitionAtTheNextEngineIteration)
{
    auto match = makeMatch(testConfig(2, 2, 0.5));
    runMatch(*match, 0);
    for (const char* agent : {"t0p0", "t0p1", "t1p0", "t1p1"}) {
        const Events& events = io(*match, agent).events;
        auto firstRun = std::find_if(events.begin(), events.end(), [](const auto& e) { return e.first == "SucceedFirst.run"; });
        auto walk = std::find_if(events.begin(), events.end(), [](const auto& e) { return e.first == "Walk.init"; });
        ASSERT_NE(firstRun, events.end()) << agent;
        ASSERT_NE(walk, events.end()) << agent;
        EXPECT_EQ(walk->second - firstRun->second, kPeriod) << agent;
    }
}

TEST(MatchTest, FourAgentsAt30HzRunAtLeast50TimesFasterThanRealTime)
{
    const double duration = 30;
    auto match = makeMatch(testConfig(2, 2, duration));
    const auto start = std::chrono::steady_clock::now();
    runMatch(*match, 0);
    const double wall = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    EXPECT_GE(duration / wall, 50.0) << "wall time " << wall << " s";
}

TEST(MatchTest, ReplaysAreIdenticalAcrossRunsAndSpeeds)
{
    const MatchConfig config = testConfig(2, 2, 1.2);
    const std::string first = runToReplay(config, 0);
    const std::string second = runToReplay(config, 0);
    const std::string paced = runToReplay(config, 1);
    EXPECT_EQ(first, second);
    EXPECT_EQ(first, paced);

    std::istringstream lines(first);
    std::vector<std::string> all;
    for (std::string line; std::getline(lines, line);) {
        all.push_back(line);
    }
    ASSERT_EQ(all.size(), 1u + 1u + 120u / 5u + 1u);
    YAML::Node header = YAML::Load(all.front());
    EXPECT_EQ(header["format"].as<std::string>(), "alica-sim-replay");
    EXPECT_EQ(header["match"]["teams"][1]["players"][0]["params"]["n"].as<int>(), 3);
    EXPECT_TRUE(header["match"]["effectiveConfig"]["t1p1"]["Alica"]["SimulatedTime"].as<bool>());
    EXPECT_EQ(header["match"]["effectiveConfig"]["t1p1"]["Local"]["DefaultRole"].as<std::string>(), "Player");
    EXPECT_EQ(header["provenance"]["world"]["sha256"].as<std::string>().size(), 64u);
    EXPECT_EQ(header["provenance"]["teams"]["T0"]["plans"].as<std::string>(), Match::planHash(config.teams[0].plans));
    EXPECT_EQ(header["provenance"]["teams"]["T1"]["libraries"]["alica_sim_test_team"]["sha256"].as<std::string>().size(), 64u);
    EXPECT_EQ(header["world"]["name"].as<std::string>(), "test_world");
    EXPECT_EQ(header["world"]["agents"].size(), 4u);
    EXPECT_EQ(YAML::Load(all[1])["tick"].as<int>(), 0);
    EXPECT_EQ(YAML::Load(all[2])["t"].as<double>(), 0.05);
    YAML::Node result = YAML::Load(all.back());
    EXPECT_EQ(result["ticks"].as<int>(), 120);
    EXPECT_EQ(result["result"]["t0p0"]["events"].size(), 5u);
}

TEST(MatchTest, TheEngineStartsNoThread)
{
    auto match = makeMatch(testConfig(2, 2, 2));
    const int before = threadCount();
    match->start();
    EXPECT_EQ(threadCount(), before);
    for (int i = 0; i < 100; ++i) {
        match->tick();
    }
    EXPECT_EQ(threadCount(), before);
    match->stop();
    EXPECT_EQ(threadCount(), before);
}

TEST(MatchTest, AnAlicaYamlInAPlanFolderIsAConfigError)
{
    const fs::path dir = fs::temp_directory_path() / "alica_sim_test_plans_with_config";
    fs::remove_all(dir);
    fs::create_directories(dir);
    fs::copy(fs::path(ALICA_SIM_TEST_DIR) / "team/plans", dir, fs::copy_options::recursive);
    std::ofstream(dir / "Alica.yaml") << "Alica: {}\n";
    auto match = makeMatch(testConfig(1, 1, 1, dir.string()));
    EXPECT_THROW(match->start(), ConfigError);
    fs::remove_all(dir);
}

fs::path copyPlans(const std::string& name)
{
    const fs::path dir = fs::temp_directory_path() / ("alica_sim_test_plans_" + name);
    fs::remove_all(dir);
    fs::create_directories(dir);
    fs::copy(fs::path(ALICA_SIM_TEST_DIR) / "team/plans", dir, fs::copy_options::recursive);
    return dir;
}

TEST(MatchTest, AnUnknownRoleIsAConfigError)
{
    MatchConfig config = testConfig(1, 1, 1);
    config.teams[0].players[0].local["DefaultRole"] = "Goalie";
    auto match = makeMatch(config);
    EXPECT_THROW(match->start(), ConfigError);
}

TEST(MatchTest, AMissingMasterPlanIsAConfigError)
{
    MatchConfig config = testConfig(1, 1, 1);
    config.teams[0].masterPlan = "Nope";
    auto match = makeMatch(config);
    EXPECT_THROW(match->start(), ConfigError);
}

TEST(MatchTest, AMissingRoleSetIsAConfigError)
{
    const fs::path dir = copyPlans("without_role_set");
    fs::remove(dir / "RoleSet.rst");
    auto match = makeMatch(testConfig(1, 1, 1, dir.string()));
    EXPECT_THROW(match->start(), ConfigError);
    fs::remove_all(dir);
}

TEST(MatchTest, AMissingPlanTreeFileIsAConfigError)
{
    const fs::path dir = copyPlans("without_walk");
    fs::remove(dir / "Walk.beh");
    auto match = makeMatch(testConfig(1, 1, 1, dir.string()));
    EXPECT_THROW(match->start(), ConfigError);
    fs::remove_all(dir);
}

TEST(MatchTest, AnInvalidEngineOptionIsAConfigError)
{
    MatchConfig config = testConfig(1, 1, 1);
    config.engine.emplace_back("Alica.EngineFrequency", YAML::Node(2000));
    auto match = makeMatch(config);
    EXPECT_THROW(match->start(), ConfigError);
}

TEST(MatchTest, AnUnusedPlanWithAnUnknownLibraryDoesNotStopTheMatch)
{
    const fs::path dir = copyPlans("with_unused_plan");
    std::ofstream(dir / "Old.pml") << R"({"id": 1, "name": "Old", "libraryName": "retired_lib", "states": []})";
    std::ostringstream replay;
    auto match = makeMatch(testConfig(1, 1, 0.2, dir.string()), &replay);
    runMatch(*match, 0);
    EXPECT_EQ(match->ticks(), 20);
    EXPECT_EQ(replay.str().find("retired_lib"), std::string::npos);
    fs::remove_all(dir);
}

int destroyedUtilityCreators = 0;

class TrackedUtilityCreator : public IUtilityCreator
{
public:
    explicit TrackedUtilityCreator(std::unique_ptr<IUtilityCreator> inner)
            : _inner(std::move(inner))
    {
    }
    ~TrackedUtilityCreator() override { ++destroyedUtilityCreators; }
    std::shared_ptr<BasicUtilityFunction> createUtility(int64_t id, UtilityFunctionContext& context) override { return _inner->createUtility(id, context); }

private:
    std::unique_ptr<IUtilityCreator> _inner;
};

class TrackedTeamFactory : public DynamicTeamFactory
{
public:
    using DynamicTeamFactory::DynamicTeamFactory;
    AlicaCreators creators() override
    {
        AlicaCreators c = DynamicTeamFactory::creators();
        return AlicaCreators(std::move(c.conditionCreator), std::make_unique<TrackedUtilityCreator>(std::move(c.utilityCreator)),
                std::move(c.constraintCreator), std::move(c.behaviourCreator), std::move(c.planCreator), std::move(c.transitionConditionCreator));
    }
};

TEST(MatchTest, TheCreatorsLiveUntilTheEngineIsDestroyed)
{
    destroyedUtilityCreators = 0;
    auto match = makeMatch(testConfig(1, 2, 0.5), nullptr, [](const TeamConfig& team) { return std::make_unique<TrackedTeamFactory>(team); });
    match->start();
    EXPECT_EQ(destroyedUtilityCreators, 0);
    while (!match->finished()) {
        match->tick();
    }
    match->stop();
    EXPECT_EQ(destroyedUtilityCreators, 0);
    match.reset();
    EXPECT_EQ(destroyedUtilityCreators, 2);
}

TEST(MatchTest, AnAgentStoppingItselfInACallbackStopsAfterTheCallbackReturns)
{
    auto match = makeMatch(testConfig(1, 2, 1.5));
    match->start();
    Agent& agent = match->agent("t0p0");
    bool runningRightAfterStop = false;
    io(*match, "t0p0").onEvent = [&](const std::string& what) {
        if (what == "Walk.init") {
            agent.stop();
            runningRightAfterStop = agent.running();
        }
    };
    while (!match->finished()) {
        match->tick();
    }
    EXPECT_TRUE(runningRightAfterStop);
    EXPECT_FALSE(agent.running());
    EXPECT_EQ(io(*match, "t0p0").events.back(), (std::pair<std::string, int64_t>{"Walk.init", kPeriod}));
    EXPECT_EQ(io(*match, "t0p1").events.back(), (std::pair<std::string, int64_t>{"Rest.init", 31 * kPeriod}));
    match->stop();
}

TEST(MatchTest, APlayerWithoutARoleIsAConfigError)
{
    MatchConfig config = testConfig(1, 1, 1);
    config.teams[0].players[0].local = YAML::Node(YAML::NodeType::Map);
    auto match = makeMatch(config);
    EXPECT_THROW(match->start(), ConfigError);
}

} // namespace
} // namespace alica::sim
