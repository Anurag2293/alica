#include <alica_sim/JsonWriter.h>
#include <alica_sim/MatchConfig.h>
#include <alica_sim/Sha256.h>

#include <gtest/gtest.h>
#include <yaml-cpp/yaml.h>

#include <string>

namespace alica::sim
{
namespace
{

TEST(Sha256Test, KnownVectors)
{
    EXPECT_EQ(Sha256::hex(""), "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    EXPECT_EQ(Sha256::hex("abc"), "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    EXPECT_EQ(Sha256::hex("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"), "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
    Sha256 million;
    std::string chunk(1000, 'a');
    for (int i = 0; i < 1000; ++i) {
        million.update(chunk);
    }
    EXPECT_EQ(million.hexDigest(), "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
}

TEST(JsonWriterTest, WritesNestedValues)
{
    JsonWriter w;
    w.beginObject();
    w.field("s", "a\"b\\c\n\x01");
    w.field("i", int64_t(-3));
    w.field("b", true);
    w.key("list").beginArray().value(1).null().beginObject().endObject().endArray();
    w.endObject();
    EXPECT_EQ(w.str(), R"({"s":"a\"b\\c\n\u0001","i":-3,"b":true,"list":[1,null,{}]})");
}

TEST(JsonWriterTest, DoublesHaveAFixedFormat)
{
    EXPECT_EQ(JsonWriter::formatDouble(0.0), "0");
    EXPECT_EQ(JsonWriter::formatDouble(-0.0), "0");
    EXPECT_EQ(JsonWriter::formatDouble(1.5), "1.5");
    EXPECT_EQ(JsonWriter::formatDouble(0.1 + 0.2), "0.3");
    EXPECT_EQ(JsonWriter::formatDouble(-2.0), "-2");
    EXPECT_EQ(JsonWriter::formatDouble(1e-7), "0");
    EXPECT_EQ(JsonWriter::formatDouble(123456.1234567), "123456.123457");
    EXPECT_EQ(JsonWriter::formatDouble(1.0 / 0.0), "null");
}

TEST(JsonWriterTest, WritesYamlWithScalarTypes)
{
    JsonWriter w;
    w.value(YAML::Load(R"({a: 1, b: 2.5, c: true, d: text, e: "7", f: [x, ~], g: {}})"));
    EXPECT_EQ(w.str(), R"({"a":1,"b":2.5,"c":true,"d":"text","e":"7","f":["x",null],"g":{}})");
}

YAML::Node matchYaml(const std::string& players)
{
    return YAML::Load("{world: {plugin: w}, duration: 1, teams: [{name: T, plans: p, masterPlan: M, players: " + players + "}]}");
}

TEST(MatchConfigTest, ResolvesPathsAndDefaults)
{
    MatchConfig c = parseMatchConfig(
            YAML::Load("{world: {plugin: w}, duration: 2, engine: {Alica: {EngineFrequency: 20}, Alica.AllowIdling: true}, teams: [{name: T, plans: [p, /abs], "
                       "libraries: lib, masterPlan: M, players: [{name: a, id: 1, local: {DefaultRole: R}}]}]}"),
            "/base");
    EXPECT_EQ(c.dt, AlicaTime::milliseconds(10));
    EXPECT_EQ(c.duration, AlicaTime::seconds(2));
    EXPECT_EQ(c.teams[0].plans, (std::vector<std::string>{"/base/p", "/abs"}));
    EXPECT_EQ(c.teams[0].libraries, (std::vector<std::string>{"/base/lib"}));
    EXPECT_EQ(c.teams[0].roleSet, "RoleSet");
    ASSERT_EQ(c.engine.size(), 2u);
    EXPECT_EQ(c.engine[0].first, "Alica.EngineFrequency");
    EXPECT_EQ(c.engine[1].first, "Alica.AllowIdling");

    YAML::Node engine = engineConfig(c, c.teams[0].players[0]);
    EXPECT_EQ(engine["Alica"]["EngineFrequency"].as<int>(), 20);
    EXPECT_TRUE(engine["Alica"]["AllowIdling"].as<bool>());
    EXPECT_TRUE(engine["Alica"]["SimulatedTime"].as<bool>());
    EXPECT_EQ(engine["Alica"]["TeamTimeOut"].as<int>(), 10000);
    EXPECT_EQ(engine["Local"]["DefaultRole"].as<std::string>(), "R");
}

TEST(MatchConfigTest, SimulatedTimeCannotBeTurnedOff)
{
    MatchConfig c = parseMatchConfig(
            YAML::Load("{world: {plugin: w}, duration: 1, engine: {Alica.SimulatedTime: false}, teams: [{name: T, plans: p, masterPlan: M, players: [{name: a, "
                       "id: 1}]}]}"),
            "/base");
    EXPECT_TRUE(engineConfig(c, c.teams[0].players[0])["Alica"]["SimulatedTime"].as<bool>());
}

TEST(MatchConfigTest, RejectsBadPlayers)
{
    EXPECT_NO_THROW(parseMatchConfig(matchYaml("[{name: a-1_B, id: 1}, {name: b, id: 2}]"), "/"));
    EXPECT_THROW(parseMatchConfig(matchYaml("[{name: a, id: 1}, {name: A, id: 2}]"), "/"), ConfigError);
    EXPECT_THROW(parseMatchConfig(matchYaml("[{name: a, id: 1}, {name: b, id: 1}]"), "/"), ConfigError);
    EXPECT_THROW(parseMatchConfig(matchYaml("[{name: a.b, id: 1}]"), "/"), ConfigError);
    EXPECT_THROW(parseMatchConfig(matchYaml("[{name: a, id: 0}]"), "/"), ConfigError);
    EXPECT_THROW(parseMatchConfig(matchYaml("[]"), "/"), ConfigError);
    EXPECT_THROW(parseMatchConfig(YAML::Load("{world: {plugin: w}, teams: []}"), "/"), ConfigError);
}

MatchConfig timedConfig(const std::string& dt, const std::string& duration)
{
    return parseMatchConfig(YAML::Load("{world: {plugin: w}, dt: " + dt + ", duration: " + duration +
                                       ", teams: [{name: T, plans: p, masterPlan: M, players: [{name: a, id: 1}]}]}"),
            "/");
}

TEST(MatchConfigTest, TimesAreExactNanoseconds)
{
    EXPECT_EQ(timedConfig("0.016666666", "1").dt, AlicaTime::nanoseconds(16666666));
    EXPECT_EQ(timedConfig("0.0166666665", "1").dt, AlicaTime::nanoseconds(16666667));
    EXPECT_EQ(timedConfig("1e-3", "1").dt, AlicaTime::milliseconds(1));
    EXPECT_EQ(timedConfig("0.000000001", "3600").duration, AlicaTime::seconds(3600));
    EXPECT_EQ(timedConfig("0.01", "4000000000").duration, AlicaTime::seconds(4000000000));
    EXPECT_EQ(formatSeconds(AlicaTime::nanoseconds(16666666)), "0.016666666");
    EXPECT_EQ(formatSeconds(AlicaTime::milliseconds(1500)), "1.5");
    EXPECT_EQ(formatSeconds(AlicaTime::seconds(2)), "2");
    EXPECT_EQ(formatSeconds(AlicaTime::zero()), "0");
}

TEST(MatchConfigTest, RejectsTimesThatDoNotGiveAtLeastOneTick)
{
    for (const auto& [dt, duration] : std::vector<std::pair<std::string, std::string>>{{"1e-10", "1"}, {"0", "1"}, {"-0.01", "1"}, {".inf", "1"}, {".nan", "1"},
                 {"abc", "1"}, {"[1]", "1"}, {"0.01", "0.001"}, {"0.01", "1e300"}, {"0.01", "99999999999"}, {"0.01", "-.inf"}}) {
        EXPECT_THROW(timedConfig(dt, duration), ConfigError) << dt << " " << duration;
    }
}

TEST(MatchConfigTest, TheReplayHeaderReproducesTheTimes)
{
    for (int64_t ns : {1LL, 16666666LL, 33333333LL, 999999999LL, 1000000001LL, 123456789012345LL}) {
        MatchConfig config = timedConfig("0.01", "1");
        config.dt = AlicaTime::nanoseconds(ns);
        config.duration = AlicaTime::nanoseconds(ns * 3);
        JsonWriter w;
        w.beginObject();
        writeMatchConfigFields(w, config);
        w.endObject();
        const YAML::Node header = YAML::Load(w.str());
        EXPECT_EQ(parseSeconds(header["dt"], "dt"), config.dt) << w.str();
        EXPECT_EQ(parseSeconds(header["duration"], "duration"), config.duration) << w.str();
    }
}

} // namespace
} // namespace alica::sim
