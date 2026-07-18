#include "test_common.hpp"

#include "hyperverse/sprite_frame_builder.hpp"
#include "hyperverse/vertical_slice_seed.hpp"

using hyperverse::test::TestAccountWorld;

TEST_CASE("clear color stays stable now that sprites expose state") {
  const hyperverse::RenderColor idle = hyperverse::make_clear_color({});
  const hyperverse::RenderColor fast = hyperverse::make_clear_color({.speed_fraction = 1.0F});
  const hyperverse::RenderColor locked = hyperverse::make_clear_color({.target_locked = true});
  const hyperverse::RenderColor mining = hyperverse::make_clear_color({.mining_active = true});
  const hyperverse::RenderColor warning = hyperverse::make_clear_color({.speed_fraction = 1.0F, .wrap_warning = true});

  CHECK(fast.r == Catch::Approx(idle.r));
  CHECK(locked.g == Catch::Approx(idle.g));
  CHECK(mining.b == Catch::Approx(idle.b));
  CHECK(warning.r == Catch::Approx(idle.r));
}

TEST_CASE("sprite frame includes engine trail vertices after thrust history") {
  TestAccountWorld world;
  hyperverse::AccountCtx account = world.account_context();
  const hyperverse::VerticalSliceEntities entities = hyperverse::seed_vertical_slice(account);
  hyperverse::ShipMotion& ship = account.registry().get<hyperverse::ShipMotion>(entities.player);
  hyperverse::EngineTrailModel& engine_trail = account.registry().get<hyperverse::EngineTrailModel>(entities.player);
  const hyperverse::SectorTuning sector = hyperverse::default_sector();
  const hyperverse::SemanticInputFrame input{.desired_movement = {.x = 1.0F, .y = 0.0F}};

  (void)hyperverse::update_engine_trail(engine_trail, ship, input, sector, 1.0F / 60.0F);
  ship.position.x += 24.0F;
  (void)hyperverse::update_engine_trail(engine_trail, ship, input, sector, 1.0F / 60.0F);

  const hyperverse::SpriteFrame frame = hyperverse::build_sprite_frame(
    account,
    entities.player,
    entities.mining_drones,
    entities.raider,
    hyperverse::make_flight_hud_snapshot(ship, input, {}, sector),
    input,
    sector,
    1280,
    720
  );

  CHECK_FALSE(frame.engine_trails.empty());
}
