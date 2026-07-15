#pragma once

#include <vector>

namespace hyperverse {

struct RenderColor {
  float r{0.02F};
  float g{0.025F};
  float b{0.04F};
  float a{1.0F};
};

struct VulkanFrameSnapshot {
  float speed_fraction{0.0F};
  bool wrap_warning{false};
  bool target_locked{false};
  bool mining_active{false};
};

[[nodiscard]] RenderColor make_clear_color(const VulkanFrameSnapshot& frame);

enum class SpriteTexture {
  Ship,
  Rock,
  Reticle,
  Laser,
  Drone,
};

struct SpriteDraw {
  SpriteTexture texture{SpriteTexture::Ship};
  float center_x_ndc{0.0F};
  float center_y_ndc{0.0F};
  float half_width_ndc{0.05F};
  float half_height_ndc{0.05F};
  float rotation_radians{0.0F};
  float tint_r{1.0F};
  float tint_g{1.0F};
  float tint_b{1.0F};
  float tint_a{1.0F};
};

struct LineDraw {
  float start_x_ndc{0.0F};
  float start_y_ndc{0.0F};
  float end_x_ndc{0.0F};
  float end_y_ndc{0.0F};
  float r{1.0F};
  float g{1.0F};
  float b{1.0F};
  float a{1.0F};
};

struct SpriteFrame {
  VulkanFrameSnapshot state{};
  std::vector<SpriteDraw> sprites{};
  std::vector<LineDraw> lines{};
};

}  // namespace hyperverse
