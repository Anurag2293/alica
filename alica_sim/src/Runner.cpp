#include "alica_sim/Runner.h"
#include "alica_sim/PlanFiles.h"

#include <DynamicBehaviourCreator.h>
#include <DynamicConditionCreator.h>
#include <DynamicConstraintCreator.h>
#include <DynamicLoadingUtils.h>
#include <DynamicPlanCreator.h>
#include <DynamicTransitionConditionCreator.h>
#include <DynamicUtilityFunctionCreator.h>

#include <dlfcn.h>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <thread>

namespace alica::sim
{

namespace
{

namespace fs = std::filesystem;

std::string join(const std::vector<std::string>& parts, char sep)
{
    std::string out;
    for (const std::string& p : parts) {
        if (!out.empty()) {
            out += sep;
        }
        out += p;
    }
    return out;
}

class ScopedLibraryPath
{
public:
    explicit ScopedLibraryPath(const std::string& value)
    {
        const char* old = std::getenv("LD_LIBRARY_PATH");
        if (old) {
            _old = old;
        }
        setenv("LD_LIBRARY_PATH", value.c_str(), 1);
    }
    ~ScopedLibraryPath()
    {
        if (_old) {
            setenv("LD_LIBRARY_PATH", _old->c_str(), 1);
        } else {
            unsetenv("LD_LIBRARY_PATH");
        }
    }

private:
    std::optional<std::string> _old;
};

} // namespace

std::string sharedLibraryFileName(const std::string& name)
{
#ifdef __APPLE__
    return "lib" + name + ".dylib";
#else
    return "lib" + name + ".so";
#endif
}

LoadedWorld loadWorld(const std::string& plugin, const std::vector<std::string>& searchPath)
{
    const std::string fileName = sharedLibraryFileName(plugin);
    for (const std::string& dir : searchPath) {
        fs::path file = fs::path(dir) / fileName;
        if (!fs::exists(file)) {
            continue;
        }
        void* handle = dlopen(file.c_str(), RTLD_NOW | RTLD_GLOBAL);
        if (!handle) {
            throw std::runtime_error("cannot load world plugin " + file.string() + ": " + dlerror());
        }
        auto create = reinterpret_cast<CreateWorldFn>(dlsym(handle, ALICA_SIM_WORLD_SYMBOL));
        if (!create) {
            throw std::runtime_error("world plugin " + file.string() + " does not export " ALICA_SIM_WORLD_SYMBOL);
        }
        std::unique_ptr<World> world(create());
        if (!world) {
            throw std::runtime_error("world plugin " + file.string() + " returned no world");
        }
        return {std::move(world), fs::absolute(file).lexically_normal().string()};
    }
    throw std::runtime_error("world plugin " + fileName + " not found in: " + join(searchPath, ':'));
}

DynamicTeamFactory::DynamicTeamFactory(const TeamConfig& team)
        : _libraries(team.libraries)
        , _plans(team.plans)
        , _masterPlan(team.masterPlan)
{
}

std::vector<std::string> DynamicTeamFactory::librarySearchPath() const
{
    std::vector<std::string> path = _libraries;
    if (const char* env = std::getenv("LD_LIBRARY_PATH")) {
        for (const std::string& dir : tokenizeStr(env, ':')) {
            path.push_back(dir);
        }
    }
    return path;
}

AlicaCreators DynamicTeamFactory::creators()
{
    ScopedLibraryPath scope(join(librarySearchPath(), ':'));
    return AlicaCreators(std::make_unique<DynamicConditionCreator>(), std::make_unique<DynamicUtilityFunctionCreator>(),
            std::make_unique<DynamicConstraintCreator>(), std::make_unique<DynamicBehaviourCreator>(), std::make_unique<DynamicPlanCreator>(),
            std::make_unique<DynamicTransitionConditionCreator>());
}

std::vector<std::pair<std::string, std::string>> DynamicTeamFactory::libraries() const
{
    const std::vector<std::string> path = librarySearchPath();
    std::vector<std::pair<std::string, std::string>> out;
    for (const std::string& name : planTreeLibraries(_plans, _masterPlan)) {
        out.emplace_back(name, fs::absolute(calculateLibraryCompleteName(path, name)).lexically_normal().string());
    }
    return out;
}

void runMatch(Match& match, double speed)
{
    match.start();
    const auto wallStart = std::chrono::steady_clock::now();
    while (!match.finished()) {
        if (speed > 0) {
            auto due = std::chrono::nanoseconds(static_cast<int64_t>(static_cast<double>(match.now().inNanoseconds()) / speed));
            std::this_thread::sleep_until(wallStart + due);
        }
        match.tick();
    }
    match.stop();
}

} // namespace alica::sim
