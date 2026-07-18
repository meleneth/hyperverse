#include "hyperverse/engine_trail.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace hyperverse {
namespace {

constexpr float TauRadians = 6.28318530718F;

[[nodiscard]] Vec2 direction_from_angle(float radians) {
  return {.x = std::cos(radians), .y = std::sin(radians)};
}

[[nodiscard]] Vec2 rotate(Vec2 value, float radians) {
  const float cosine = std::cos(radians);
  const float sine = std::sin(radians);
  return {.x = (value.x * cosine) - (value.y * sine), .y = (value.x * sine) + (value.y * cosine)};
}

[[nodiscard]] Vec2 nozzle_offset(std::size_t index, float facing_radians) {
  const float side = index == 0U ? -1.0F : 1.0F;
  return rotate({.x = -25.0F, .y = side * 10.0F}, facing_radians);
}

[[nodiscard]] float thrust_intensity(const ShipMotion& ship, const SemanticInputFrame& input) {
  const float movement = std::clamp(length(input.desired_movement), 0.0F, 1.0F);
  const float boost = ship.boost_seconds_remaining > 0.0F ? 0.35F : 0.0F;
  return std::clamp(movement + boost, 0.0F, 1.0F);
}

[[nodiscard]] std::size_t sample_index(const EngineTrailEngine& engine, std::size_t logical_index) {
  return (engine.start + logical_index) % EngineTrailEngine::Capacity;
}

[[nodiscard]] const EngineTrailSample& sample_at(const EngineTrailEngine& engine, std::size_t logical_index) {
  return engine.samples[sample_index(engine, logical_index)];
}

void push_sample(EngineTrailEngine& engine, EngineTrailSample sample) {
  if (engine.count < EngineTrailEngine::Capacity) {
    engine.samples[sample_index(engine, engine.count)] = sample;
    ++engine.count;
  } else {
    engine.samples[engine.start] = sample;
    engine.start = (engine.start + 1U) % EngineTrailEngine::Capacity;
  }
}

void age_samples(EngineTrailEngine& engine, float dt_seconds, float lifetime_seconds) {
  const float scaled_dt = std::clamp(dt_seconds, 0.0F, lifetime_seconds);
  for (std::size_t index = 0; index < engine.count; ++index) {
    engine.samples[sample_index(engine, index)].age_seconds += scaled_dt;
  }
  while (engine.count > 0U && sample_at(engine, 0U).age_seconds > lifetime_seconds) {
    engine.start = (engine.start + 1U) % EngineTrailEngine::Capacity;
    --engine.count;
  }
}

[[nodiscard]] bool should_sample(
  const EngineTrailEngine& engine,
  Vec2 position,
  Vec2 direction,
  float intensity,
  const SectorTuning& sector,
  const EngineTrailTuning& tuning
) {
  if (intensity <= 0.001F) {
    return false;
  }
  if (!engine.has_previous || engine.count == 0U) {
    return true;
  }
  if (engine.previous_intensity <= 0.001F && intensity > 0.001F) {
    return true;
  }
  if (engine.sample_accumulator_seconds >= tuning.sample_interval_seconds) {
    return true;
  }
  if (length(wrapped_delta(engine.previous_position, position, sector)) >= tuning.movement_sample_distance) {
    return true;
  }
  if (dot(normalize_or_zero(engine.previous_direction), normalize_or_zero(direction)) <= tuning.direction_sample_dot) {
    return true;
  }
  return std::abs(intensity - engine.previous_intensity) >= tuning.thrust_sample_delta;
}

[[nodiscard]] bool should_force_sample(
  const EngineTrailEngine& engine,
  Vec2 position,
  Vec2 direction,
  float intensity,
  const SectorTuning& sector,
  const EngineTrailTuning& tuning
) {
  const float saved_accumulator = engine.sample_accumulator_seconds;
  EngineTrailEngine probe = engine;
  probe.sample_accumulator_seconds = 0.0F;
  const bool result = should_sample(probe, position, direction, intensity, sector, tuning);
  (void)saved_accumulator;
  return result;
}

void reset_engine(EngineTrailEngine& engine, Vec2 position, Vec2 direction, float intensity) {
  engine.start = 0U;
  engine.count = 0U;
  engine.sample_accumulator_seconds = 0.0F;
  engine.previous_intensity = intensity;
  engine.previous_position = position;
  engine.previous_direction = direction;
  engine.source_intensity = intensity;
  engine.source_position = position;
  engine.source_direction = direction;
  engine.has_previous = true;
}

[[nodiscard]] bool finite(Vec2 value) {
  return std::isfinite(value.x) && std::isfinite(value.y);
}

}  // namespace

void reset_engine_trail(EngineTrailModel& model) {
  for (EngineTrailEngine& engine : model.engines) {
    engine = {};
    engine.previous_direction = {.x = -1.0F, .y = 0.0F};
  }
  model.sources = {};
  model.active_sources = 0U;
}

EngineTrailUpdate update_engine_trail(
  EngineTrailModel& model,
  const ShipMotion& ship,
  const SemanticInputFrame& input,
  const SectorTuning& sector,
  float dt_seconds,
  const EngineTrailTuning& tuning
) {
  const float intensity = thrust_intensity(ship, input);
  const Vec2 exhaust_direction = direction_from_angle(ship.facing_radians + (TauRadians * 0.5F));
  std::array<EngineTrailNozzle, 2> nozzles{};

  for (std::size_t index = 0; index < nozzles.size(); ++index) {
    nozzles[index] = EngineTrailNozzle{
      .world_position = wrap_position(ship.position + nozzle_offset(index, ship.facing_radians), sector),
      .exhaust_direction = exhaust_direction,
      .intensity = intensity,
    };
  }

  return update_engine_trail_from_nozzles(model, nozzles, sector, dt_seconds, tuning);
}

EngineTrailUpdate update_engine_trail_from_nozzles(
  EngineTrailModel& model,
  std::span<const EngineTrailNozzle> nozzles,
  const SectorTuning& sector,
  float dt_seconds,
  const EngineTrailTuning& tuning
) {
  const float scaled_dt = std::clamp(dt_seconds, 0.0F, tuning.sample_lifetime_seconds);
  EngineTrailUpdate update{};

  for (std::size_t index = 0; index < model.engines.size(); ++index) {
    EngineTrailEngine& engine = model.engines[index];
    if (index >= nozzles.size()) {
      engine.sample_accumulator_seconds += scaled_dt;
      age_samples(engine, scaled_dt, tuning.sample_lifetime_seconds);
      engine.previous_intensity = 0.0F;
      continue;
    }

    const EngineTrailNozzle& nozzle = nozzles[index];
    const Vec2 position = wrap_position(nozzle.world_position, sector);
    const Vec2 exhaust_direction = nozzle.exhaust_direction;
    const float intensity = std::clamp(nozzle.intensity, 0.0F, 1.0F);
    if (!finite(position) || !finite(exhaust_direction)) {
      reset_engine(engine, {}, {.x = -1.0F, .y = 0.0F}, 0.0F);
      continue;
    }

    if (engine.has_previous && length(wrapped_delta(engine.previous_position, position, sector)) > tuning.teleport_distance) {
      reset_engine(engine, position, exhaust_direction, intensity);
    }

    engine.sample_accumulator_seconds += scaled_dt;
    age_samples(engine, scaled_dt, tuning.sample_lifetime_seconds);
    if (intensity > 0.001F) {
      engine.source_intensity = intensity;
      engine.source_position = position;
      engine.source_direction = exhaust_direction;
    } else {
      const float decay_seconds = std::max(tuning.source_decay_seconds, std::numeric_limits<float>::epsilon());
      engine.source_intensity = std::max(0.0F, engine.source_intensity - (scaled_dt / decay_seconds));
      engine.source_position = position;
      engine.source_direction = exhaust_direction;
    }
    const bool force_sample = should_force_sample(engine, position, exhaust_direction, intensity, sector, tuning);
    if (force_sample) {
      push_sample(engine, EngineTrailSample{.world_position = position, .exhaust_direction = exhaust_direction, .intensity = intensity});
      if (engine.sample_accumulator_seconds >= tuning.sample_interval_seconds) {
        engine.sample_accumulator_seconds -= tuning.sample_interval_seconds;
      } else {
        engine.sample_accumulator_seconds = 0.0F;
      }
    }
    int interval_samples = 0;
    while (intensity > 0.001F && engine.sample_accumulator_seconds >= tuning.sample_interval_seconds && interval_samples < 4) {
      push_sample(engine, EngineTrailSample{.world_position = position, .exhaust_direction = exhaust_direction, .intensity = intensity});
      engine.sample_accumulator_seconds -= tuning.sample_interval_seconds;
      ++interval_samples;
    }
    engine.previous_position = position;
    engine.previous_direction = exhaust_direction;
    engine.previous_intensity = intensity;
    engine.has_previous = true;

    if (engine.source_intensity > 0.001F && update.active_sources < update.sources.size()) {
      update.sources[update.active_sources] =
        EngineSourceDraw{.position = engine.source_position, .exhaust_direction = engine.source_direction, .intensity = engine.source_intensity};
      ++update.active_sources;
    }
  }

  model.sources = update.sources;
  model.active_sources = update.active_sources;
  return update;
}

std::vector<EngineTrailVertex> build_engine_trail_ribbon(
  const EngineTrailEngine& engine,
  const SectorTuning& sector,
  const EngineTrailTuning& tuning
) {
  std::vector<EngineTrailVertex> vertices;
  if (engine.count < 2U) {
    return vertices;
  }
  vertices.reserve(engine.count * 2U);

  for (std::size_t index = 0; index < engine.count; ++index) {
    const EngineTrailSample& sample = sample_at(engine, index);
    Vec2 direction = sample.exhaust_direction;
    if (index + 1U < engine.count) {
      direction = wrapped_delta(sample.world_position, sample_at(engine, index + 1U).world_position, sector);
    } else if (index > 0U) {
      direction = wrapped_delta(sample_at(engine, index - 1U).world_position, sample.world_position, sector);
    }
    direction = normalize_or_zero(direction);
    if (length(direction) <= 0.0001F) {
      direction = normalize_or_zero(sample.exhaust_direction);
    }
    if (length(direction) <= 0.0001F) {
      direction = {.x = -1.0F, .y = 0.0F};
    }
    const Vec2 perpendicular{.x = -direction.y, .y = direction.x};
    const float normalized_age = std::clamp(sample.age_seconds / std::max(tuning.sample_lifetime_seconds, std::numeric_limits<float>::epsilon()), 0.0F, 1.0F);
    const float width = tuning.base_width + ((tuning.max_width - tuning.base_width) * std::clamp(sample.intensity, 0.0F, 1.0F));
    vertices.push_back({
      .position = wrap_position(sample.world_position + (perpendicular * (width * 0.5F)), sector),
      .normalized_age = normalized_age,
      .intensity = sample.intensity,
      .side = -1.0F,
    });
    vertices.push_back({
      .position = wrap_position(sample.world_position - (perpendicular * (width * 0.5F)), sector),
      .normalized_age = normalized_age,
      .intensity = sample.intensity,
      .side = 1.0F,
    });
  }

  return vertices;
}

}  // namespace hyperverse
