#include "alica_sim/JsonWriter.h"
#include "alica_sim/Numeric.h"

#include <yaml-cpp/yaml.h>

#include <cstdio>
#include <stdexcept>

namespace alica::sim
{

JsonWriter& JsonWriter::beginObject()
{
    separate();
    _out += '{';
    _first.push_back(true);
    return *this;
}

JsonWriter& JsonWriter::endObject()
{
    _out += '}';
    _first.pop_back();
    return *this;
}

JsonWriter& JsonWriter::beginArray()
{
    separate();
    _out += '[';
    _first.push_back(true);
    return *this;
}

JsonWriter& JsonWriter::endArray()
{
    _out += ']';
    _first.pop_back();
    return *this;
}

JsonWriter& JsonWriter::key(std::string_view k)
{
    separate();
    writeString(k);
    _out += ':';
    _afterKey = true;
    return *this;
}

JsonWriter& JsonWriter::value(double v)
{
    separate();
    _out += formatDouble(v);
    return *this;
}

JsonWriter& JsonWriter::value(int64_t v)
{
    separate();
    _out += std::to_string(v);
    return *this;
}

JsonWriter& JsonWriter::value(uint64_t v)
{
    separate();
    _out += std::to_string(v);
    return *this;
}

JsonWriter& JsonWriter::value(bool v)
{
    separate();
    _out += v ? "true" : "false";
    return *this;
}

JsonWriter& JsonWriter::value(std::string_view v)
{
    separate();
    writeString(v);
    return *this;
}

JsonWriter& JsonWriter::number(std::string_view literal)
{
    separate();
    _out += literal;
    return *this;
}

JsonWriter& JsonWriter::null()
{
    separate();
    _out += "null";
    return *this;
}

JsonWriter& JsonWriter::value(const YAML::Node& node)
{
    switch (node.Type()) {
    case YAML::NodeType::Map:
        beginObject();
        for (const auto& kv : node) {
            key(kv.first.as<std::string>());
            value(kv.second);
        }
        return endObject();
    case YAML::NodeType::Sequence:
        beginArray();
        for (const auto& item : node) {
            value(item);
        }
        return endArray();
    case YAML::NodeType::Scalar: {
        const std::string& s = node.Scalar();
        if (node.Tag() != "!") {
            if (s == "true" || s == "false") {
                return value(s == "true");
            }
            if (s == "null" || s == "~") {
                return null();
            }
            int64_t i = 0;
            if (YAML::convert<int64_t>::decode(node, i)) {
                return value(i);
            }
            double d = 0;
            if (decodeFiniteDouble(node, d)) {
                return value(d);
            }
        }
        return value(s);
    }
    default:
        return null();
    }
}

void JsonWriter::clear()
{
    _out.clear();
    _first.clear();
    _afterKey = false;
}

std::string JsonWriter::formatDouble(double v)
{
    if (!isFinite(v)) {
        return "null";
    }
    char buf[512];
    std::snprintf(buf, sizeof(buf), "%.6f", v);
    std::string s(buf);
    size_t dot = s.find('.');
    if (dot != std::string::npos) {
        size_t end = s.find_last_not_of('0');
        s.erase(end == dot ? dot : end + 1);
    }
    if (s == "-0") {
        s = "0";
    }
    return s;
}

void JsonWriter::separate()
{
    if (_afterKey) {
        _afterKey = false;
        return;
    }
    if (!_first.empty()) {
        if (!_first.back()) {
            _out += ',';
        }
        _first.back() = false;
    }
}

void JsonWriter::writeString(std::string_view s)
{
    _out += '"';
    for (char c : s) {
        switch (c) {
        case '"':
            _out += "\\\"";
            break;
        case '\\':
            _out += "\\\\";
            break;
        case '\n':
            _out += "\\n";
            break;
        case '\r':
            _out += "\\r";
            break;
        case '\t':
            _out += "\\t";
            break;
        default:
            if (static_cast<unsigned char>(c) < 0x20) {
                char buf[8];
                std::snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned char>(c));
                _out += buf;
            } else {
                _out += c;
            }
        }
    }
    _out += '"';
}

} // namespace alica::sim
