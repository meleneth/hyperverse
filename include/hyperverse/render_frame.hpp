#pragma once

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

}  // namespace hyperverse
