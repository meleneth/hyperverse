#pragma once

#include "hyperverse/math.hpp"

namespace hyperverse {

enum class ControlMapping {
  Keyboard,
  Gamepad,
};

struct RawInputFrame {
  Vec2 movement_axis{};
  Vec2 aim_axis{};
  bool confirm{false};
  bool cancel{false};
  bool target_cycle{false};
  bool enemy_target_cycle{false};
  bool clear_targets{false};
  bool boost{false};
  bool gravity_sling{false};
  bool particle_fire{false};
  bool missile_fire{false};
  float tool_intensity{0.0F};
  ControlMapping control_mapping{ControlMapping::Keyboard};
};

struct SemanticInputFrame {
  Vec2 desired_movement{};
  Vec2 primary_aim{};
  bool confirm_requested{false};
  bool cancel_requested{false};
  bool target_cycle_active{false};
  bool target_cycle_requested{false};
  bool enemy_target_cycle_active{false};
  bool enemy_target_cycle_requested{false};
  bool clear_targets_active{false};
  bool clear_targets_requested{false};
  bool boost_requested{false};
  bool gravity_sling_requested{false};
  bool particle_fire_requested{false};
  bool particle_fire_active{false};
  bool missile_fire_requested{false};
  bool missile_fire_active{false};
  float tool_intensity{0.0F};
  ControlMapping control_mapping{ControlMapping::Keyboard};
};

struct InputTuning {
  float deadzone{0.18F};
};

[[nodiscard]] SemanticInputFrame map_flight_intent(const RawInputFrame& raw, const InputTuning& tuning = {});

class FlightInputMapper {
public:
  [[nodiscard]] SemanticInputFrame map(const RawInputFrame& raw, const InputTuning& tuning = {});
  [[nodiscard]] ControlMapping active_mapping() const;

private:
  RawInputFrame previous_{};
  bool has_previous_{false};
  ControlMapping active_mapping_{ControlMapping::Keyboard};
};

}  // namespace hyperverse
