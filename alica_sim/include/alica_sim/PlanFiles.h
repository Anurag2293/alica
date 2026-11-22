#pragma once

#include <filesystem>
#include <set>
#include <string>
#include <vector>

namespace alica::sim
{

struct PlanFile
{
    std::string relative;
    std::string path;
};

bool isPlanFile(const std::filesystem::path& path);

// Every file under the folders, following symlinked directories like the engine's FileSystem::findFile,
// in folder order and then the engine's search order (scandir with alphasort, so the process locale's
// collation). A symlink cycle is a ConfigError.
std::vector<PlanFile> listFiles(const std::vector<std::string>& folders);

std::string findPlanFile(const std::vector<std::string>& folders, const std::string& fileName);
std::string findRoleSetFile(const std::vector<std::string>& folders, const std::string& roleSet);
std::set<std::string> roleNames(const std::string& roleSetFile);

// The library names the engine loads for the plan tree of masterPlan.
std::set<std::string> planTreeLibraries(const std::vector<std::string>& folders, const std::string& masterPlan);

} // namespace alica::sim
