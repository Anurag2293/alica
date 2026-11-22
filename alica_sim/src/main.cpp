#include "alica_sim/Match.h"
#include "alica_sim/MatchConfig.h"
#include "alica_sim/Runner.h"

#include <unistd.h>

#include <cstdio>
#include <cstring>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace
{

const char* const kUsage = "usage: alica_sim_run MATCH.yaml [--out replay.jsonl] [--plugin-path DIR]... [--speed X] [--seed N] [--work DIR]\n";

int usage(const std::string& error)
{
    std::cerr << "alica_sim_run: " << error << "\n" << kUsage;
    return 2;
}

} // namespace

int main(int argc, char** argv)
{
    std::string configPath;
    std::string outPath;
    std::string workDir;
    std::vector<std::string> pluginPath;
    double speed = 0;
    bool haveSeed = false;
    uint64_t seed = 0;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        auto next = [&]() -> std::string {
            if (i + 1 >= argc) {
                throw std::invalid_argument(arg + " needs a value");
            }
            return argv[++i];
        };
        try {
            if (arg == "--out") {
                outPath = next();
            } else if (arg == "--plugin-path") {
                pluginPath.push_back(next());
            } else if (arg == "--speed") {
                speed = std::stod(next());
            } else if (arg == "--seed") {
                seed = std::stoull(next());
                haveSeed = true;
            } else if (arg == "--work") {
                workDir = next();
            } else if (arg == "-h" || arg == "--help") {
                std::cout << kUsage;
                return 0;
            } else if (!arg.empty() && arg[0] == '-') {
                return usage("unknown option " + arg);
            } else if (configPath.empty()) {
                configPath = arg;
            } else {
                return usage("more than one match config");
            }
        } catch (const std::exception& e) {
            return usage(arg + ": " + e.what());
        }
    }
    if (configPath.empty()) {
        return usage("no match config");
    }
    if (speed < 0) {
        return usage("--speed must not be negative");
    }

    // The engine logs to stdout; keep stdout for the result line only.
    std::fflush(stdout);
    int resultFd = dup(STDOUT_FILENO);
    dup2(STDERR_FILENO, STDOUT_FILENO);

    try {
        alica::sim::MatchConfig config = alica::sim::loadMatchConfig(configPath);
        if (haveSeed) {
            config.seed = seed;
        }
        if (pluginPath.empty()) {
            pluginPath.push_back(std::filesystem::absolute(configPath).parent_path().string());
        }
        alica::sim::LoadedWorld world = alica::sim::loadWorld(config.worldPlugin, pluginPath);

        std::ofstream out;
        alica::sim::MatchOptions options;
        options.workDir = workDir;
        options.worldLibrary = world.file;
        if (!outPath.empty()) {
            out.open(outPath, std::ios::binary);
            if (!out) {
                throw std::runtime_error("cannot write " + outPath);
            }
            options.replay = &out;
        }

        alica::sim::Match match(
                config, std::move(world.world), [](const alica::sim::TeamConfig& team) { return std::make_unique<alica::sim::DynamicTeamFactory>(team); },
                options);
        alica::sim::runMatch(match, speed);
        if (out.is_open()) {
            out.close();
            if (!out) {
                throw std::runtime_error("cannot write " + outPath);
            }
        }

        std::string line = match.resultLine() + "\n";
        if (write(resultFd, line.data(), line.size()) != static_cast<ssize_t>(line.size())) {
            return 1;
        }
    } catch (const std::exception& e) {
        std::cerr << "alica_sim_run: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
