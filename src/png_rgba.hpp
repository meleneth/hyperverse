#pragma once

#include "hyperverse/sprite_collision_shape.hpp"

#include <filesystem>

namespace hyperverse {

[[nodiscard]] SpriteAlphaMask load_png_rgba(const std::filesystem::path& path);

}  // namespace hyperverse
