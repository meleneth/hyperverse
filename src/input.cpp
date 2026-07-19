#include "hyperverse/input.hpp"

#include <boost/sml.hpp>

#include <algorithm>

namespace {

namespace sml = boost::sml;

struct keyboard_mapping_observed {};
struct gamepad_mapping_observed {};
struct keyboard_mapping_active {};
struct gamepad_mapping_active {};

struct ControlMappingMachine {
  auto operator()() const {
    using namespace sml;
    return make_transition_table(
      *state<keyboard_mapping_active> + event<gamepad_mapping_observed> = state<gamepad_mapping_active>,
      state<gamepad_mapping_active> + event<keyboard_mapping_observed> = state<keyboard_mapping_active>
    );
  }
};

[[nodiscard]] hyperverse::Vec2 apply_deadzone(hyperverse::Vec2 value, float deadzone) {
  const float magnitude = hyperverse::length(value);
  if (magnitude <= deadzone) {
    return {};
  }

  const float scaled = (magnitude - deadzone) / (1.0F - deadzone);
  return hyperverse::normalize_or_zero(value) * scaled;
}

[[nodiscard]] hyperverse::ControlMapping resolve_active_mapping(hyperverse::ControlMapping previous, hyperverse::ControlMapping observed) {
  sml::sm<ControlMappingMachine> machine;
  if (previous == hyperverse::ControlMapping::Gamepad) {
    machine.process_event(gamepad_mapping_observed{});
  }

  if (observed == hyperverse::ControlMapping::Gamepad) {
    machine.process_event(gamepad_mapping_observed{});
  } else {
    machine.process_event(keyboard_mapping_observed{});
  }

  return machine.is(sml::state<gamepad_mapping_active>) ? hyperverse::ControlMapping::Gamepad : hyperverse::ControlMapping::Keyboard;
}

}  // namespace

namespace hyperverse {

SemanticInputFrame map_flight_intent(const RawInputFrame& raw, const InputTuning& tuning) {
  return {
    .desired_movement = clamp_length(apply_deadzone(raw.movement_axis, tuning.deadzone), 1.0F),
    .primary_aim = clamp_length(apply_deadzone(raw.aim_axis, tuning.deadzone), 1.0F),
    .confirm_requested = raw.confirm,
    .cancel_requested = raw.cancel,
    .target_cycle_active = raw.target_cycle,
    .target_cycle_requested = raw.target_cycle,
    .enemy_target_cycle_active = raw.enemy_target_cycle,
    .enemy_target_cycle_requested = raw.enemy_target_cycle,
    .clear_targets_active = raw.clear_targets || (raw.target_cycle && raw.enemy_target_cycle),
    .clear_targets_requested = raw.clear_targets || (raw.target_cycle && raw.enemy_target_cycle),
    .boost_requested = raw.boost,
    .gravity_sling_requested = raw.gravity_sling,
    .particle_fire_requested = raw.particle_fire,
    .particle_fire_active = raw.particle_fire,
    .missile_fire_requested = raw.missile_fire,
    .missile_fire_active = raw.missile_fire,
    .tool_intensity = std::clamp(raw.tool_intensity, 0.0F, 1.0F),
    .control_mapping = raw.control_mapping,
  };
}

SemanticInputFrame FlightInputMapper::map(const RawInputFrame& raw, const InputTuning& tuning) {
  active_mapping_ = resolve_active_mapping(active_mapping_, raw.control_mapping);
  SemanticInputFrame intent = map_flight_intent(raw, tuning);
  intent.control_mapping = active_mapping_;
  if (has_previous_) {
    intent.confirm_requested = raw.confirm && !previous_.confirm;
    intent.cancel_requested = raw.cancel && !previous_.cancel;
    intent.target_cycle_requested = raw.target_cycle && !previous_.target_cycle;
    intent.enemy_target_cycle_requested = raw.enemy_target_cycle && !previous_.enemy_target_cycle;
    intent.clear_targets_requested = intent.clear_targets_active && !(previous_.clear_targets || (previous_.target_cycle && previous_.enemy_target_cycle));
    intent.boost_requested = raw.boost && !previous_.boost;
    intent.gravity_sling_requested = raw.gravity_sling && !previous_.gravity_sling;
    intent.particle_fire_requested = raw.particle_fire && !previous_.particle_fire;
    intent.missile_fire_requested = raw.missile_fire && !previous_.missile_fire;
  }

  previous_ = raw;
  has_previous_ = true;
  return intent;
}

ControlMapping FlightInputMapper::active_mapping() const {
  return active_mapping_;
}

}  // namespace hyperverse
