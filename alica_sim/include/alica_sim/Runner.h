#pragma once

#include "alica_sim/Agent.h"
#include "alica_sim/Match.h"
#include "alica_sim/World.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace alica::sim
{

struct LoadedWorld
{
    std::unique_ptr<World> world;
    std::string file;
};

std::string sharedLibraryFileName(const std::string& name);
LoadedWorld loadWorld(const std::string& plugin, const std::vector<std::string>& searchPath);

class DynamicTeamFactory : public TeamFactory
{
public:
    explicit DynamicTeamFactory(const TeamConfig& team);
    AlicaCreators creators() override;
    std::vector<std::pair<std::string, std::string>> libraries() const override;

private:
    std::vector<std::string> librarySearchPath() const;

    std::vector<std::string> _libraries;
    std::vector<std::string> _plans;
};

void runMatch(Match& match, double speed);

} // namespace alica::sim
