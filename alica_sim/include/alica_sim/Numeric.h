#pragma once

namespace YAML
{
class Node;
}

namespace alica::sim
{

// Compiled without -ffinite-math-only in its own source file, because -ffast-math lets the compiler fold
// any NaN or infinity check, even one on the bit pattern, to "finite".
bool isFinite(double v);

// Rejects the NaN and infinity spellings by name, because a -ffast-math copy of yaml-cpp's decoder turns them into 0.
bool decodeFiniteDouble(const YAML::Node& node, double& out);

} // namespace alica::sim
