#pragma once

namespace hyperverse {

struct ShipHealth {
  float armor{100.0F};
  float max_armor{100.0F};
  float shields{100.0F};
  float max_shields{100.0F};
  float shield_regen_per_second{8.0F};
};

struct ShipComputer {
  float hud_effectiveness{1.0F};
  float scan_resolution{1.0F};
  float prediction_quality{1.0F};
};

struct RoundTimer {
  float elapsed_seconds{0.0F};
  float duration_seconds{20.0F * 60.0F};
};

void update_ship_status(ShipHealth& health, RoundTimer& round_timer, float dt_seconds);

}  // namespace hyperverse
