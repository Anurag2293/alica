#include <alica_sim/Match.h>
#include <alica_sim/PlanFiles.h>

#include <engine/FileSystem.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <clocale>
#include <cstring>
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

TEST_F(PlanFilesTest, ASymlinkCycleIsAConfigError)
{
    fs::copy(plans, root, fs::copy_options::recursive);
    fs::create_directories(root / "sub");
    fs::create_directory_symlink(root, root / "sub/loop");
    EXPECT_THROW(listFiles({root.string()}), ConfigError);
}

TEST_F(PlanFilesTest, TheSameFolderLinkedTwiceIsNotACycle)
{
    fs::create_directories(root / "real");
    fs::copy(plans, root / "real", fs::copy_options::recursive);
    fs::create_directory_symlink(root / "real", root / "one");
    fs::create_directory_symlink(root / "real", root / "two");
    const std::vector<std::string> files = relatives(root.string());
    EXPECT_EQ(std::count(files.begin(), files.end(), "one/TestMaster.pml"), 1);
    EXPECT_EQ(std::count(files.begin(), files.end(), "two/TestMaster.pml"), 1);
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

TEST_F(PlanFilesTest, PicksTheSameFileAsTheEngineUnderTheLocaleCollation)
{
    const std::string previous = std::setlocale(LC_COLLATE, nullptr);
    std::string chosen;
    for (const char* locale : {"en_US.UTF-8", "en_US.utf8", "de_DE.UTF-8", "de_DE.utf8", "C.UTF-8", "C.utf8"}) {
        if (std::setlocale(LC_COLLATE, locale) && std::strcoll("a", "B") < 0) {
            chosen = locale;
            break;
        }
    }
    if (chosen.empty()) {
        std::setlocale(LC_COLLATE, previous.c_str());
        GTEST_SKIP() << "no locale that collates a before B";
    }
    for (const char* sub : {"B", "a"}) {
        fs::create_directories(root / sub);
        fs::copy(plans / "Walk.beh", root / sub / "Walk.beh");
    }
    std::string engineFound;
    const bool engineHit = essentials::FileSystem::findFile(root.string(), "Walk.beh", engineFound);
    const std::string walkFound = findPlanFile({root.string()}, "Walk.beh");
    const std::vector<std::string> files = relatives(root.string());
    std::setlocale(LC_COLLATE, previous.c_str());

    ASSERT_TRUE(engineHit);
    EXPECT_EQ(walkFound, engineFound) << chosen;
    EXPECT_EQ(walkFound, (root / "a/Walk.beh").string()) << chosen;
    EXPECT_EQ(files, (std::vector<std::string>{"a/Walk.beh", "B/Walk.beh"})) << chosen;
}

} // namespace
} // namespace alica::sim
