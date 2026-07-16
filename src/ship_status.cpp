#include "hyperverse/ship_status.hpp"

#include <algorithm>

namespace hyperverse {

void update_ship_status(ShipHealth& health, RoundTimer& round_timer, float dt_seconds) {
  const float scaled_dt = std::max(0.0F, dt_seconds);
  health.shields = std::min(health.max_shields, health.shields + (health.shield_regen_per_second * scaled_dt));
  health.armor = std::clamp(health.armor, 0.0F, health.max_armor);
  round_timer.elapsed_seconds = std::min(round_timer.duration_seconds, round_timer.elapsed_seconds + scaled_dt);
}

}  // namespace hyperverse
