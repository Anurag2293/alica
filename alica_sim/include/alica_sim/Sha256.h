#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <string_view>

namespace alica::sim
{

class Sha256
{
public:
    Sha256();
    void update(const void* data, size_t size);
    void update(std::string_view s) { update(s.data(), s.size()); }
    std::string hexDigest();

    static std::string hex(std::string_view s);
    static std::string fileHex(const std::string& path);

private:
    void block(const uint8_t* p);

    std::array<uint32_t, 8> _h;
    std::array<uint8_t, 64> _buf;
    size_t _bufLen = 0;
    uint64_t _total = 0;
};

} // namespace alica::sim
