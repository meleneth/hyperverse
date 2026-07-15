#include "hyperverse/input.hpp"

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
    .control_mapping = raw.control_mapping,
  };
}

}  // namespace hyperverse
