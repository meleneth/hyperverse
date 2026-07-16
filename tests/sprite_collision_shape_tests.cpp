#include "test_common.hpp"

#include "hyperverse/sprite_collision_shape.hpp"

#include <algorithm>

TEST_CASE("sprite silhouettes extract a normalized alpha hull") {
  hyperverse::SpriteAlphaMask mask{
    .width = 5,
    .height = 5,
    .rgba = std::vector<std::uint8_t>(5U * 5U * 4U, 0U),
  };

  const auto set_alpha = [&](std::uint32_t x, std::uint32_t y) {
    const std::size_t index = ((static_cast<std::size_t>(y) * mask.width) + x) * 4U;
    mask.rgba[index + 3U] = 255U;
  };

  set_alpha(2, 0);
  set_alpha(1, 1);
  set_alpha(2, 1);
  set_alpha(3, 1);
  set_alpha(0, 2);
  set_alpha(1, 2);
  set_alpha(2, 2);
  set_alpha(3, 2);
  set_alpha(4, 2);
  set_alpha(1, 3);
  set_alpha(2, 3);
  set_alpha(3, 3);
  set_alpha(2, 4);

  const hyperverse::SpriteSilhouette silhouette = hyperverse::extract_sprite_silhouette(mask);

  REQUIRE(silhouette.hull.size() == 4U);
  CHECK(std::ranges::any_of(silhouette.hull, [](hyperverse::Vec2 point) { return point.y > 0.7F; }));
  CHECK(std::ranges::any_of(silhouette.hull, [](hyperverse::Vec2 point) { return point.y < -0.7F; }));
  CHECK(std::ranges::any_of(silhouette.hull, [](hyperverse::Vec2 point) { return point.x < -0.7F; }));
  CHECK(std::ranges::any_of(silhouette.hull, [](hyperverse::Vec2 point) { return point.x > 0.7F; }));
}
