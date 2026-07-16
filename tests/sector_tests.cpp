#include "test_common.hpp"

using hyperverse::test::TestAccountWorld;

TEST_CASE("wrapped sector distance uses the shortest edge crossing") {
  const hyperverse::SectorTuning sector{.width = 9000.0F, .height = 9000.0F};
  const hyperverse::Vec2 from{.x = 8950.0F, .y = 100.0F};
  const hyperverse::Vec2 to{.x = 25.0F, .y = 100.0F};

  CHECK(hyperverse::wrapped_delta(from, to, sector).x == Catch::Approx(75.0F));
  CHECK(hyperverse::wrapped_distance(from, to, sector) == Catch::Approx(75.0F));
  CHECK(hyperverse::wrap_position({.x = -10.0F, .y = 9010.0F}, sector).x == Catch::Approx(8990.0F));
  CHECK(hyperverse::wrap_position({.x = -10.0F, .y = 9010.0F}, sector).y == Catch::Approx(10.0F));
}
