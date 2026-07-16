#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <vector>

namespace hyperverse::vulkan_detail {

struct SpritePushConstants {
  float center_x{0.0F};
  float center_y{0.0F};
  float half_width{0.0F};
  float half_height{0.0F};
  float rotation_radians{0.0F};
  float padding0{0.0F};
  float padding1{0.0F};
  float padding2{0.0F};
  float tint_r{1.0F};
  float tint_g{1.0F};
  float tint_b{1.0F};
  float tint_a{1.0F};
};

struct LinePushConstants {
  float start_x{0.0F};
  float start_y{0.0F};
  float end_x{0.0F};
  float end_y{0.0F};
  float r{1.0F};
  float g{1.0F};
  float b{1.0F};
  float a{1.0F};
};

inline void check(VkResult result, const char* message) {
  if (result != VK_SUCCESS) {
    throw std::runtime_error(message);
  }
}

struct LoadedPng {
  std::uint32_t width{0};
  std::uint32_t height{0};
  std::vector<std::uint8_t> rgba{};
};

[[nodiscard]] std::vector<char> read_file(const std::filesystem::path& path);
[[nodiscard]] std::vector<LoadedPng> load_sprite_textures();
[[nodiscard]] VkShaderModule create_shader_module(VkDevice device, const std::vector<char>& code);

}  // namespace hyperverse::vulkan_detail
