#pragma once
#include <cstdint>
#include <vector>
#include <string>

struct RawImageData
{
    int width;
    int height;
    unsigned char* data;
};

namespace barch
{
std::vector<std::uint8_t> encode(const RawImageData& img);
RawImageData decode(const std::uint8_t* bytes, std::size_t size);
void saveToFile(const std::string& path, const RawImageData& img);
RawImageData loadFromFile(const std::string& path);
void freeImage(RawImageData& img);
} // namespace barch
