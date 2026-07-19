#include "hyperverse/flight.hpp"

#include "hyperverse/account_context.hpp"

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

[[nodiscard]] float boost_extra_speed(const hyperverse::FlightTuning& flight, float seconds_remaining) {
  const float duration = std::max(flight.boost_duration_seconds, std::numeric_limits<float>::epsilon());
  const float remaining_fraction = std::clamp(seconds_remaining / duration, 0.0F, 1.0F);
  const float slow_then_fast = 1.0F - std::pow(1.0F - remaining_fraction, 3.0F);
  return flight.max_speed * std::max(0.0F, flight.boost_speed_multiplier - 1.0F) * slow_then_fast;
}

}  // namespace

namespace hyperverse {

ThrusterCommand flight_computer_assist(
  const ShipMotion& ship,
  const SemanticInputFrame& input,
  const FlightTuning& flight
) {
  ThrusterCommand command{};
  const Vec2 movement = clamp_length(input.desired_movement, 1.0F);
  if (length(movement) > 0.0001F) {
    command.linear_thrust = movement;
    command.desired_facing_radians = angle_of(movement);
    command.has_desired_facing = true;
  } else if (flight.auto_brake && length(ship.velocity) > 0.0001F) {
    command.linear_thrust = normalize_or_zero(ship.velocity) * -1.0F;
    command.desired_facing_radians = angle_of(command.linear_thrust);
    command.has_desired_facing = true;
  } else if (length(input.primary_aim) > 0.0001F) {
    command.desired_facing_radians = angle_of(input.primary_aim);
    command.has_desired_facing = true;
  }

  command.boost = input.boost_requested;
  return command;
}

void apply_thruster_physics(
  ShipMotion& ship,
  const ThrusterCommand& command,
  const FlightTuning& flight,
  const SectorTuning& sector,
  float dt_seconds
) {
  const float dt = std::max(0.0F, dt_seconds);
  if (command.boost) {
    ship.boost_seconds_remaining = std::max(ship.boost_seconds_remaining, flight.boost_duration_seconds);
  }
  ship.boost_seconds_remaining = std::max(0.0F, ship.boost_seconds_remaining - std::max(0.0F, dt_seconds));
  ship.boost_speed = boost_extra_speed(flight, ship.boost_seconds_remaining);

  Vec2 thrust = clamp_length(command.linear_thrust, 1.0F);
  if (length(thrust) > 0.0001F) {
    const bool braking = dot(thrust, ship.velocity) < 0.0F;
    float base_acceleration = braking ? flight.braking : flight.acceleration;
    if (braking && dt > 0.0001F) {
      base_acceleration = std::min(base_acceleration, length(ship.velocity) / dt);
    }
    const float boost_multiplier = ship.boost_seconds_remaining > 0.0F ? std::max(1.0F, flight.boost_speed_multiplier) : 1.0F;
    ship.velocity += thrust * base_acceleration * boost_multiplier * dt;
  }

  if (command.enforce_speed_envelope) {
    ship.velocity = clamp_length(ship.velocity, flight.max_speed + ship.boost_speed);
  }
  ship.position = wrap_position(ship.position + (ship.velocity * dt), sector);

  if (command.has_desired_facing) {
    const float target_angle = command.desired_facing_radians;
    const float max_turn = flight.turn_rate * dt_seconds;
    const float delta = std::clamp(shortest_angle_delta(ship.facing_radians, target_angle), -max_turn, max_turn);
    ship.facing_radians += delta;
  }
}

void simulate_assisted_flight(
  AccountCtx& ctx,
  ShipMotion& ship,
  const SemanticInputFrame& input,
  const FlightTuning& flight,
  const SectorTuning& sector,
  float dt_seconds
) {
  (void)ctx;
  apply_thruster_physics(ship, flight_computer_assist(ship, input, flight), flight, sector, dt_seconds);
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
  const ThrusterCommand command = flight_computer_assist(ship, input, flight);

  return {
    .position = ship.position,
    .velocity = ship.velocity,
    .thrust_vector = command.linear_thrust,
    .speed = speed,
    .speed_fraction = std::clamp(speed / max_speed, 0.0F, 1.0F),
    .facing_radians = ship.facing_radians,
    .desired_movement = input.desired_movement,
    .nearest_wrap_edge_distance = nearest_edge,
    .wrap_warning = nearest_edge <= hud.wrap_warning_distance,
    .control_mapping = input.control_mapping,
    .braking_assist = length(input.desired_movement) <= 0.0001F && length(command.linear_thrust) > 0.0001F,
  };
}

}  // namespace hyperverse
