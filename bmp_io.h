#pragma once
#include <string>
#include "barch.hpp"

RawImageData loadGrayBMP(const std::string& path);
void writeGrayBMP(const std::string& path, const RawImageData& img);
