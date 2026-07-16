#include "vulkan_detail.hpp"

#include "png_rgba.hpp"

#include <cstring>
#include <fstream>

namespace hyperverse::vulkan_detail {

[[nodiscard]] std::vector<char> read_file(const std::filesystem::path& path) {
  std::ifstream file{path, std::ios::ate | std::ios::binary};
  if (!file) {
    throw std::runtime_error("failed to open file: " + path.string());
  }

  const std::streamsize size = file.tellg();
  std::vector<char> buffer(static_cast<std::size_t>(size));
  file.seekg(0);
  file.read(buffer.data(), size);
  return buffer;
}

[[nodiscard]] LoadedPng crop_rgba(const LoadedPng& source, std::uint32_t x, std::uint32_t y, std::uint32_t width, std::uint32_t height) {
  LoadedPng cropped{
    .width = width,
    .height = height,
    .rgba = std::vector<std::uint8_t>(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4U),
  };

  for (std::uint32_t row = 0; row < height; ++row) {
    const std::size_t source_offset = ((static_cast<std::size_t>(y + row) * source.width) + x) * 4U;
    const std::size_t destination_offset = static_cast<std::size_t>(row) * width * 4U;
    std::memcpy(cropped.rgba.data() + destination_offset, source.rgba.data() + source_offset, static_cast<std::size_t>(width) * 4U);
  }

  return cropped;
}

[[nodiscard]] std::vector<LoadedPng> load_sprite_textures() {
  std::vector<LoadedPng> images;
  images.push_back(load_png_rgba("assets/sector7/sprites/ship.png"));
  images.push_back(load_png_rgba("assets/sector7/sprites/rock1.png"));
  images.push_back(load_png_rgba("assets/sector7/sprites/reticle.png"));
  images.push_back(load_png_rgba("assets/sector7/sprites/laser.png"));
  images.push_back(load_png_rgba("assets/sector7/sprites/robot.png"));
  images.push_back(load_png_rgba("assets/sector7/sprites/rocket.png"));
  images.push_back(load_png_rgba("assets/sector7/sprites/particle.png"));

  const LoadedPng alpha = load_png_rgba("assets/sector7/sprites/alpha.png");
  for (std::uint32_t index = 0; index < 26U; ++index) {
    images.push_back(crop_rgba(alpha, index * 8U, 0U, 8U, 16U));
  }

  const LoadedPng digits = load_png_rgba("assets/sector7/sprites/digits.png");
  for (std::uint32_t index = 0; index < 10U; ++index) {
    images.push_back(crop_rgba(digits, index * 8U, 0U, 8U, 16U));
  }

  return images;
}

[[nodiscard]] VkShaderModule create_shader_module(VkDevice device, const std::vector<char>& code) {
  VkShaderModuleCreateInfo create_info{};
  create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  create_info.codeSize = code.size();
  create_info.pCode = reinterpret_cast<const std::uint32_t*>(code.data());

  VkShaderModule shader = VK_NULL_HANDLE;
  check(vkCreateShaderModule(device, &create_info, nullptr, &shader), "create shader module");
  return shader;
}

}  // namespace hyperverse::vulkan_detail
