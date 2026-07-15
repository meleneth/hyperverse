#pragma once

#include "hyperverse/input.hpp"
#include "hyperverse/math.hpp"
#include "hyperverse/sector.hpp"

namespace hyperverse {

struct ShipMotion {
  Vec2 position{};
  Vec2 velocity{};
  float facing_radians{0.0F};
};

struct FlightTuning {
  float max_speed{760.0F};
  float acceleration{1800.0F};
  float braking{2400.0F};
  float turn_rate{9.0F};
};

struct FlightHudSnapshot {
  Vec2 position{};
  Vec2 velocity{};
  float speed{0.0F};
  float facing_radians{0.0F};
  Vec2 desired_movement{};
};

void simulate_assisted_flight(
  ShipMotion& ship,
  const SemanticInputFrame& input,
  const FlightTuning& flight,
  const SectorTuning& sector,
  float dt_seconds
);

[[nodiscard]] FlightHudSnapshot make_flight_hud_snapshot(const ShipMotion& ship, const SemanticInputFrame& input);

}  // namespace hyperverse
