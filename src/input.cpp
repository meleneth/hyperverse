#include "hyperverse/input.hpp"

#include <algorithm>

namespace {

[[nodiscard]] hyperverse::Vec2 apply_deadzone(hyperverse::Vec2 value, float deadzone) {
  const float magnitude = hyperverse::length(value);
  if (magnitude <= deadzone) {
    return {};
  }

  const float scaled = (magnitude - deadzone) / (1.0F - deadzone);
  return hyperverse::normalize_or_zero(value) * scaled;
}

}  // namespace

namespace hyperverse {

SemanticInputFrame map_flight_intent(const RawInputFrame& raw, const InputTuning& tuning) {
  return {
    .desired_movement = clamp_length(apply_deadzone(raw.movement_axis, tuning.deadzone), 1.0F),
    .primary_aim = clamp_length(apply_deadzone(raw.aim_axis, tuning.deadzone), 1.0F),
    .confirm_requested = raw.confirm,
    .cancel_requested = raw.cancel,
    .target_cycle_requested = raw.target_cycle,
    .particle_fire_requested = raw.particle_fire,
    .tool_intensity = std::clamp(raw.tool_intensity, 0.0F, 1.0F),
    .control_mapping = raw.control_mapping,
  };
}

SemanticInputFrame FlightInputMapper::map(const RawInputFrame& raw, const InputTuning& tuning) {
  SemanticInputFrame intent = map_flight_intent(raw, tuning);
  if (has_previous_) {
    intent.confirm_requested = raw.confirm && !previous_.confirm;
    intent.cancel_requested = raw.cancel && !previous_.cancel;
    intent.target_cycle_requested = raw.target_cycle && !previous_.target_cycle;
    intent.particle_fire_requested = raw.particle_fire && !previous_.particle_fire;
  }

  previous_ = raw;
  has_previous_ = true;
  return intent;
}

}  // namespace hyperverse
