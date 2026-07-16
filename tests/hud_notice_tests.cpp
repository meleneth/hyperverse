#include "test_common.hpp"

#include "hyperverse/hud_notice.hpp"

TEST_CASE("HUD notices remain visible for important messages") {
  hyperverse::HudNotice notice;
  hyperverse::push_hud_notice(notice, "CONVOY UNDER ATTACK", 2.0F);

  hyperverse::update_hud_notice(notice, 1.0F);
  CHECK(notice.message == "CONVOY UNDER ATTACK");

  hyperverse::update_hud_notice(notice, 1.0F);
  CHECK(notice.message.empty());
}
