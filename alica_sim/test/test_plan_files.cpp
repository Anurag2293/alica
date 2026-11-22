#include <alica_sim/Match.h>
#include <alica_sim/PlanFiles.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace alica::sim
{
namespace
{

namespace fs = std::filesystem;

class PlanFilesTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        root = fs::temp_directory_path() / ("alica_sim_plan_files_" + std::string(::testing::UnitTest::GetInstance()->current_test_info()->name()));
        fs::remove_all(root);
        fs::create_directories(root);
    }
    void TearDown() override { fs::remove_all(root); }

    std::vector<std::string> relatives(const std::string& folder)
    {
        std::vector<std::string> out;
        for (const PlanFile& file : listFiles({folder})) {
            out.push_back(file.relative);
        }
        return out;
    }

    const fs::path plans = fs::path(ALICA_SIM_TEST_DIR) / "team/plans";
    fs::path root;
};

TEST_F(PlanFilesTest, FollowsSymlinkedDirectoriesLikeTheEngine)
{
    fs::create_directories(root / "real/sub");
    fs::copy(plans, root / "real/sub", fs::copy_options::recursive);
    fs::create_directories(root / "linked");
    fs::create_directory_symlink(root / "real", root / "linked/team");
    fs::create_directories(root / "copied/team");
    fs::copy(root / "real", root / "copied/team", fs::copy_options::recursive);

    const std::vector<std::string> files = relatives((root / "linked").string());
    EXPECT_NE(std::find(files.begin(), files.end(), "team/sub/TestMaster.pml"), files.end());
    EXPECT_EQ(findPlanFile({(root / "linked").string()}, "Walk.beh"), (root / "linked/team/sub/Walk.beh").string());
    EXPECT_EQ(Match::planHash({(root / "linked").string()}), Match::planHash({(root / "copied").string()}));
    EXPECT_EQ(planTreeLibraries({(root / "linked").string()}, "TestMaster"), (std::set<std::string>{"alica_sim_test_team"}));
}

TEST_F(PlanFilesTest, ASymlinkCycleIsVisitedOnce)
{
    fs::copy(plans, root, fs::copy_options::recursive);
    fs::create_directory_symlink(root, root / "loop");
    const std::vector<std::string> files = relatives(root.string());
    EXPECT_EQ(std::count(files.begin(), files.end(), "TestMaster.pml"), 1);
    EXPECT_EQ(std::count_if(files.begin(), files.end(), [](const std::string& f) { return f.rfind("loop/", 0) == 0; }), 0);
}

TEST_F(PlanFilesTest, LibrariesComeFromThePlanTreeOnly)
{
    fs::copy(plans, root, fs::copy_options::recursive);
    std::ofstream(root / "Old.pml") << R"({"id": 1, "name": "Old", "libraryName": "retired_lib", "states": []})";
    EXPECT_EQ(planTreeLibraries({root.string()}, "TestMaster"), (std::set<std::string>{"alica_sim_test_team"}));
}

TEST_F(PlanFilesTest, AMissingFileInThePlanTreeIsAConfigError)
{
    fs::copy(plans, root, fs::copy_options::recursive);
    fs::remove(root / "Walk.beh");
    EXPECT_THROW(planTreeLibraries({root.string()}, "TestMaster"), ConfigError);
}

TEST_F(PlanFilesTest, FindsTheNamedOrTheDefaultRoleSet)
{
    fs::copy(plans, root, fs::copy_options::recursive);
    EXPECT_EQ(findRoleSetFile({root.string()}, "RoleSet"), (root / "RoleSet.rst").string());
    EXPECT_EQ(findRoleSetFile({root.string()}, "Other"), (root / "RoleSet.rst").string());
    EXPECT_EQ(roleNames((root / "RoleSet.rst").string()), (std::set<std::string>{"Player"}));
    fs::remove(root / "RoleSet.rst");
    EXPECT_EQ(findRoleSetFile({root.string()}, "RoleSet"), "");
}

} // namespace
} // namespace alica::sim
