#include "hyperverse/pressure.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace hyperverse {

SectorPressureHudSnapshot update_sector_pressure(
  SectorPressureModel& pressure,
  float dt_seconds,
  const SectorPressureTuning& tuning
) {
  const float interval = std::max(tuning.escalation_interval_seconds, std::numeric_limits<float>::epsilon());
  pressure.elapsed_seconds += std::max(0.0F, dt_seconds);

  const int previous_level = pressure.escalation_level;
  pressure.escalation_level = static_cast<int>(std::floor(pressure.elapsed_seconds / interval));
  if (pressure.escalation_level > previous_level) {
    pressure.announcement_seconds_remaining = tuning.announcement_duration_seconds;
  } else {
    pressure.announcement_seconds_remaining =
      std::max(0.0F, pressure.announcement_seconds_remaining - std::max(0.0F, dt_seconds));
  }

  const float next_boundary = static_cast<float>(pressure.escalation_level + 1) * interval;
  const float level_elapsed = pressure.elapsed_seconds - (static_cast<float>(pressure.escalation_level) * interval);
  return {
    .escalation_level = pressure.escalation_level,
    .elapsed_seconds = pressure.elapsed_seconds,
    .next_escalation_seconds = std::max(0.0F, next_boundary - pressure.elapsed_seconds),
    .pressure_fraction = std::clamp(static_cast<float>(pressure.escalation_level) * tuning.pressure_per_level, 0.0F, 1.0F),
    .escalation_progress_fraction = std::clamp(level_elapsed / interval, 0.0F, 1.0F),
    .escalation_announced = pressure.announcement_seconds_remaining > 0.0F,
  };
}

}  // namespace hyperverse
