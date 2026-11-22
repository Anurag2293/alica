#include "alica_sim/Numeric.h"

#include <yaml-cpp/yaml.h>

#include <cstdint>
#include <cstring>

namespace alica::sim
{

bool isFinite(double v)
{
    uint64_t bits;
    std::memcpy(&bits, &v, sizeof(bits));
    return (bits & 0x7ff0000000000000ULL) != 0x7ff0000000000000ULL;
}

bool decodeFiniteDouble(const YAML::Node& node, double& out)
{
    if (!node.IsScalar() || YAML::conversion::IsInfinity(node.Scalar()) || YAML::conversion::IsNegativeInfinity(node.Scalar()) ||
            YAML::conversion::IsNaN(node.Scalar())) {
        return false;
    }
    double d = 0;
    if (!YAML::convert<double>::decode(node, d) || !isFinite(d)) {
        return false;
    }
    out = d;
    return true;
}

} // namespace alica::sim
