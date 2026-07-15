#pragma once

namespace hyperverse {

struct SectorPressureModel {
  float elapsed_seconds{0.0F};
  int escalation_level{0};
  float announcement_seconds_remaining{0.0F};
};

struct SectorPressureTuning {
  float escalation_interval_seconds{300.0F};
  float announcement_duration_seconds{8.0F};
  float pressure_per_level{0.18F};
};

struct SectorPressureHudSnapshot {
  int escalation_level{0};
  float elapsed_seconds{0.0F};
  float next_escalation_seconds{0.0F};
  float pressure_fraction{0.0F};
  bool escalation_announced{false};
};

[[nodiscard]] SectorPressureHudSnapshot update_sector_pressure(
  SectorPressureModel& pressure,
  float dt_seconds,
  const SectorPressureTuning& tuning = {}
);

}  // namespace hyperverse
