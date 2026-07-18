#include "test_common.hpp"

#include "hyperverse/sprite_frame_builder.hpp"
#include "hyperverse/vertical_slice_seed.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>

using hyperverse::test::TestAccountWorld;

namespace {

[[nodiscard]] std::size_t test_ore_tier_index(hyperverse::OreTier tier) {
  switch (tier) {
    case hyperverse::OreTier::Common:
      return 0;
    case hyperverse::OreTier::Industrial:
      return 1;
    case hyperverse::OreTier::Rare:
      return 2;
    case hyperverse::OreTier::Exotic:
      return 3;
    case hyperverse::OreTier::Anomalous:
      return 4;
  }

  return 0;
}

}  // namespace

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

TEST_CASE("sprite frame includes deterministic parallax starfield") {
  TestAccountWorld world;
  hyperverse::AccountCtx account = world.account_context();
  const hyperverse::VerticalSliceEntities entities = hyperverse::seed_vertical_slice(account);
  hyperverse::ShipMotion& ship = account.registry().get<hyperverse::ShipMotion>(entities.player);
  hyperverse::CameraState& camera = account.registry().get<hyperverse::CameraState>(entities.player);
  const hyperverse::SectorTuning sector = hyperverse::default_sector();
  const hyperverse::SemanticInputFrame input{};

  const hyperverse::SpriteFrame first = hyperverse::build_sprite_frame(
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
  const hyperverse::SpriteFrame repeated = hyperverse::build_sprite_frame(
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
  camera.position.x += 800.0F;
  const hyperverse::SpriteFrame moved = hyperverse::build_sprite_frame(
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

  REQUIRE(first.stars.size() > 40U);
  REQUIRE(first.stars.size() == repeated.stars.size());
  CHECK(first.stars.front().x_ndc == Catch::Approx(repeated.stars.front().x_ndc));
  CHECK(first.stars.front().r == Catch::Approx(repeated.stars.front().r));

  bool saw_changed_position = false;
  bool saw_different_shades = false;
  for (std::size_t index = 0; index < std::min(first.stars.size(), moved.stars.size()); ++index) {
    const hyperverse::StarDraw& star = first.stars[index];
    CHECK(std::isfinite(star.x_ndc));
    CHECK(std::isfinite(star.y_ndc));
    CHECK(star.r == Catch::Approx(star.g));
    CHECK(star.g == Catch::Approx(star.b));
    CHECK(star.r >= 0.19F);
    CHECK(star.r <= 0.85F);
    if (first.stars[index].x_ndc != Catch::Approx(moved.stars[index].x_ndc)) {
      saw_changed_position = true;
    }
    if (index > 0U && first.stars[index].r != Catch::Approx(first.stars[0].r)) {
      saw_different_shades = true;
    }
  }

  CHECK(saw_changed_position);
  CHECK(saw_different_shades);
}

TEST_CASE("sprite frame fades active raiders in from cloak") {
  TestAccountWorld world;
  hyperverse::AccountCtx account = world.account_context();
  const hyperverse::VerticalSliceEntities entities = hyperverse::seed_vertical_slice(account);
  hyperverse::ShipMotion& ship = account.registry().get<hyperverse::ShipMotion>(entities.player);
  const hyperverse::SectorTuning sector = hyperverse::default_sector();
  const hyperverse::SemanticInputFrame input{};
  const entt::entity raider = account.registry().create();
  account.registry().emplace<hyperverse::RaiderShip>(
    raider,
    hyperverse::RaiderShip{
      .position = {.x = ship.position.x + 300.0F, .y = ship.position.y},
      .role = hyperverse::RaiderRole::Combat,
      .cloak_fade_seconds = 0.575F,
    }
  );

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

  const auto visible_raider = std::ranges::find_if(frame.sprites, [](const hyperverse::SpriteDraw& sprite) {
    return sprite.texture == hyperverse::SpriteTexture::Drone && sprite.tint_r == Catch::Approx(0.95F);
  });

  REQUIRE(visible_raider != frame.sprites.end());
  CHECK(visible_raider->tint_a == Catch::Approx(0.5F));
}

TEST_CASE("sprite frame renders all large asteroids in right side radar panel") {
  TestAccountWorld world;
  hyperverse::AccountCtx account = world.account_context();
  const hyperverse::VerticalSliceEntities entities = hyperverse::seed_vertical_slice(account);
  hyperverse::ShipMotion& ship = account.registry().get<hyperverse::ShipMotion>(entities.player);
  const hyperverse::SectorTuning sector = hyperverse::default_sector();
  const hyperverse::SemanticInputFrame input{};
  int large_asteroids = 0;
  std::array<int, hyperverse::OreTierCount> large_asteroids_by_tier{};
  for (auto [entity, asteroid] : account.registry().view<hyperverse::AsteroidBody>().each()) {
    if (asteroid.radius >= 400.0F) {
      ++large_asteroids;
      const hyperverse::MiningResource* resource = account.registry().try_get<hyperverse::MiningResource>(entity);
      REQUIRE(resource != nullptr);
      ++large_asteroids_by_tier[test_ore_tier_index(resource->tier)];
    }
  }

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

  std::array<int, hyperverse::OreTierCount> marker_lines_by_tier{};
  for (const hyperverse::LineDraw& line : frame.lines) {
    const bool inside_asteroid_radar =
      line.start_x_ndc >= 0.56F && line.start_x_ndc <= 1.0F && line.end_x_ndc >= 0.56F && line.end_x_ndc <= 1.0F &&
      line.start_y_ndc >= -0.14F && line.start_y_ndc <= 0.30F && line.end_y_ndc >= -0.14F && line.end_y_ndc <= 0.30F;
    if (!inside_asteroid_radar) {
      continue;
    }

    auto tier = std::optional<hyperverse::OreTier>{};
    for (const hyperverse::OreTier candidate : {
           hyperverse::OreTier::Common,
           hyperverse::OreTier::Industrial,
           hyperverse::OreTier::Rare,
           hyperverse::OreTier::Exotic,
           hyperverse::OreTier::Anomalous,
         }) {
      const hyperverse::OreTint tint = hyperverse::ore_tint(candidate);
      if (std::abs(line.r - tint.r) <= 0.001F && std::abs(line.g - tint.g) <= 0.001F && std::abs(line.b - tint.b) <= 0.001F) {
        tier = candidate;
        break;
      }
    }
    if (!tier.has_value()) {
      continue;
    }

    ++marker_lines_by_tier[test_ore_tier_index(*tier)];
  }

  CHECK(large_asteroids > 0);
  for (std::size_t index = 0; index < large_asteroids_by_tier.size(); ++index) {
    CHECK(marker_lines_by_tier[index] == large_asteroids_by_tier[index] * 2);
  }
}

TEST_CASE("sprite frame teaches cargo colors with mineral container legend") {
  TestAccountWorld world;
  hyperverse::AccountCtx account = world.account_context();
  const hyperverse::VerticalSliceEntities entities = hyperverse::seed_vertical_slice(account);
  hyperverse::ShipMotion& ship = account.registry().get<hyperverse::ShipMotion>(entities.player);
  const hyperverse::SectorTuning sector = hyperverse::default_sector();
  const hyperverse::SemanticInputFrame input{};

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

  std::array<int, hyperverse::OreTierCount> legend_lines_by_tier{};
  for (const hyperverse::LineDraw& line : frame.lines) {
    const bool inside_cargo_legend =
      line.start_x_ndc >= -0.97F && line.start_x_ndc <= -0.91F && line.end_x_ndc >= -0.97F && line.end_x_ndc <= -0.91F &&
      line.start_y_ndc >= -0.76F && line.start_y_ndc <= -0.54F && line.end_y_ndc >= -0.76F && line.end_y_ndc <= -0.54F;
    if (!inside_cargo_legend) {
      continue;
    }

    for (const hyperverse::OreTier candidate : {
           hyperverse::OreTier::Common,
           hyperverse::OreTier::Industrial,
           hyperverse::OreTier::Rare,
           hyperverse::OreTier::Exotic,
           hyperverse::OreTier::Anomalous,
         }) {
      const hyperverse::OreTint tint = hyperverse::ore_tint(candidate);
      if (std::abs(line.r - tint.r) <= 0.001F && std::abs(line.g - tint.g) <= 0.001F && std::abs(line.b - tint.b) <= 0.001F) {
        ++legend_lines_by_tier[test_ore_tier_index(candidate)];
        break;
      }
    }
  }

  for (const int line_count : legend_lines_by_tier) {
    CHECK(line_count >= 15);
  }
}
