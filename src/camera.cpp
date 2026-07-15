#include "hyperverse/camera.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>

namespace {

[[nodiscard]] float smoothing_alpha(float dt_seconds, float lag_seconds) {
  const float lag = std::max(lag_seconds, std::numeric_limits<float>::epsilon());
  return 1.0F - std::exp(-dt_seconds / lag);
}

[[nodiscard]] float shortest_angle_delta(float from, float to) {
  float delta = std::fmod(to - from, std::numbers::pi_v<float> * 2.0F);
  if (delta > std::numbers::pi_v<float>) {
    delta -= std::numbers::pi_v<float> * 2.0F;
  } else if (delta < -std::numbers::pi_v<float>) {
    delta += std::numbers::pi_v<float> * 2.0F;
  }
  return delta;
}

}  // namespace

namespace hyperverse {

void update_camera_anchor(
  CameraState& camera,
  const ShipMotion& ship,
  const SectorTuning& sector,
  const CameraTuning& tuning,
  float dt_seconds
) {
  const Vec2 target = wrap_position(ship.position + (ship.velocity * tuning.velocity_lookahead_seconds), sector);
  const float position_alpha = smoothing_alpha(dt_seconds, tuning.position_lag_seconds);
  camera.position = wrap_position(camera.position + (wrapped_delta(camera.position, target, sector) * position_alpha), sector);

  const float rotation_alpha = smoothing_alpha(dt_seconds, tuning.rotation_lag_seconds);
  camera.rotation_radians += shortest_angle_delta(camera.rotation_radians, ship.facing_radians) * rotation_alpha;
}

}  // namespace hyperverse
