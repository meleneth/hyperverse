#include "hyperverse/flight.hpp"

#include "hyperverse/grand_central.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>

namespace {

[[nodiscard]] float shortest_angle_delta(float from, float to) {
  float delta = std::fmod(to - from, std::numbers::pi_v<float> * 2.0F);
  if (delta > std::numbers::pi_v<float>) {
    delta -= std::numbers::pi_v<float> * 2.0F;
  } else if (delta < -std::numbers::pi_v<float>) {
    delta += std::numbers::pi_v<float> * 2.0F;
  }
  return delta;
}

[[nodiscard]] float angle_of(hyperverse::Vec2 value) {
  return std::atan2(value.y, value.x);
}

[[nodiscard]] float nearest_wrap_edge_distance(hyperverse::Vec2 position, const hyperverse::SectorTuning& sector) {
  return std::min({position.x, sector.width - position.x, position.y, sector.height - position.y});
}

}  // namespace

namespace hyperverse {

void simulate_assisted_flight(
  AccountCtx& ctx,
  ShipMotion& ship,
  const SemanticInputFrame& input,
  const FlightTuning& flight,
  const SectorTuning& sector,
  float dt_seconds
) {
  const Vec2 desired_velocity = input.desired_movement * flight.max_speed;
  const Vec2 velocity_error = desired_velocity - ship.velocity;
  const float response = length(input.desired_movement) > 0.0F ? flight.acceleration : flight.braking;
  ship.velocity += clamp_length(velocity_error, response * dt_seconds);
  ship.velocity = clamp_length(ship.velocity, flight.max_speed);
  ctx.physics().integrate_ship(ship, sector, dt_seconds);

  const Vec2 facing_intent = length(input.desired_movement) > 0.0F ? input.desired_movement : input.primary_aim;
  if (length(facing_intent) > 0.0F) {
    const float target_angle = angle_of(facing_intent);
    const float max_turn = flight.turn_rate * dt_seconds;
    const float delta = std::clamp(shortest_angle_delta(ship.facing_radians, target_angle), -max_turn, max_turn);
    ship.facing_radians += delta;
  }
}

FlightHudSnapshot make_flight_hud_snapshot(
  const ShipMotion& ship,
  const SemanticInputFrame& input,
  const FlightTuning& flight,
  const SectorTuning& sector,
  const FlightHudTuning& hud
) {
  const float speed = length(ship.velocity);
  const float max_speed = std::max(flight.max_speed, std::numeric_limits<float>::epsilon());
  const float nearest_edge = nearest_wrap_edge_distance(ship.position, sector);

  return {
    .position = ship.position,
    .velocity = ship.velocity,
    .speed = speed,
    .speed_fraction = std::clamp(speed / max_speed, 0.0F, 1.0F),
    .facing_radians = ship.facing_radians,
    .desired_movement = input.desired_movement,
    .nearest_wrap_edge_distance = nearest_edge,
    .wrap_warning = nearest_edge <= hud.wrap_warning_distance,
    .control_mapping = input.control_mapping,
  };
}

}  // namespace hyperverse
