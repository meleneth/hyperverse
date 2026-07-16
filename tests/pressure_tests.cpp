#include "test_common.hpp"

using hyperverse::test::TestAccountWorld;

TEST_CASE("sector pressure escalates on a tunable interval") {
  hyperverse::SectorPressureModel pressure;
  const hyperverse::SectorPressureTuning tuning{
    .escalation_interval_seconds = 10.0F,
    .announcement_duration_seconds = 3.0F,
    .pressure_per_level = 0.25F,
  };

  const hyperverse::SectorPressureHudSnapshot before = hyperverse::update_sector_pressure(pressure, 9.0F, tuning);
  CHECK(before.escalation_level == 0);
  CHECK(before.next_escalation_seconds == Catch::Approx(1.0F));
  CHECK(before.escalation_progress_fraction == Catch::Approx(0.9F));
  CHECK_FALSE(before.escalation_announced);

  const hyperverse::SectorPressureHudSnapshot after = hyperverse::update_sector_pressure(pressure, 1.0F, tuning);
  CHECK(after.escalation_level == 1);
  CHECK(after.next_escalation_seconds == Catch::Approx(10.0F));
  CHECK(after.escalation_progress_fraction == Catch::Approx(0.0F));
  CHECK(after.pressure_fraction == Catch::Approx(0.25F));
  CHECK(after.escalation_announced);
}

TEST_CASE("sector pressure eventually opens a terminal space tear") {
  hyperverse::SectorPressureModel pressure;

  const hyperverse::SectorPressureHudSnapshot hud = hyperverse::update_sector_pressure(
    pressure,
    30.0F,
    {.escalation_interval_seconds = 10.0F, .universe_tear_level = 3}
  );

  CHECK(hud.escalation_level == 3);
  CHECK(hud.universe_tear_open);
  CHECK(pressure.universe_tear_open);
}

TEST_CASE("sector pressure announcement expires after the HUD window") {
  hyperverse::SectorPressureModel pressure;
  const hyperverse::SectorPressureTuning tuning{
    .escalation_interval_seconds = 10.0F,
    .announcement_duration_seconds = 3.0F,
  };

  (void)hyperverse::update_sector_pressure(pressure, 10.0F, tuning);
  const hyperverse::SectorPressureHudSnapshot expired = hyperverse::update_sector_pressure(pressure, 3.1F, tuning);

  CHECK(expired.escalation_level == 1);
  CHECK_FALSE(expired.escalation_announced);
}
