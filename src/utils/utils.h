#pragma once
#include <vector>
#include <string>

namespace rt {

std::vector<char> readFile(const std::string& filename);
uint32_t align(uint32_t value, uint32_t alignment);
}
