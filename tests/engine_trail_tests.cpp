#include "test_common.hpp"

#include <cmath>

namespace {

constexpr hyperverse::SectorTuning Sector{.width = 9000.0F, .height = 9000.0F};

[[nodiscard]] hyperverse::SemanticInputFrame thrust(float amount = 1.0F) {
  return {.desired_movement = {.x = amount, .y = 0.0F}};
}

[[nodiscard]] std::size_t sample_count(const hyperverse::EngineTrailModel& model) {
  std::size_t count = 0U;
  for (const hyperverse::EngineTrailEngine& engine : model.engines) {
    count += engine.count;
  }
  return count;
}

}  // namespace

TEST_CASE("engine trail samples age and expire") {
  hyperverse::EngineTrailModel model;
  hyperverse::ShipMotion ship{.position = {.x = 1000.0F, .y = 1000.0F}};
  const hyperverse::EngineTrailTuning tuning{.sample_interval_seconds = 0.01F, .sample_lifetime_seconds = 0.10F};

  (void)hyperverse::update_engine_trail(model, ship, thrust(), Sector, 0.02F, tuning);
  REQUIRE(sample_count(model) == 4U);
  ship.position.x += 20.0F;
  (void)hyperverse::update_engine_trail(model, ship, thrust(), Sector, 0.05F, tuning);
  CHECK(model.engines[0].samples[model.engines[0].start].age_seconds == Catch::Approx(0.05F));
  ship.position.x += 20.0F;
  (void)hyperverse::update_engine_trail(model, ship, thrust(), Sector, 0.10F, tuning);

  CHECK(sample_count(model) <= hyperverse::EngineTrailEngine::Capacity * model.engines.size());
  for (const hyperverse::EngineTrailEngine& engine : model.engines) {
    for (std::size_t index = 0; index < engine.count; ++index) {
      const std::size_t physical_index = (engine.start + index) % hyperverse::EngineTrailEngine::Capacity;
      CHECK(engine.samples[physical_index].age_seconds <= tuning.sample_lifetime_seconds);
    }
  }
}

TEST_CASE("engine trail capacity remains bounded") {
  hyperverse::EngineTrailModel model;
  hyperverse::ShipMotion ship{.position = {.x = 1000.0F, .y = 1000.0F}};
  const hyperverse::EngineTrailTuning tuning{.sample_interval_seconds = 0.001F, .sample_lifetime_seconds = 10.0F};

  for (int index = 0; index < 80; ++index) {
    ship.position.x += 9.0F;
    (void)hyperverse::update_engine_trail(model, ship, thrust(), Sector, 0.01F, tuning);
  }

  CHECK(model.engines[0].count == hyperverse::EngineTrailEngine::Capacity);
  CHECK(model.engines[1].count == hyperverse::EngineTrailEngine::Capacity);
}

TEST_CASE("thrust off stops new active engine trail samples") {
  hyperverse::EngineTrailModel model;
  hyperverse::ShipMotion ship{.position = {.x = 1000.0F, .y = 1000.0F}};
  const hyperverse::EngineTrailTuning tuning{.sample_interval_seconds = 0.01F, .sample_lifetime_seconds = 1.0F};

  (void)hyperverse::update_engine_trail(model, ship, thrust(), Sector, 0.02F, tuning);
  const std::size_t before = sample_count(model);
  ship.position.x += 50.0F;
  const hyperverse::EngineTrailUpdate update = hyperverse::update_engine_trail(model, ship, {}, Sector, 0.20F, tuning);

  CHECK(update.active_sources == 0U);
  CHECK(sample_count(model) == before);
  CHECK(model.engines[0].samples[model.engines[0].start].age_seconds == Catch::Approx(0.20F));
}

TEST_CASE("engine trail reset clears history") {
  hyperverse::EngineTrailModel model;
  hyperverse::ShipMotion ship{.position = {.x = 1000.0F, .y = 1000.0F}};
  (void)hyperverse::update_engine_trail(model, ship, thrust(), Sector, 0.02F);

  hyperverse::reset_engine_trail(model);

  CHECK(sample_count(model) == 0U);
}

TEST_CASE("engine trail teleport detection clears stale history") {
  hyperverse::EngineTrailModel model;
  hyperverse::ShipMotion ship{.position = {.x = 1000.0F, .y = 1000.0F}};
  const hyperverse::EngineTrailTuning tuning{.sample_interval_seconds = 0.01F, .teleport_distance = 200.0F};

  (void)hyperverse::update_engine_trail(model, ship, thrust(), Sector, 0.02F, tuning);
  ship.position = {.x = 4000.0F, .y = 4000.0F};
  (void)hyperverse::update_engine_trail(model, ship, thrust(), Sector, 0.02F, tuning);

  CHECK(model.engines[0].count == 2U);
  CHECK(model.engines[1].count == 2U);
}

TEST_CASE("engine trail ribbon handles one or zero samples") {
  const hyperverse::EngineTrailModel model;

  CHECK(hyperverse::build_engine_trail_ribbon(model.engines[0], Sector).empty());
}

TEST_CASE("engine trail ribbon remains finite for zero motion samples") {
  hyperverse::EngineTrailEngine engine;
  engine.samples[0] = hyperverse::EngineTrailSample{.world_position = {.x = 100.0F, .y = 100.0F}, .exhaust_direction = {}};
  engine.samples[1] = hyperverse::EngineTrailSample{.world_position = {.x = 100.0F, .y = 100.0F}, .exhaust_direction = {}};
  engine.count = 2U;

  const std::vector<hyperverse::EngineTrailVertex> vertices = hyperverse::build_engine_trail_ribbon(engine, Sector);

  REQUIRE(vertices.size() == 4U);
  for (const hyperverse::EngineTrailVertex& vertex : vertices) {
    CHECK(std::isfinite(vertex.position.x));
    CHECK(std::isfinite(vertex.position.y));
  }
}

TEST_CASE("engine trail angular motion creates changing ribbon positions") {
  hyperverse::EngineTrailModel model;
  hyperverse::ShipMotion ship{.position = {.x = 1000.0F, .y = 1000.0F}, .facing_radians = 0.0F};
  const hyperverse::EngineTrailTuning tuning{.sample_interval_seconds = 0.01F};

  (void)hyperverse::update_engine_trail(model, ship, thrust(), Sector, 0.02F, tuning);
  ship.position.x += 20.0F;
  ship.facing_radians = 1.2F;
  (void)hyperverse::update_engine_trail(model, ship, thrust(), Sector, 0.02F, tuning);
  const std::vector<hyperverse::EngineTrailVertex> vertices = hyperverse::build_engine_trail_ribbon(model.engines[0], Sector, tuning);

  REQUIRE(vertices.size() >= 4U);
  CHECK(vertices[0].position.x != Catch::Approx(vertices[2].position.x));
  CHECK(vertices[0].position.y != Catch::Approx(vertices[2].position.y));
}

TEST_CASE("engine trail sampling is stable across differing frame delta sequences") {
  hyperverse::EngineTrailModel many_small;
  hyperverse::EngineTrailModel few_large;
  hyperverse::ShipMotion small_ship{.position = {.x = 1000.0F, .y = 1000.0F}};
  hyperverse::ShipMotion large_ship = small_ship;
  const hyperverse::EngineTrailTuning tuning{.sample_interval_seconds = 1.0F / 60.0F, .movement_sample_distance = 1000.0F};

  for (int index = 0; index < 6; ++index) {
    small_ship.position.x += 10.0F;
    (void)hyperverse::update_engine_trail(many_small, small_ship, thrust(), Sector, 1.0F / 60.0F, tuning);
  }
  for (int index = 0; index < 3; ++index) {
    large_ship.position.x += 20.0F;
    (void)hyperverse::update_engine_trail(few_large, large_ship, thrust(), Sector, 1.0F / 30.0F, tuning);
  }

  CHECK(many_small.engines[0].count == few_large.engines[0].count);
  CHECK(many_small.engines[1].count == few_large.engines[1].count);
}
