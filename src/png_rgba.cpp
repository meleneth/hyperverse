#include "png_rgba.hpp"

#include <png.h>

#include <cstdio>
#include <stdexcept>

namespace hyperverse {

SpriteAlphaMask load_png_rgba(const std::filesystem::path& path) {
  FILE* file = std::fopen(path.string().c_str(), "rb");
  if (file == nullptr) {
    throw std::runtime_error("failed to open png: " + path.string());
  }

  png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
  png_infop info = png != nullptr ? png_create_info_struct(png) : nullptr;
  if (png == nullptr || info == nullptr) {
    png_destroy_read_struct(&png, nullptr, nullptr);
    std::fclose(file);
    throw std::runtime_error("failed to allocate png decoder");
  }

  if (setjmp(png_jmpbuf(png)) != 0) {
    png_destroy_read_struct(&png, &info, nullptr);
    std::fclose(file);
    throw std::runtime_error("failed to decode png: " + path.string());
  }

  png_init_io(png, file);
  png_read_info(png, info);

  const png_uint_32 width = png_get_image_width(png, info);
  const png_uint_32 height = png_get_image_height(png, info);
  const png_byte color_type = png_get_color_type(png, info);
  const png_byte bit_depth = png_get_bit_depth(png, info);

  if (bit_depth == 16) {
    png_set_strip_16(png);
  }
  if (color_type == PNG_COLOR_TYPE_PALETTE) {
    png_set_palette_to_rgb(png);
  }
  if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8) {
    png_set_expand_gray_1_2_4_to_8(png);
  }
  if (png_get_valid(png, info, PNG_INFO_tRNS) != 0) {
    png_set_tRNS_to_alpha(png);
  }
  if (color_type == PNG_COLOR_TYPE_RGB || color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_PALETTE) {
    png_set_filler(png, 0xFF, PNG_FILLER_AFTER);
  }
  if (color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_GRAY_ALPHA) {
    png_set_gray_to_rgb(png);
  }

  png_read_update_info(png, info);

  SpriteAlphaMask image{
    .width = static_cast<std::uint32_t>(width),
    .height = static_cast<std::uint32_t>(height),
    .rgba = std::vector<std::uint8_t>(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4U),
  };

  std::vector<png_bytep> rows(height);
  for (png_uint_32 row = 0; row < height; ++row) {
    rows[row] = image.rgba.data() + (static_cast<std::size_t>(row) * width * 4U);
  }
  png_read_image(png, rows.data());

  png_destroy_read_struct(&png, &info, nullptr);
  std::fclose(file);
  return image;
}

}  // namespace hyperverse
