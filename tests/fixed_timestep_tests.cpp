#include "test_common.hpp"

using hyperverse::test::TestAccountWorld;

TEST_CASE("fixed timestep consumes deterministic simulation ticks") {
  hyperverse::FixedTimestep timestep{0.25F};
  timestep.accumulate(0.74F);

  CHECK(timestep.consume_tick());
  CHECK(timestep.consume_tick());
  CHECK_FALSE(timestep.consume_tick());
  CHECK(timestep.alpha() == Catch::Approx(0.96F));
}
