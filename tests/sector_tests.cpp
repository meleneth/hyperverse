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

TEST_CASE("default sector dimensions are fixed at nine 2k reference screens") {
  const hyperverse::SectorTuning sector = hyperverse::default_sector();

  CHECK(sector.width == Catch::Approx((1920.0F / hyperverse::PixelsPerWorldUnit) * 9.0F));
  CHECK(sector.height == Catch::Approx((1080.0F / hyperverse::PixelsPerWorldUnit) * 9.0F));
}

TEST_CASE("viewport-derived sector helper remains explicit for tests and tools") {
  const hyperverse::SectorTuning sector = hyperverse::sector_from_viewport(1920.0F, 1080.0F);

  CHECK(sector.width == Catch::Approx((1920.0F / hyperverse::PixelsPerWorldUnit) * 9.0F));
  CHECK(sector.height == Catch::Approx((1080.0F / hyperverse::PixelsPerWorldUnit) * 9.0F));
}
