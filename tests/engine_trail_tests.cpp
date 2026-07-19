#include "test_common.hpp"

#include <array>
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

TEST_CASE("engine trail accepts renderer neutral nozzle emitters") {
  hyperverse::EngineTrailModel model;
  const hyperverse::EngineTrailTuning tuning{.sample_interval_seconds = 0.01F};
  const std::array<hyperverse::EngineTrailNozzle, 2> nozzles{{
    hyperverse::EngineTrailNozzle{.world_position = {.x = 100.0F, .y = 100.0F}, .exhaust_direction = {.x = -1.0F, .y = 0.0F}, .intensity = 0.75F},
    hyperverse::EngineTrailNozzle{.world_position = {.x = 100.0F, .y = 130.0F}, .exhaust_direction = {.x = -1.0F, .y = 0.0F}, .intensity = 0.75F},
  }};

  const hyperverse::EngineTrailUpdate update = hyperverse::update_engine_trail_from_nozzles(model, nozzles, Sector, 0.02F, tuning);

  CHECK(update.active_sources == 2U);
  CHECK(model.engines[0].count == 2U);
  CHECK(model.engines[1].count == 2U);
  CHECK(model.sources[0].position.x == Catch::Approx(100.0F));
  CHECK(model.sources[1].position.y == Catch::Approx(130.0F));
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

TEST_CASE("engine source decays after thrust instead of disappearing between frames") {
  hyperverse::EngineTrailModel model;
  hyperverse::ShipMotion ship{.position = {.x = 1000.0F, .y = 1000.0F}};
  const hyperverse::EngineTrailTuning tuning{.sample_interval_seconds = 0.01F, .source_decay_seconds = 0.25F};

  (void)hyperverse::update_engine_trail(model, ship, thrust(), Sector, 0.02F, tuning);
  const hyperverse::EngineTrailUpdate update = hyperverse::update_engine_trail(model, ship, {}, Sector, 0.05F, tuning);

  REQUIRE(update.active_sources == 2U);
  CHECK(model.engines[0].source_phase == hyperverse::EngineSourcePhase::Decaying);
  CHECK(update.sources[0].intensity > 0.0F);
  CHECK(update.sources[0].intensity < 1.0F);
}

TEST_CASE("engine source transitions through active decay and dormant phases") {
  hyperverse::EngineTrailModel model;
  hyperverse::ShipMotion ship{.position = {.x = 1000.0F, .y = 1000.0F}};
  const hyperverse::EngineTrailTuning tuning{.sample_interval_seconds = 0.01F, .source_decay_seconds = 0.10F};

  (void)hyperverse::update_engine_trail(model, ship, thrust(), Sector, 0.02F, tuning);
  CHECK(model.engines[0].source_phase == hyperverse::EngineSourcePhase::Active);

  const hyperverse::Vec2 lit_position = model.engines[0].source_position;
  ship.position.x += 40.0F;
  (void)hyperverse::update_engine_trail(model, ship, {}, Sector, 0.05F, tuning);
  CHECK(model.engines[0].source_phase == hyperverse::EngineSourcePhase::Decaying);
  CHECK(model.engines[0].source_position.x == Catch::Approx(lit_position.x));

  const hyperverse::EngineTrailUpdate dark = hyperverse::update_engine_trail(model, ship, {}, Sector, 0.10F, tuning);
  CHECK(model.engines[0].source_phase == hyperverse::EngineSourcePhase::Dormant);
  CHECK(dark.active_sources == 0U);
}

TEST_CASE("player engine trail stays alive during sustained long thrust") {
  hyperverse::EngineTrailModel model;
  hyperverse::ShipMotion ship{.position = {.x = 1000.0F, .y = 1000.0F}, .facing_radians = 0.0F};
  const hyperverse::EngineTrailTuning tuning{};

  for (int tick = 0; tick < 1200; ++tick) {
    ship.position = hyperverse::wrap_position(ship.position + hyperverse::Vec2{.x = 15.0F, .y = 0.0F}, Sector);
    (void)hyperverse::update_engine_trail(model, ship, thrust(), Sector, 1.0F / 60.0F, tuning);
  }

  CHECK(model.active_sources == 2U);
  CHECK(model.sources[0].intensity == Catch::Approx(1.0F));
  CHECK(model.engines[0].count >= 2U);
  CHECK_FALSE(hyperverse::build_engine_trail_ribbon(model.engines[0], Sector, tuning).empty());
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

TEST_CASE("engine trail resumes after gravity sling release") {
  entt::registry registry;
  const entt::entity target = registry.create();
  registry.emplace<hyperverse::AsteroidBody>(
    target,
    hyperverse::AsteroidBody{.position = {.x = 1300.0F, .y = 1000.0F}, .radius = 100.0F}
  );
  hyperverse::GravitySlingModel sling;
  hyperverse::EngineTrailModel trail;
  hyperverse::ShipMotion ship{.position = {.x = 1000.0F, .y = 1000.0F}, .velocity = {.x = 0.0F, .y = 180.0F}};

  (void)hyperverse::update_engine_trail(trail, ship, thrust(), Sector, 1.0F / 60.0F);
  (void)hyperverse::update_gravity_sling(
    sling,
    registry,
    ship,
    {.primary_aim = {.x = 1.0F}, .gravity_sling_requested = true},
    Sector,
    0.0F,
    {.engagement_seconds = 0.01F}
  );
  (void)hyperverse::update_gravity_sling(sling, registry, ship, thrust(), Sector, 1.0F / 60.0F, {.engagement_seconds = 0.01F});
  REQUIRE(sling.phase == hyperverse::GravitySlingPhase::Active);

  (void)hyperverse::update_gravity_sling(sling, registry, ship, {.gravity_sling_requested = true}, Sector, 1.0F / 60.0F);
  REQUIRE(sling.phase == hyperverse::GravitySlingPhase::FreeFlight);

  ship.position = hyperverse::wrap_position(ship.position + hyperverse::Vec2{.x = 12.0F, .y = 0.0F}, Sector);
  const hyperverse::EngineTrailUpdate update = hyperverse::update_engine_trail(trail, ship, thrust(), Sector, 1.0F / 60.0F);

  CHECK(update.active_sources == 2U);
  CHECK(trail.sources[0].intensity == Catch::Approx(1.0F));
  CHECK(sample_count(trail) >= 4U);
}
