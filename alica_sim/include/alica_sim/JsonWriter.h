#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace YAML
{
class Node;
}

namespace alica::sim
{

// Doubles are written with at most 6 decimals and no trailing zeros, so output is byte-stable.
class JsonWriter
{
public:
    JsonWriter& beginObject();
    JsonWriter& endObject();
    JsonWriter& beginArray();
    JsonWriter& endArray();
    JsonWriter& key(std::string_view k);

    JsonWriter& value(double v);
    JsonWriter& value(int v) { return value(static_cast<int64_t>(v)); }
    JsonWriter& value(int64_t v);
    JsonWriter& value(uint64_t v);
    JsonWriter& value(bool v);
    JsonWriter& value(std::string_view v);
    JsonWriter& value(const char* v) { return value(std::string_view(v)); }
    JsonWriter& value(const std::string& v) { return value(std::string_view(v)); }
    JsonWriter& value(const YAML::Node& node);
    JsonWriter& number(std::string_view literal);
    JsonWriter& null();

    template <class T>
    JsonWriter& field(std::string_view k, const T& v)
    {
        key(k);
        return value(v);
    }

    const std::string& str() const { return _out; }
    void clear();

    static std::string formatDouble(double v);

private:
    void separate();
    void writeString(std::string_view s);

    std::string _out;
    std::vector<bool> _first;
    bool _afterKey = false;
};

} // namespace alica::sim
