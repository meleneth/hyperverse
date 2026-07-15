#pragma once

#include "hyperverse/flight.hpp"
#include "hyperverse/math.hpp"
#include "hyperverse/sector.hpp"

namespace hyperverse {

struct CameraState {
  Vec2 position{};
  float rotation_radians{0.0F};
};

struct CameraTuning {
  float position_lag_seconds{0.18F};
  float rotation_lag_seconds{0.12F};
  float velocity_lookahead_seconds{0.35F};
  float screen_anchor_y_fraction{0.75F};
};

void update_camera_anchor(
  CameraState& camera,
  const ShipMotion& ship,
  const SectorTuning& sector,
  const CameraTuning& tuning,
  float dt_seconds
);

}  // namespace hyperverse
