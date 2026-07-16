#include "hyperverse/hud_notice.hpp"

#include <algorithm>
#include <utility>

namespace hyperverse {

void push_hud_notice(HudNotice& notice, std::string message, float duration_seconds) {
  notice.message = std::move(message);
  notice.seconds_remaining = std::max(0.0F, duration_seconds);
}

void update_hud_notice(HudNotice& notice, float dt_seconds) {
  notice.seconds_remaining = std::max(0.0F, notice.seconds_remaining - std::max(0.0F, dt_seconds));
  if (notice.seconds_remaining <= 0.0F) {
    notice.message.clear();
  }
}

}  // namespace hyperverse
