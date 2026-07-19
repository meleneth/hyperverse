#pragma once

#include "hyperverse/domain_events.hpp"
#include "hyperverse/input.hpp"

namespace hyperverse {

enum class RadarControlPhase {
  Released,
  SingleHeld,
  ChordBlocked,
};

enum class RadarFocus {
  None,
  Mining,
  Combat,
};

struct RadarControlModel {
  RadarControlPhase phase{RadarControlPhase::Released};
  RadarFocus focus{RadarFocus::None};
};

struct RadarControlFrame {
  bool mining_target_cycle_requested{false};
  bool enemy_target_cycle_requested{false};
  bool clear_targets_requested{false};
};

[[nodiscard]] RadarControlFrame update_radar_control(
  RadarControlModel& control,
  const SemanticInputFrame& input,
  DomainEventBus* event_bus = nullptr
);

}  // namespace hyperverse
