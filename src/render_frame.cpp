#include "hyperverse/render_frame.hpp"

#include <algorithm>

namespace hyperverse {

RenderColor make_clear_color(const FrameSnapshot& frame) {
  (void)frame;
  return {};
}

}  // namespace hyperverse
