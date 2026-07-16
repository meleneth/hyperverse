#include "test_common.hpp"

#include "hyperverse/ship_status.hpp"

TEST_CASE("ship shields regenerate but armor does not") {
  hyperverse::ShipHealth health{.armor = 42.0F, .shields = 10.0F, .shield_regen_per_second = 5.0F};
  hyperverse::RoundTimer round_timer{.duration_seconds = 30.0F};

  hyperverse::update_ship_status(health, round_timer, 3.0F);

  CHECK(health.armor == Catch::Approx(42.0F));
  CHECK(health.shields == Catch::Approx(25.0F));
  CHECK(round_timer.elapsed_seconds == Catch::Approx(3.0F));
}

TEST_CASE("round timer caps at duration") {
  hyperverse::ShipHealth health{};
  hyperverse::RoundTimer round_timer{.elapsed_seconds = 29.0F, .duration_seconds = 30.0F};

  hyperverse::update_ship_status(health, round_timer, 10.0F);

  CHECK(round_timer.elapsed_seconds == Catch::Approx(30.0F));
}
