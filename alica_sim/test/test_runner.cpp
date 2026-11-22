#include <gtest/gtest.h>

#include <sys/wait.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace
{

namespace fs = std::filesystem;

class RunnerTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        dir = fs::temp_directory_path() / ("alica_sim_runner_" + std::string(::testing::UnitTest::GetInstance()->current_test_info()->name()));
        fs::remove_all(dir);
        fs::create_directories(dir);
    }
    void TearDown() override { fs::remove_all(dir); }

    std::string writeConfig(const std::string& role)
    {
        const fs::path file = dir / "match.yaml";
        std::ofstream(file) << "{world: {plugin: alica_sim_test_world}, dt: 0.01, duration: 0.1, teams: [{name: T, plans: " << ALICA_SIM_TEST_DIR
                            << "/team/plans, libraries: [" << ALICA_SIM_TEST_TEAM_DIR
                            << "], masterPlan: TestMaster, players: [{name: p, id: 1, local: {DefaultRole: " << role << "}}]}]}\n";
        return file.string();
    }

    int run(const std::string& args, const std::string& shellPrefix = "")
    {
        std::ostringstream command;
        command << shellPrefix << "'" << ALICA_SIM_RUN << "' " << args << " --plugin-path '" << ALICA_SIM_TEST_WORLD_DIR << "' >'" << (dir / "stdout").string()
                << "' 2>'" << (dir / "stderr").string() << "'";
        const int status = std::system(command.str().c_str());
        return WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status);
    }

    std::string read(const std::string& name)
    {
        std::ifstream in(dir / name);
        return std::string(std::istreambuf_iterator<char>(in), {});
    }

    fs::path dir;
};

TEST_F(RunnerTest, RunsAMatch)
{
    EXPECT_EQ(run(writeConfig("Player")), 0) << read("stderr");
    EXPECT_NE(read("stdout").find("\"ticks\":10}"), std::string::npos);
}

TEST_F(RunnerTest, AnUnknownRoleExitsWithAConfigError)
{
    EXPECT_EQ(run(writeConfig("Goalie")), 1);
    EXPECT_NE(read("stderr").find("role Goalie"), std::string::npos) << read("stderr");
    EXPECT_EQ(read("stdout"), "");
}

TEST_F(RunnerTest, AReplayThatCannotBeOpenedExitsWithAnError)
{
    EXPECT_EQ(run(writeConfig("Player") + " --out '" + (dir / "missing/replay.jsonl").string() + "'"), 1);
    EXPECT_NE(read("stderr").find("cannot write"), std::string::npos) << read("stderr");
}

TEST_F(RunnerTest, AReplayThatCannotBeWrittenExitsWithAnError)
{
    EXPECT_EQ(run(writeConfig("Player") + " --out '" + (dir / "replay.jsonl").string() + "'", "trap '' XFSZ; ulimit -f 1; "), 1);
    EXPECT_NE(read("stderr").find("cannot write"), std::string::npos) << read("stderr");
    EXPECT_EQ(read("stdout"), "");
}

TEST_F(RunnerTest, AFullDeviceExitsWithAnError)
{
    if (!fs::exists("/dev/full")) {
        GTEST_SKIP() << "no /dev/full";
    }
    EXPECT_EQ(run(writeConfig("Player") + " --out /dev/full"), 1);
    EXPECT_NE(read("stderr").find("cannot write"), std::string::npos) << read("stderr");
}

} // namespace
