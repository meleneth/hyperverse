#include "hyperverse/render_frame.hpp"

#include <algorithm>

namespace hyperverse {

RenderColor make_clear_color(const VulkanFrameSnapshot& frame) {
  const float speed = std::clamp(frame.speed_fraction, 0.0F, 1.0F);
  RenderColor color{
    .r = 0.02F + (speed * 0.03F),
    .g = 0.025F + (speed * 0.04F),
    .b = 0.04F + (speed * 0.10F),
    .a = 1.0F,
  };

  if (frame.target_locked) {
    color.r += 0.035F;
    color.g += 0.025F;
  }

  if (frame.wrap_warning) {
    color.r = 0.12F;
    color.g = 0.025F;
    color.b = 0.025F;
  }

  return color;
}

}  // namespace hyperverse
