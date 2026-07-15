#pragma once

#include "hyperverse/math.hpp"

namespace hyperverse {

struct RawInputFrame {
  Vec2 movement_axis{};
  Vec2 aim_axis{};
  bool confirm{false};
  bool cancel{false};
};

struct SemanticInputFrame {
  Vec2 desired_movement{};
  Vec2 primary_aim{};
  bool confirm_requested{false};
  bool cancel_requested{false};
};

struct InputTuning {
  float deadzone{0.18F};
};

[[nodiscard]] SemanticInputFrame map_flight_intent(const RawInputFrame& raw, const InputTuning& tuning = {});

}  // namespace hyperverse
