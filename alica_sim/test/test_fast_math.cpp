#include <alica_sim/JsonWriter.h>
#include <alica_sim/Numeric.h>

#include <gtest/gtest.h>
#include <yaml-cpp/yaml.h>

#include <cstdlib>
#include <string>

namespace alica::sim
{
namespace
{

double parse(const char* s)
{
    return std::strtod(s, nullptr);
}

TEST(FastMathTest, NonFiniteDoublesAreDetected)
{
    EXPECT_FALSE(isFinite(parse("inf")));
    EXPECT_FALSE(isFinite(parse("-inf")));
    EXPECT_FALSE(isFinite(parse("nan")));
    EXPECT_TRUE(isFinite(parse("1e308")));
    EXPECT_TRUE(isFinite(parse("-0")));
    EXPECT_EQ(JsonWriter::formatDouble(parse("inf")), "null");
    EXPECT_EQ(JsonWriter::formatDouble(parse("-inf")), "null");
    EXPECT_EQ(JsonWriter::formatDouble(parse("nan")), "null");
}

TEST(FastMathTest, NonFiniteYamlScalarsStayStrings)
{
    JsonWriter w;
    w.value(YAML::Load("[.inf, -.inf, .nan, 1.5]"));
    EXPECT_EQ(w.str(), R"([".inf","-.inf",".nan",1.5])");
}

} // namespace
} // namespace alica::sim
