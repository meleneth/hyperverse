#pragma once

#include <string>

namespace hyperverse {

struct HudNotice {
  std::string message{};
  float seconds_remaining{0.0F};
};

void push_hud_notice(HudNotice& notice, std::string message, float duration_seconds = 4.0F);
void update_hud_notice(HudNotice& notice, float dt_seconds);

}  // namespace hyperverse
