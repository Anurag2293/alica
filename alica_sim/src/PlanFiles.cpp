#include "alica_sim/PlanFiles.h"
#include "alica_sim/MatchConfig.h"

#include <dirent.h>
#include <sys/stat.h>
#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <cstdlib>
#include <deque>
#include <functional>
#include <map>
#include <utility>

namespace alica::sim
{

namespace
{

using Visit = std::function<bool(const std::string& path, const std::string& relative)>;
using DirId = std::pair<dev_t, ino_t>;

std::vector<std::string> sortedNames(const std::string& dir)
{
    std::vector<std::string> names;
    dirent** list = nullptr;
    const int n = scandir(dir.c_str(), &list, nullptr, alphasort);
    for (int i = 0; i < n; ++i) {
        std::string name = list[i]->d_name;
        if (name != "." && name != "..") {
            names.push_back(std::move(name));
        }
        free(list[i]);
    }
    free(list);
    return names;
}

bool walk(const std::string& dir, const std::string& relative, std::vector<DirId>& ancestors, const Visit& visit)
{
    struct stat st;
    if (stat(dir.c_str(), &st) != 0 || !S_ISDIR(st.st_mode)) {
        return false;
    }
    const DirId id{st.st_dev, st.st_ino};
    if (std::find(ancestors.begin(), ancestors.end(), id) != ancestors.end()) {
        throw ConfigError(dir + " is a symlink cycle; the engine's file search would never end");
    }
    ancestors.push_back(id);
    bool stop = false;
    for (const std::string& name : sortedNames(dir)) {
        const std::string path = dir + "/" + name;
        const std::string rel = relative.empty() ? name : relative + "/" + name;
        struct stat entry;
        if (stat(path.c_str(), &entry) != 0) {
            continue;
        }
        if (S_ISDIR(entry.st_mode)) {
            stop = walk(path, rel, ancestors, visit);
        } else if (S_ISREG(entry.st_mode)) {
            stop = visit(path, rel);
        }
        if (stop) {
            break;
        }
    }
    ancestors.pop_back();
    return stop;
}

bool walk(const std::string& folder, const Visit& visit)
{
    std::vector<DirId> ancestors;
    return walk(folder, "", ancestors, visit);
}

YAML::Node load(const std::string& path)
{
    try {
        return YAML::LoadFile(path);
    } catch (const YAML::Exception& e) {
        throw ConfigError("cannot parse " + path + ": " + e.what());
    }
}

struct Reference
{
    std::string file;
    int64_t id;
};

bool parseReference(const std::string& s, Reference& ref)
{
    const size_t hash = s.find_last_of('#');
    if (hash == std::string::npos) {
        return false;
    }
    std::string file = s.substr(0, hash);
    if (const size_t slash = file.find_last_of('/'); slash != std::string::npos) {
        file = file.substr(slash + 1);
    }
    if (!isPlanFile(file)) {
        return false;
    }
    try {
        ref = {file, std::stoll(s.substr(hash + 1))};
    } catch (const std::exception&) {
        return false;
    }
    return true;
}

void scan(const YAML::Node& node, bool collectLibraries, std::set<std::string>& libraries, std::vector<Reference>& references)
{
    if (node.IsMap()) {
        for (const auto& kv : node) {
            if (collectLibraries && kv.first.Scalar() == "libraryName" && kv.second.IsScalar()) {
                if (!kv.second.Scalar().empty()) {
                    libraries.insert(kv.second.Scalar());
                }
            } else {
                scan(kv.second, collectLibraries, libraries, references);
            }
        }
    } else if (node.IsSequence()) {
        for (const auto& item : node) {
            scan(item, collectLibraries, libraries, references);
        }
    } else if (node.IsScalar()) {
        Reference ref;
        if (parseReference(node.Scalar(), ref)) {
            references.push_back(ref);
        }
    }
}

std::string extension(const std::string& file)
{
    return std::filesystem::path(file).extension().string();
}

} // namespace

bool isPlanFile(const std::filesystem::path& path)
{
    static const std::set<std::string> extensions{".pml", ".beh", ".pty", ".plh", ".tsk", ".cnd", ".rst"};
    return extensions.count(path.extension().string()) > 0;
}

std::vector<PlanFile> listFiles(const std::vector<std::string>& folders)
{
    std::vector<PlanFile> files;
    for (const std::string& folder : folders) {
        walk(folder, [&](const std::string& path, const std::string& relative) {
            files.push_back({relative, path});
            return false;
        });
    }
    return files;
}

std::string findPlanFile(const std::vector<std::string>& folders, const std::string& fileName)
{
    std::string found;
    for (const std::string& folder : folders) {
        const bool hit = walk(folder, [&](const std::string& path, const std::string& relative) {
            if (std::filesystem::path(relative).filename() == fileName) {
                found = path;
                return true;
            }
            return false;
        });
        if (hit) {
            return found;
        }
    }
    return {};
}

std::string findRoleSetFile(const std::vector<std::string>& folders, const std::string& roleSet)
{
    if (std::string file = findPlanFile(folders, roleSet + ".rst"); !file.empty()) {
        return file;
    }
    for (const std::string& folder : folders) {
        for (const std::string& name : sortedNames(folder)) {
            const std::string path = folder + "/" + name;
            struct stat st;
            if (extension(name) != ".rst" || stat(path.c_str(), &st) != 0 || !S_ISREG(st.st_mode)) {
                continue;
            }
            try {
                YAML::Node node = YAML::LoadFile(path);
                if (node["defaultRoleSet"] && node["defaultRoleSet"].as<bool>()) {
                    return path;
                }
            } catch (const YAML::Exception&) {
            }
        }
    }
    return {};
}

std::set<std::string> roleNames(const std::string& roleSetFile)
{
    std::set<std::string> names;
    for (const auto& role : load(roleSetFile)["roles"]) {
        if (role["name"]) {
            names.insert(role["name"].as<std::string>());
        }
    }
    return names;
}

std::set<std::string> planTreeLibraries(const std::vector<std::string>& folders, const std::string& masterPlan)
{
    std::set<std::string> libraries;
    std::deque<std::string> queue{masterPlan + ".pml"};
    std::set<std::string> seen{queue.front()};
    std::map<std::string, std::set<int64_t>> conditionIds;
    std::map<std::string, std::string> conditionFiles;
    while (!queue.empty()) {
        const std::string name = queue.front();
        queue.pop_front();
        const std::string path = findPlanFile(folders, name);
        if (path.empty()) {
            throw ConfigError("cannot find " + name + " in the plan tree of " + masterPlan);
        }
        const std::string ext = extension(name);
        if (ext == ".cnd") {
            conditionFiles[name] = path;
            continue;
        }
        std::vector<Reference> references;
        scan(load(path), ext == ".pml" || ext == ".beh", libraries, references);
        for (const Reference& ref : references) {
            if (extension(ref.file) == ".cnd") {
                conditionIds[ref.file].insert(ref.id);
            }
            if (seen.insert(ref.file).second) {
                queue.push_back(ref.file);
            }
        }
    }
    for (const auto& [name, path] : conditionFiles) {
        const std::set<int64_t>& ids = conditionIds[name];
        for (const auto& condition : load(path)["conditions"]) {
            if (condition["id"] && ids.count(condition["id"].as<int64_t>()) && condition["name"].as<std::string>("") != "DefaultCondition" &&
                    !condition["libraryName"].as<std::string>("").empty()) {
                libraries.insert(condition["libraryName"].as<std::string>());
            }
        }
    }
    return libraries;
}

} // namespace alica::sim
