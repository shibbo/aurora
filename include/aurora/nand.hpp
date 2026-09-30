#pragma once

#include <filesystem>
#include <string_view>
#include <cstdint>

namespace aurora::nand {
int32_t configure(const std::filesystem::path& directory, std::string_view home);
}
