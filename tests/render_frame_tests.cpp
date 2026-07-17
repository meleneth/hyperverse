#include "test_common.hpp"

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
