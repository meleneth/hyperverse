#pragma once

#include "hyperverse/flight.hpp"
#include "hyperverse/input.hpp"
#include "hyperverse/math.hpp"
#include "hyperverse/sector.hpp"

#include <array>
#include <cstddef>
#include <vector>

namespace hyperverse {

struct EngineTrailTuning {
  float sample_interval_seconds{1.0F / 60.0F};
  float sample_lifetime_seconds{0.48F};
  float movement_sample_distance{8.0F};
  float direction_sample_dot{0.965F};
  float thrust_sample_delta{0.12F};
  float teleport_distance{900.0F};
  float base_width{14.0F};
  float max_width{30.0F};
  float source_width{22.0F};
  float source_length{30.0F};
  float brightness_multiplier{1.35F};
  float decay_rate{3.8F};
  float exposure_steps{7.0F};
};

struct EngineTrailSample {
  Vec2 world_position{};
  Vec2 exhaust_direction{};
  float intensity{0.0F};
  float age_seconds{0.0F};
};

struct EngineTrailVertex {
  Vec2 position{};
  float normalized_age{0.0F};
  float intensity{0.0F};
  float side{0.0F};
};

struct EngineSourceDraw {
  Vec2 position{};
  Vec2 exhaust_direction{};
  float intensity{0.0F};
};

struct EngineTrailEngine {
  static constexpr std::size_t Capacity = 32U;
  std::array<EngineTrailSample, Capacity> samples{};
  std::size_t start{0U};
  std::size_t count{0U};
  float sample_accumulator_seconds{0.0F};
  float previous_intensity{0.0F};
  Vec2 previous_position{};
  Vec2 previous_direction{.x = -1.0F, .y = 0.0F};
  bool has_previous{false};
};

struct EngineTrailModel {
  std::array<EngineTrailEngine, 2> engines{};
};

struct EngineTrailUpdate {
  std::array<EngineSourceDraw, 2> sources{};
  std::size_t active_sources{0U};
};

void reset_engine_trail(EngineTrailModel& model);

[[nodiscard]] EngineTrailUpdate update_engine_trail(
  EngineTrailModel& model,
  const ShipMotion& ship,
  const SemanticInputFrame& input,
  const SectorTuning& sector,
  float dt_seconds,
  const EngineTrailTuning& tuning = {}
);

[[nodiscard]] std::vector<EngineTrailVertex> build_engine_trail_ribbon(
  const EngineTrailEngine& engine,
  const SectorTuning& sector,
  const EngineTrailTuning& tuning = {}
);

}  // namespace hyperverse
