#include "hyperverse/sprite_frame_builder.hpp"

#include "hyperverse/asteroid_mass.hpp"
#include "hyperverse/asteroid_geometry.hpp"
#include "hyperverse/camera.hpp"
#include "hyperverse/cargo_box.hpp"
#include "hyperverse/cargo_escort.hpp"
#include "hyperverse/cargo_manifest.hpp"
#include "hyperverse/cargo_route.hpp"
#include "hyperverse/cargo_train.hpp"
#include "hyperverse/collision.hpp"
#include "hyperverse/drone.hpp"
#include "hyperverse/engine_trail.hpp"
#include "hyperverse/gravity_sling.hpp"
#include "hyperverse/hud_notice.hpp"
#include "hyperverse/mining.hpp"
#include "hyperverse/pressure.hpp"
#include "hyperverse/projectile.hpp"
#include "hyperverse/raider.hpp"
#include "hyperverse/radar_hud.hpp"
#include "hyperverse/ship_status.hpp"
#include "hyperverse/targeting.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <numbers>
#include <string>
#include <string_view>
#include <tuple>

namespace {

constexpr float ScreenAnchorYFraction = 0.75F;
constexpr std::array<float, 4> StarLayerParallax{0.08F, 0.18F, 0.34F, 0.56F};
constexpr std::array<float, 4> StarLayerCellSize{420.0F, 340.0F, 270.0F, 215.0F};
constexpr std::array<float, 4> StarLayerPixelSize{1.0F, 1.25F, 1.55F, 1.9F};
constexpr std::array<float, 4> StarLayerBaseGrey{0.20F, 0.32F, 0.46F, 0.62F};
constexpr std::array<float, 4> StarLayerGreyRange{0.16F, 0.18F, 0.20F, 0.22F};
constexpr float RaiderCloakFadeSeconds = 1.15F;
constexpr float AsteroidRadarCenterX = 0.78F;
constexpr float AsteroidRadarCenterY = 0.08F;
constexpr float AsteroidRadarHalfSize = 0.20F;
constexpr float AsteroidRadarMinimumRadius = 400.0F;

[[nodiscard]] hyperverse::SpriteDraw make_world_sprite(
  hyperverse::SpriteTexture texture,
  hyperverse::Vec2 world_position,
  hyperverse::Vec2 camera_position,
  const hyperverse::SectorTuning& sector,
  std::uint32_t width,
  std::uint32_t height,
  float pixel_size,
  float pixel_height,
  float rotation_radians
) {
  const hyperverse::Vec2 relative = hyperverse::wrapped_delta(camera_position, world_position, sector) * hyperverse::PixelsPerWorldUnit;
  const float screen_x = (static_cast<float>(width) * 0.5F) + relative.x;
  const float screen_y = (static_cast<float>(height) * ScreenAnchorYFraction) + relative.y;

  return {
    .texture = texture,
    .center_x_ndc = ((screen_x / static_cast<float>(width)) * 2.0F) - 1.0F,
    .center_y_ndc = ((screen_y / static_cast<float>(height)) * 2.0F) - 1.0F,
    .half_width_ndc = pixel_size / static_cast<float>(width),
    .half_height_ndc = pixel_height / static_cast<float>(height),
    .rotation_radians = rotation_radians,
  };
}

[[nodiscard]] hyperverse::Vec2 world_to_ndc(
  hyperverse::Vec2 world_position,
  hyperverse::Vec2 camera_position,
  const hyperverse::SectorTuning& sector,
  std::uint32_t width,
  std::uint32_t height
) {
  const hyperverse::Vec2 relative = hyperverse::wrapped_delta(camera_position, world_position, sector) * hyperverse::PixelsPerWorldUnit;
  const float screen_x = (static_cast<float>(width) * 0.5F) + relative.x;
  const float screen_y = (static_cast<float>(height) * ScreenAnchorYFraction) + relative.y;
  return {
    .x = ((screen_x / static_cast<float>(width)) * 2.0F) - 1.0F,
    .y = ((screen_y / static_cast<float>(height)) * 2.0F) - 1.0F,
  };
}

[[nodiscard]] std::uint32_t hash_star(std::int32_t x, std::int32_t y, std::uint32_t layer) {
  std::uint32_t seed = static_cast<std::uint32_t>(x) * 0x9E3779B9U;
  seed ^= static_cast<std::uint32_t>(y) * 0x85EBCA6BU;
  seed ^= (layer + 1U) * 0xC2B2AE35U;
  seed ^= seed >> 16U;
  seed *= 0x7FEB352DU;
  seed ^= seed >> 15U;
  seed *= 0x846CA68BU;
  seed ^= seed >> 16U;
  return seed;
}

[[nodiscard]] float hash_star_unit(std::uint32_t value) {
  return static_cast<float>(value >> 8U) * (1.0F / 16777215.0F);
}

[[nodiscard]] std::uint32_t mix_star_hash(std::uint32_t value) {
  value ^= value >> 16U;
  value *= 0x7FEB352DU;
  value ^= value >> 15U;
  value *= 0x846CA68BU;
  value ^= value >> 16U;
  return value;
}

void add_star(
  std::vector<hyperverse::StarDraw>& stars,
  float screen_x,
  float screen_y,
  float pixel_size,
  float grey,
  std::uint32_t width,
  std::uint32_t height
) {
  if (screen_x < -pixel_size || screen_x > static_cast<float>(width) + pixel_size || screen_y < -pixel_size ||
      screen_y > static_cast<float>(height) + pixel_size) {
    return;
  }

  stars.push_back(hyperverse::StarDraw{
    .x_ndc = ((screen_x / static_cast<float>(width)) * 2.0F) - 1.0F,
    .y_ndc = ((screen_y / static_cast<float>(height)) * 2.0F) - 1.0F,
    .half_size_x_ndc = pixel_size / static_cast<float>(width),
    .half_size_y_ndc = pixel_size / static_cast<float>(height),
    .r = grey,
    .g = grey,
    .b = grey,
    .a = 1.0F,
  });
}

void add_starfield(
  std::vector<hyperverse::StarDraw>& stars,
  hyperverse::Vec2 camera_position,
  std::uint32_t width,
  std::uint32_t height
) {
  const float screen_anchor_x = static_cast<float>(width) * 0.5F;
  const float screen_anchor_y = static_cast<float>(height) * ScreenAnchorYFraction;

  for (std::size_t layer = 0; layer < StarLayerParallax.size(); ++layer) {
    const float cell_size = StarLayerCellSize[layer];
    const hyperverse::Vec2 layer_camera = camera_position * (hyperverse::PixelsPerWorldUnit * StarLayerParallax[layer]);
    const float left = layer_camera.x - screen_anchor_x - cell_size;
    const float right = layer_camera.x + (static_cast<float>(width) - screen_anchor_x) + cell_size;
    const float top = layer_camera.y - screen_anchor_y - cell_size;
    const float bottom = layer_camera.y + (static_cast<float>(height) - screen_anchor_y) + cell_size;
    const std::int32_t min_x = static_cast<std::int32_t>(std::floor(left / cell_size));
    const std::int32_t max_x = static_cast<std::int32_t>(std::floor(right / cell_size));
    const std::int32_t min_y = static_cast<std::int32_t>(std::floor(top / cell_size));
    const std::int32_t max_y = static_cast<std::int32_t>(std::floor(bottom / cell_size));

    for (std::int32_t cell_y = min_y; cell_y <= max_y; ++cell_y) {
      for (std::int32_t cell_x = min_x; cell_x <= max_x; ++cell_x) {
        const std::uint32_t hash = hash_star(cell_x, cell_y, static_cast<std::uint32_t>(layer));
        const float jitter_x = hash_star_unit(hash);
        const float jitter_y = hash_star_unit(mix_star_hash(hash ^ 0xB5297A4DU));
        const float shade = StarLayerBaseGrey[layer] + (StarLayerGreyRange[layer] * hash_star_unit(hash ^ 0xA5115A7U));
        const float size = StarLayerPixelSize[layer] + (hash_star_unit(hash ^ 0x51EAD5U) * 0.85F);
        const float star_x = ((static_cast<float>(cell_x) + jitter_x) * cell_size) - layer_camera.x + screen_anchor_x;
        const float star_y = ((static_cast<float>(cell_y) + jitter_y) * cell_size) - layer_camera.y + screen_anchor_y;
        add_star(stars, star_x, star_y, size, shade, width, height);
      }
    }
  }
}

[[nodiscard]] hyperverse::Vec3 operator-(hyperverse::Vec3 lhs, hyperverse::Vec3 rhs) {
  return {.x = lhs.x - rhs.x, .y = lhs.y - rhs.y, .z = lhs.z - rhs.z};
}

[[nodiscard]] hyperverse::Vec3 operator+(hyperverse::Vec3 lhs, hyperverse::Vec3 rhs) {
  return {.x = lhs.x + rhs.x, .y = lhs.y + rhs.y, .z = lhs.z + rhs.z};
}

[[nodiscard]] hyperverse::Vec3 operator*(hyperverse::Vec3 value, float scale) {
  return {.x = value.x * scale, .y = value.y * scale, .z = value.z * scale};
}

[[nodiscard]] hyperverse::Vec3 cross(hyperverse::Vec3 lhs, hyperverse::Vec3 rhs) {
  return {
    .x = (lhs.y * rhs.z) - (lhs.z * rhs.y),
    .y = (lhs.z * rhs.x) - (lhs.x * rhs.z),
    .z = (lhs.x * rhs.y) - (lhs.y * rhs.x),
  };
}

[[nodiscard]] float dot(hyperverse::Vec3 lhs, hyperverse::Vec3 rhs) {
  return (lhs.x * rhs.x) + (lhs.y * rhs.y) + (lhs.z * rhs.z);
}

[[nodiscard]] hyperverse::Vec3 normalize_or_zero(hyperverse::Vec3 value) {
  const float magnitude = std::sqrt(dot(value, value));
  return magnitude > 0.0001F ? hyperverse::Vec3{.x = value.x / magnitude, .y = value.y / magnitude, .z = value.z / magnitude} : hyperverse::Vec3{};
}

[[nodiscard]] float fract(float value) {
  return value - std::floor(value);
}

[[nodiscard]] float surface_noise(hyperverse::Vec3 point) {
  return fract(std::sin(dot(point, {.x = 12.9898F, .y = 78.233F, .z = 37.719F})) * 43758.5453F);
}

[[nodiscard]] hyperverse::TriangleVertexDraw asteroid_triangle_vertex(
  hyperverse::Vec3 point,
  hyperverse::Vec3 rotated_point,
  hyperverse::Vec3 normal,
  hyperverse::Vec2 center,
  hyperverse::OreTint tint,
  std::uint32_t width,
  std::uint32_t height,
  float radius,
  float heat_r,
  float heat_g,
  float heat_b,
  float tint_blend
) {
  const hyperverse::Vec3 radial = normalize_or_zero(point);
  const hyperverse::Vec3 light = normalize_or_zero({.x = -0.35F, .y = -0.45F, .z = 0.82F});
  const hyperverse::OreTint blended_tint{
    .r = 1.0F + ((tint.r - 1.0F) * tint_blend),
    .g = 1.0F + ((tint.g - 1.0F) * tint_blend),
    .b = 1.0F + ((tint.b - 1.0F) * tint_blend),
  };
  const float face_lit = std::max(0.0F, std::abs(dot(normal, light)));
  const float soft_lit = std::max(0.0F, dot(normalize_or_zero(rotated_point), light));
  const float depth_shade = std::clamp(0.92F + (rotated_point.z / std::max(radius, 1.0F)) * 0.12F, 0.78F, 1.06F);
  const float height_shade = std::clamp(0.84F + (std::sqrt(dot(point, point)) / std::max(radius, 1.0F)) * 0.34F, 0.72F, 1.08F);
  const float freckle = surface_noise((radial * 8.0F) + hyperverse::Vec3{.x = 0.31F, .y = 0.67F, .z = 0.19F});
  const float shade = (0.32F + (face_lit * 0.30F) + (soft_lit * 0.20F)) * depth_shade * height_shade;
  const float mineral = 0.88F + (freckle * 0.20F);
  return {
    .x_ndc = center.x + ((rotated_point.x * hyperverse::PixelsPerWorldUnit * 2.0F) / static_cast<float>(width)),
    .y_ndc = center.y + ((rotated_point.y * hyperverse::PixelsPerWorldUnit * 2.0F) / static_cast<float>(height)),
    .u = (radial.x * 0.52F) + (radial.z * 0.27F) + (point.x / std::max(radius, 1.0F)),
    .v = (radial.y * 0.52F) - (radial.z * 0.23F) + (point.y / std::max(radius, 1.0F)),
    .r = std::clamp(blended_tint.r * heat_r * shade * mineral, 0.04F, 1.0F),
    .g = std::clamp(blended_tint.g * heat_g * shade * (0.92F + (freckle * 0.16F)), 0.04F, 1.0F),
    .b = std::clamp(blended_tint.b * heat_b * shade * (0.86F + (freckle * 0.18F)), 0.04F, 1.0F),
  };
}

[[nodiscard]] hyperverse::Vec3 rotate_mesh_point(hyperverse::Vec3 point, hyperverse::Vec3 angles) {
  const float cx = std::cos(angles.x);
  const float sx = std::sin(angles.x);
  const float cy = std::cos(angles.y);
  const float sy = std::sin(angles.y);
  const float cz = std::cos(angles.z);
  const float sz = std::sin(angles.z);

  hyperverse::Vec3 rotated{
    .x = point.x,
    .y = (point.y * cx) - (point.z * sx),
    .z = (point.y * sx) + (point.z * cx),
  };
  rotated = {
    .x = (rotated.x * cy) + (rotated.z * sy),
    .y = rotated.y,
    .z = (-rotated.x * sy) + (rotated.z * cy),
  };
  return {
    .x = (rotated.x * cz) - (rotated.y * sz),
    .y = (rotated.x * sz) + (rotated.y * cz),
    .z = rotated.z,
  };
}

void add_asteroid_mesh(
  std::vector<hyperverse::TriangleDraw>& triangles,
  const hyperverse::AsteroidBody& asteroid,
  const hyperverse::AsteroidGeometry& geometry,
  const hyperverse::OreTint tint,
  hyperverse::Vec2 camera_position,
  const hyperverse::SectorTuning& sector,
  std::uint32_t width,
  std::uint32_t height,
  float heat_r = 1.0F,
  float heat_g = 1.0F,
  float heat_b = 1.0F
) {
  const hyperverse::Vec2 center = world_to_ndc(asteroid.position, camera_position, sector, width, height);
  std::vector<hyperverse::Vec3> rotated_vertices;
  rotated_vertices.reserve(geometry.vertices.size());
  for (const hyperverse::AsteroidMeshVertex& vertex : geometry.vertices) {
    rotated_vertices.push_back(rotate_mesh_point(vertex.position, geometry.tumble_angles));
  }

  struct PendingTriangle {
    float depth{};
    hyperverse::TriangleDraw draw{};
  };
  std::vector<PendingTriangle> pending;
  pending.reserve(geometry.triangles.size());

  const hyperverse::Vec3 light = normalize_or_zero({.x = -0.35F, .y = -0.45F, .z = 0.82F});
  for (const hyperverse::AsteroidMeshTriangle& triangle : geometry.triangles) {
    const hyperverse::Vec3 a = rotated_vertices[triangle.a];
    const hyperverse::Vec3 b = rotated_vertices[triangle.b];
    const hyperverse::Vec3 c = rotated_vertices[triangle.c];
    const hyperverse::Vec3 normal = normalize_or_zero(cross(b - a, c - a));
    const float lit = std::max(0.0F, std::abs(dot(normal, light)));
    const float depth = (a.z + b.z + c.z) / 3.0F;
    const float depth_shade = std::clamp(0.92F + (depth / std::max(asteroid.radius, 1.0F)) * 0.12F, 0.78F, 1.06F);
    const float shade = 0.92F + (lit * 0.08F) + ((depth_shade - 1.0F) * 0.25F);
    const hyperverse::AsteroidMeshVertex& av = geometry.vertices[triangle.a];
    const hyperverse::AsteroidMeshVertex& bv = geometry.vertices[triangle.b];
    const hyperverse::AsteroidMeshVertex& cv = geometry.vertices[triangle.c];
    hyperverse::TriangleVertexDraw draw_a = asteroid_triangle_vertex(av.position, a, normal, center, tint, width, height, asteroid.radius, heat_r, heat_g, heat_b, av.tint_blend);
    hyperverse::TriangleVertexDraw draw_b = asteroid_triangle_vertex(bv.position, b, normal, center, tint, width, height, asteroid.radius, heat_r, heat_g, heat_b, bv.tint_blend);
    hyperverse::TriangleVertexDraw draw_c = asteroid_triangle_vertex(cv.position, c, normal, center, tint, width, height, asteroid.radius, heat_r, heat_g, heat_b, cv.tint_blend);
    draw_a.r *= av.r * shade;
    draw_a.g *= av.g * shade;
    draw_a.b *= av.b * shade;
    draw_b.r *= bv.r * shade;
    draw_b.g *= bv.g * shade;
    draw_b.b *= bv.b * shade;
    draw_c.r *= cv.r * shade;
    draw_c.g *= cv.g * shade;
    draw_c.b *= cv.b * shade;
    pending.push_back(
      PendingTriangle{
        .depth = depth,
        .draw = {
          .a = draw_a,
          .b = draw_b,
          .c = draw_c,
        },
      }
    );
  }

  std::ranges::sort(pending, [](const PendingTriangle& lhs, const PendingTriangle& rhs) {
    return lhs.depth < rhs.depth;
  });
  for (const PendingTriangle& triangle : pending) {
    triangles.push_back(triangle.draw);
  }
}

[[nodiscard]] bool sprite_overlaps_screen(const hyperverse::SpriteDraw& sprite) {
  return sprite.center_x_ndc + sprite.half_width_ndc >= -1.0F && sprite.center_x_ndc - sprite.half_width_ndc <= 1.0F &&
         sprite.center_y_ndc + sprite.half_height_ndc >= -1.0F && sprite.center_y_ndc - sprite.half_height_ndc <= 1.0F;
}

[[nodiscard]] float ship_sprite_rotation(float facing_radians) {
  return facing_radians + (std::numbers::pi_v<float> * 0.5F);
}

[[nodiscard]] hyperverse::SpriteDraw scaled_bounds(hyperverse::SpriteDraw bounds, float scale) {
  bounds.half_width_ndc *= scale;
  bounds.half_height_ndc *= scale;
  return bounds;
}

void tint_sprite(hyperverse::SpriteDraw& sprite, float r, float g, float b, float a = 1.0F) {
  sprite.tint_r = r;
  sprite.tint_g = g;
  sprite.tint_b = b;
  sprite.tint_a = a;
}

[[nodiscard]] hyperverse::SpriteTexture glyph_texture(char character) {
  if (character >= 'A' && character <= 'Z') {
    return static_cast<hyperverse::SpriteTexture>(
      static_cast<int>(hyperverse::SpriteTexture::GlyphA) + static_cast<int>(character - 'A')
    );
  }
  return static_cast<hyperverse::SpriteTexture>(
    static_cast<int>(hyperverse::SpriteTexture::Glyph0) + static_cast<int>(character - '0')
  );
}

void add_hud_text(
  std::vector<hyperverse::SpriteDraw>& sprites,
  std::string_view text,
  float left_ndc,
  float top_ndc,
  float glyph_height_ndc,
  float r = 0.64F,
  float g = 0.95F,
  float b = 1.0F
) {
  const float glyph_width_ndc = glyph_height_ndc * 0.5F;
  const float advance = glyph_width_ndc * 1.12F;
  float x = left_ndc;

  for (char character : text) {
    if (character == ' ') {
      x += advance;
      continue;
    }
    if (character < '0' || (character > '9' && character < 'A') || character > 'Z') {
      x += advance;
      continue;
    }

    sprites.push_back({
      .texture = glyph_texture(character),
      .center_x_ndc = x + (glyph_width_ndc * 0.5F),
      .center_y_ndc = top_ndc - (glyph_height_ndc * 0.5F),
      .half_width_ndc = glyph_width_ndc * 0.5F,
      .half_height_ndc = glyph_height_ndc * 0.5F,
      .tint_r = r,
      .tint_g = g,
      .tint_b = b,
    });
    x += advance;
  }
}

void add_target_bracket_lines(
  std::vector<hyperverse::LineDraw>& lines,
  const hyperverse::SpriteDraw& bounds,
  float r,
  float g,
  float b
) {
  const float left = bounds.center_x_ndc - bounds.half_width_ndc;
  const float right = bounds.center_x_ndc + bounds.half_width_ndc;
  const float bottom = bounds.center_y_ndc - bounds.half_height_ndc;
  const float top = bounds.center_y_ndc + bounds.half_height_ndc;
  const float horizontal = bounds.half_width_ndc * 0.38F;
  const float vertical = bounds.half_height_ndc * 0.38F;
  const auto add_line = [&](float x0, float y0, float x1, float y1) {
    lines.push_back({.start_x_ndc = x0, .start_y_ndc = y0, .end_x_ndc = x1, .end_y_ndc = y1, .r = r, .g = g, .b = b});
  };

  add_line(left, top, left + horizontal, top);
  add_line(left, top, left, top - vertical);
  add_line(right, top, right - horizontal, top);
  add_line(right, top, right, top - vertical);
  add_line(left, bottom, left + horizontal, bottom);
  add_line(left, bottom, left, bottom + vertical);
  add_line(right, bottom, right - horizontal, bottom);
  add_line(right, bottom, right, bottom + vertical);
}

void add_box_lines(std::vector<hyperverse::LineDraw>& lines, const hyperverse::SpriteDraw& bounds, float r, float g, float b, float a = 1.0F) {
  const float left = bounds.center_x_ndc - bounds.half_width_ndc;
  const float right = bounds.center_x_ndc + bounds.half_width_ndc;
  const float bottom = bounds.center_y_ndc - bounds.half_height_ndc;
  const float top = bounds.center_y_ndc + bounds.half_height_ndc;
  const auto add_line = [&](float x0, float y0, float x1, float y1) {
    lines.push_back({.start_x_ndc = x0, .start_y_ndc = y0, .end_x_ndc = x1, .end_y_ndc = y1, .r = r, .g = g, .b = b, .a = a});
  };

  add_line(left, top, right, top);
  add_line(right, top, right, bottom);
  add_line(right, bottom, left, bottom);
  add_line(left, bottom, left, top);
}

void add_asteroid_radar_marker(std::vector<hyperverse::LineDraw>& lines, float center_x, float center_y, float half_size, float r, float g, float b) {
  lines.push_back({
    .start_x_ndc = center_x - half_size,
    .start_y_ndc = center_y,
    .end_x_ndc = center_x + half_size,
    .end_y_ndc = center_y,
    .r = r,
    .g = g,
    .b = b,
    .a = 0.86F,
  });
  lines.push_back({
    .start_x_ndc = center_x,
    .start_y_ndc = center_y - half_size,
    .end_x_ndc = center_x,
    .end_y_ndc = center_y + half_size,
    .r = r,
    .g = g,
    .b = b,
    .a = 0.86F,
  });
}

void add_asteroid_radar_panel(
  std::vector<hyperverse::LineDraw>& lines,
  hyperverse::Vec2 player_position,
  entt::registry& registry,
  const hyperverse::SectorTuning& sector
) {
  const hyperverse::SpriteDraw panel{
    .center_x_ndc = AsteroidRadarCenterX,
    .center_y_ndc = AsteroidRadarCenterY,
    .half_width_ndc = AsteroidRadarHalfSize,
    .half_height_ndc = AsteroidRadarHalfSize,
  };
  add_box_lines(lines, panel, 0.22F, 0.38F, 0.46F, 0.72F);
  lines.push_back({
    .start_x_ndc = AsteroidRadarCenterX - AsteroidRadarHalfSize,
    .start_y_ndc = AsteroidRadarCenterY,
    .end_x_ndc = AsteroidRadarCenterX + AsteroidRadarHalfSize,
    .end_y_ndc = AsteroidRadarCenterY,
    .r = 0.15F,
    .g = 0.22F,
    .b = 0.28F,
    .a = 0.38F,
  });
  lines.push_back({
    .start_x_ndc = AsteroidRadarCenterX,
    .start_y_ndc = AsteroidRadarCenterY - AsteroidRadarHalfSize,
    .end_x_ndc = AsteroidRadarCenterX,
    .end_y_ndc = AsteroidRadarCenterY + AsteroidRadarHalfSize,
    .r = 0.15F,
    .g = 0.22F,
    .b = 0.28F,
    .a = 0.38F,
  });
  add_asteroid_radar_marker(lines, AsteroidRadarCenterX, AsteroidRadarCenterY, 0.008F, 0.35F, 0.92F, 1.0F);

  const float half_width = std::max(sector.width * 0.5F, 1.0F);
  const float half_height = std::max(sector.height * 0.5F, 1.0F);
  for (auto [entity, asteroid] : registry.view<hyperverse::AsteroidBody>().each()) {
    if (asteroid.radius < AsteroidRadarMinimumRadius) {
      continue;
    }
    const hyperverse::Vec2 delta = hyperverse::wrapped_delta(player_position, asteroid.position, sector);
    const float size = std::clamp(asteroid.radius / 36000.0F, 0.006F, 0.024F);
    const float x = std::clamp(
      AsteroidRadarCenterX + std::clamp(delta.x / half_width, -1.0F, 1.0F) * AsteroidRadarHalfSize,
      AsteroidRadarCenterX - AsteroidRadarHalfSize + size,
      AsteroidRadarCenterX + AsteroidRadarHalfSize - size
    );
    const float y = std::clamp(
      AsteroidRadarCenterY + std::clamp(delta.y / half_height, -1.0F, 1.0F) * AsteroidRadarHalfSize,
      AsteroidRadarCenterY - AsteroidRadarHalfSize + size,
      AsteroidRadarCenterY + AsteroidRadarHalfSize - size
    );
    hyperverse::OreTint tint{.r = 0.68F, .g = 0.72F, .b = 0.74F};
    if (const hyperverse::MiningResource* resource = registry.try_get<hyperverse::MiningResource>(entity); resource != nullptr) {
      tint = hyperverse::ore_tint(resource->tier);
    } else if (const hyperverse::MineralComposition* composition = registry.try_get<hyperverse::MineralComposition>(entity); composition != nullptr) {
      tint = hyperverse::ore_tint(*composition);
    }
    add_asteroid_radar_marker(lines, x, y, size, tint.r, tint.g, tint.b);
  }
}

void add_hud_bar(
  std::vector<hyperverse::LineDraw>& lines,
  float left_ndc,
  float y_ndc,
  float width_ndc,
  float fraction,
  float r,
  float g,
  float b
) {
  const float clamped = std::clamp(fraction, 0.0F, 1.0F);
  lines.push_back({.start_x_ndc = left_ndc, .start_y_ndc = y_ndc, .end_x_ndc = left_ndc + width_ndc, .end_y_ndc = y_ndc, .r = 0.16F, .g = 0.20F, .b = 0.24F, .a = 0.88F});
  lines.push_back({.start_x_ndc = left_ndc, .start_y_ndc = y_ndc, .end_x_ndc = left_ndc + (width_ndc * clamped), .end_y_ndc = y_ndc, .r = r, .g = g, .b = b, .a = 1.0F});
}

void add_face_button_legend(std::vector<hyperverse::SpriteDraw>& sprites, const hyperverse::SemanticInputFrame& input) {
  const bool tool_modal = input.tool_intensity > 0.05F;
  const float left = 0.58F;
  float y = -0.955F;
  add_hud_text(sprites, tool_modal ? "RT TOOL" : "FACE", left, y, 0.026F, 0.78F, 0.92F, 1.0F);
  y += 0.034F;
  if (tool_modal) {
    add_hud_text(sprites, "A DRONE", left, y, 0.024F, 0.72F, 1.0F, 0.72F);
    y += 0.03F;
    add_hud_text(sprites, "B BOOST", left, y, 0.024F, 0.72F, 0.92F, 1.0F);
    y += 0.03F;
    add_hud_text(sprites, "X PCANNON", left, y, 0.024F, 0.72F, 0.92F, 1.0F);
    y += 0.03F;
    add_hud_text(sprites, "Y SLING", left, y, 0.024F, 0.95F, 0.82F, 1.0F);
  } else {
    add_hud_text(sprites, "A ESCORT", left, y, 0.024F, 0.72F, 1.0F, 0.72F);
    y += 0.03F;
    add_hud_text(sprites, "B BOOST", left, y, 0.024F, 0.72F, 0.92F, 1.0F);
    y += 0.03F;
    add_hud_text(sprites, "X PCANNON", left, y, 0.024F, 0.72F, 0.92F, 1.0F);
    y += 0.03F;
    add_hud_text(sprites, "Y SLING", left, y, 0.024F, 0.95F, 0.82F, 1.0F);
  }
}

void add_urgency_hud(
  std::vector<hyperverse::SpriteDraw>& sprites,
  std::vector<hyperverse::LineDraw>& lines,
  const hyperverse::RoundTimer& round_timer,
  const hyperverse::SectorPressureHudSnapshot& pressure_hud
) {
  const int round_remaining = static_cast<int>(std::ceil(std::max(0.0F, round_timer.duration_seconds - round_timer.elapsed_seconds)));
  const int next_threat = static_cast<int>(std::ceil(std::max(0.0F, pressure_hud.next_escalation_seconds)));
  add_hud_text(sprites, "ROUND " + std::to_string(round_remaining), -0.20F, -0.955F, 0.034F, 0.92F, 1.0F, 0.72F);
  add_hud_text(
    sprites,
    pressure_hud.universe_tear_open ? "SPACE TEAR OPEN" :
                                      "THREAT " + std::to_string(pressure_hud.escalation_level) + " NEXT " + std::to_string(next_threat),
    -0.26F,
    -0.912F,
    0.03F,
    1.0F,
    0.74F,
    0.32F
  );
  add_hud_bar(lines, -0.26F, -0.872F, 0.52F, pressure_hud.escalation_progress_fraction, 1.0F, 0.48F, 0.2F);
}

void add_composition_line(
  std::vector<hyperverse::SpriteDraw>& sprites,
  std::string_view name,
  float fraction,
  float left_ndc,
  float top_ndc
) {
  if (fraction <= 0.005F) {
    return;
  }
  add_hud_text(
    sprites,
    std::string{name} + " " + std::to_string(static_cast<int>(std::round(fraction * 100.0F))),
    left_ndc,
    top_ndc,
    0.026F,
    0.78F,
    0.95F,
    1.0F
  );
}

void add_target_inspection_panel(
  std::vector<hyperverse::SpriteDraw>& sprites,
  std::vector<hyperverse::LineDraw>& lines,
  const hyperverse::AsteroidMass* mass,
  const hyperverse::MiningResource* resource,
  const hyperverse::MineralComposition* composition
) {
  constexpr float left = -0.43F;
  constexpr float top = -0.62F;
  constexpr float width = 0.86F;
  constexpr float height = 0.35F;
  add_box_lines(
    lines,
    hyperverse::SpriteDraw{
      .center_x_ndc = left + (width * 0.5F),
      .center_y_ndc = top - (height * 0.5F),
      .half_width_ndc = width * 0.5F,
      .half_height_ndc = height * 0.5F,
    },
    0.32F,
    0.88F,
    1.0F,
    0.72F
  );

  const float remaining_mass = mass != nullptr ? mass->remaining_mass : 0.0F;
  const int estimated_mass = static_cast<int>(std::round(remaining_mass));
  add_hud_text(sprites, "TARGET ROCK", left + 0.03F, top - 0.03F, 0.032F, 0.78F, 0.95F, 1.0F);
  add_hud_text(sprites, "MASS " + std::to_string(estimated_mass), left + 0.03F, top - 0.075F, 0.028F, 0.88F, 1.0F, 0.72F);
  if (resource != nullptr) {
    const hyperverse::OreTierProfile profile = ore_tier_profile(resource->tier);
    add_hud_text(sprites, std::string{"CLASS "} + profile.name, left + 0.31F, top - 0.075F, 0.026F, profile.tint.r, profile.tint.g, profile.tint.b);
    add_hud_text(
      sprites,
      "VALUE " + std::to_string(static_cast<int>(std::round(profile.cash_per_mass))) + " CSH",
      left + 0.31F,
      top - 0.108F,
      0.024F,
      profile.tint.r,
      profile.tint.g,
      profile.tint.b
    );
  }

  if (composition == nullptr) {
    return;
  }
  add_composition_line(sprites, "SILICATE", composition->silicate, left + 0.03F, top - 0.142F);
  add_composition_line(sprites, "FERRITE", composition->ferrite, left + 0.03F, top - 0.174F);
  add_composition_line(sprites, "NICKEL", composition->nickel, left + 0.03F, top - 0.206F);
  add_composition_line(sprites, "COBALT", composition->cobalt, left + 0.03F, top - 0.238F);
  add_composition_line(sprites, "IRIDIUM", composition->iridium, left + 0.31F, top - 0.142F);
  add_composition_line(sprites, "EXOTIC CRYSTAL", composition->exotic_crystal, left + 0.31F, top - 0.174F);
  add_composition_line(sprites, "ANOMALOUS MATTER", composition->anomalous_matter, left + 0.31F, top - 0.206F);
}

void add_world_link_line(
  std::vector<hyperverse::LineDraw>& lines,
  hyperverse::Vec2 from,
  hyperverse::Vec2 to,
  hyperverse::Vec2 camera_position,
  const hyperverse::SectorTuning& sector,
  std::uint32_t width,
  std::uint32_t height,
  float r,
  float g,
  float b
) {
  const hyperverse::SpriteDraw from_point =
    make_world_sprite(hyperverse::SpriteTexture::Reticle, from, camera_position, sector, width, height, 1.0F, 1.0F, 0.0F);
  const hyperverse::SpriteDraw to_point =
    make_world_sprite(hyperverse::SpriteTexture::Reticle, to, camera_position, sector, width, height, 1.0F, 1.0F, 0.0F);
  lines.push_back({
    .start_x_ndc = from_point.center_x_ndc,
    .start_y_ndc = from_point.center_y_ndc,
    .end_x_ndc = to_point.center_x_ndc,
    .end_y_ndc = to_point.center_y_ndc,
    .r = r,
    .g = g,
    .b = b,
  });
}

[[nodiscard]] hyperverse::EngineTrailVertexDraw engine_trail_vertex(
  hyperverse::Vec2 world_position,
  hyperverse::Vec2 camera_position,
  const hyperverse::SectorTuning& sector,
  std::uint32_t width,
  std::uint32_t height,
  float normalized_age,
  float intensity,
  float side
) {
  const hyperverse::Vec2 ndc = world_to_ndc(world_position, camera_position, sector, width, height);
  return {
    .x_ndc = ndc.x,
    .y_ndc = ndc.y,
    .normalized_age = normalized_age,
    .intensity = intensity,
    .side = side,
  };
}

void append_engine_trail_triangle(
  std::vector<hyperverse::EngineTrailVertexDraw>& vertices,
  hyperverse::EngineTrailVertexDraw a,
  hyperverse::EngineTrailVertexDraw b,
  hyperverse::EngineTrailVertexDraw c
) {
  vertices.push_back(a);
  vertices.push_back(b);
  vertices.push_back(c);
}

void append_engine_trail_source(
  std::vector<hyperverse::EngineTrailVertexDraw>& vertices,
  const hyperverse::EngineSourceDraw& source,
  hyperverse::Vec2 camera_position,
  const hyperverse::SectorTuning& sector,
  std::uint32_t width,
  std::uint32_t height,
  const hyperverse::EngineTrailTuning& tuning
) {
  const hyperverse::Vec2 direction = hyperverse::normalize_or_zero(source.exhaust_direction);
  if (hyperverse::length(direction) <= 0.0001F) {
    return;
  }
  const hyperverse::Vec2 perpendicular{.x = -direction.y, .y = direction.x};
  const float half_width = tuning.source_width * 0.5F;
  const float half_length = tuning.source_length * 0.5F;
  const hyperverse::Vec2 front = source.position - (direction * half_length);
  const hyperverse::Vec2 rear = source.position + (direction * half_length);
  const hyperverse::EngineTrailVertexDraw a =
    engine_trail_vertex(front + (perpendicular * half_width), camera_position, sector, width, height, -0.2F, source.intensity, -1.0F);
  const hyperverse::EngineTrailVertexDraw b =
    engine_trail_vertex(front - (perpendicular * half_width), camera_position, sector, width, height, -0.2F, source.intensity, 1.0F);
  const hyperverse::EngineTrailVertexDraw c =
    engine_trail_vertex(rear - (perpendicular * half_width), camera_position, sector, width, height, -0.2F, source.intensity, 1.0F);
  const hyperverse::EngineTrailVertexDraw d =
    engine_trail_vertex(rear + (perpendicular * half_width), camera_position, sector, width, height, -0.2F, source.intensity, -1.0F);
  append_engine_trail_triangle(vertices, a, b, c);
  append_engine_trail_triangle(vertices, a, c, d);
}

void append_engine_trail_ribbon(
  std::vector<hyperverse::EngineTrailVertexDraw>& vertices,
  const hyperverse::EngineTrailEngine& engine,
  hyperverse::Vec2 camera_position,
  const hyperverse::SectorTuning& sector,
  std::uint32_t width,
  std::uint32_t height,
  const hyperverse::EngineTrailTuning& tuning
) {
  const std::vector<hyperverse::EngineTrailVertex> ribbon = hyperverse::build_engine_trail_ribbon(engine, sector, tuning);
  if (ribbon.size() < 4U) {
    return;
  }
  for (std::size_t index = 0; index + 3U < ribbon.size(); index += 2U) {
    const hyperverse::EngineTrailVertexDraw a = engine_trail_vertex(
      ribbon[index].position,
      camera_position,
      sector,
      width,
      height,
      ribbon[index].normalized_age,
      ribbon[index].intensity,
      ribbon[index].side
    );
    const hyperverse::EngineTrailVertexDraw b = engine_trail_vertex(
      ribbon[index + 1U].position,
      camera_position,
      sector,
      width,
      height,
      ribbon[index + 1U].normalized_age,
      ribbon[index + 1U].intensity,
      ribbon[index + 1U].side
    );
    const hyperverse::EngineTrailVertexDraw c = engine_trail_vertex(
      ribbon[index + 2U].position,
      camera_position,
      sector,
      width,
      height,
      ribbon[index + 2U].normalized_age,
      ribbon[index + 2U].intensity,
      ribbon[index + 2U].side
    );
    const hyperverse::EngineTrailVertexDraw d = engine_trail_vertex(
      ribbon[index + 3U].position,
      camera_position,
      sector,
      width,
      height,
      ribbon[index + 3U].normalized_age,
      ribbon[index + 3U].intensity,
      ribbon[index + 3U].side
    );
    append_engine_trail_triangle(vertices, a, b, c);
    append_engine_trail_triangle(vertices, b, d, c);
  }
}

void add_world_line(
  std::vector<hyperverse::LineDraw>& lines,
  hyperverse::Vec2 center,
  hyperverse::Vec2 a,
  hyperverse::Vec2 b,
  hyperverse::Vec2 camera_position,
  const hyperverse::SectorTuning& sector,
  std::uint32_t width,
  std::uint32_t height,
  float r,
  float g,
  float blue,
  float alpha = 1.0F
) {
  const hyperverse::SpriteDraw from_point =
    make_world_sprite(hyperverse::SpriteTexture::Reticle, center + a, camera_position, sector, width, height, 1.0F, 1.0F, 0.0F);
  const hyperverse::SpriteDraw to_point =
    make_world_sprite(hyperverse::SpriteTexture::Reticle, center + b, camera_position, sector, width, height, 1.0F, 1.0F, 0.0F);
  lines.push_back({
    .start_x_ndc = from_point.center_x_ndc,
    .start_y_ndc = from_point.center_y_ndc,
    .end_x_ndc = to_point.center_x_ndc,
    .end_y_ndc = to_point.center_y_ndc,
    .r = r,
    .g = g,
    .b = blue,
    .a = alpha,
  });
}

[[nodiscard]] hyperverse::Vec2 rotated_point(float angle, float radius_x, float radius_y) {
  return {.x = std::cos(angle) * radius_x, .y = std::sin(angle) * radius_y};
}

[[nodiscard]] hyperverse::TriangleVertexDraw cargo_vertex(
  hyperverse::Vec2 center_ndc,
  hyperverse::Vec2 offset,
  hyperverse::OreTint tint,
  float shade,
  std::uint32_t width,
  std::uint32_t height,
  float alpha = 0.95F
) {
  return {
    .x_ndc = center_ndc.x + ((offset.x * hyperverse::PixelsPerWorldUnit * 2.0F) / static_cast<float>(width)),
    .y_ndc = center_ndc.y + ((offset.y * hyperverse::PixelsPerWorldUnit * 2.0F) / static_cast<float>(height)),
    .r = std::clamp(tint.r * shade, 0.04F, 1.0F),
    .g = std::clamp(tint.g * shade, 0.04F, 1.0F),
    .b = std::clamp(tint.b * shade, 0.04F, 1.0F),
    .a = alpha,
  };
}

[[nodiscard]] hyperverse::TriangleVertexDraw cargo_legend_vertex(
  hyperverse::Vec2 center_ndc,
  hyperverse::Vec2 offset_ndc,
  hyperverse::OreTint tint,
  float shade,
  float alpha = 0.95F
) {
  return {
    .x_ndc = center_ndc.x + offset_ndc.x,
    .y_ndc = center_ndc.y + offset_ndc.y,
    .r = std::clamp(tint.r * shade, 0.04F, 1.0F),
    .g = std::clamp(tint.g * shade, 0.04F, 1.0F),
    .b = std::clamp(tint.b * shade, 0.04F, 1.0F),
    .a = alpha,
  };
}

void add_hud_line(
  std::vector<hyperverse::LineDraw>& lines,
  hyperverse::Vec2 center_ndc,
  hyperverse::Vec2 a,
  hyperverse::Vec2 b,
  float r,
  float g,
  float blue,
  float alpha = 0.9F
) {
  lines.push_back({
    .start_x_ndc = center_ndc.x + a.x,
    .start_y_ndc = center_ndc.y + a.y,
    .end_x_ndc = center_ndc.x + b.x,
    .end_y_ndc = center_ndc.y + b.y,
    .r = r,
    .g = g,
    .b = blue,
    .a = alpha,
  });
}

void add_hud_cargo_container(
  std::vector<hyperverse::TriangleDraw>& triangles,
  std::vector<hyperverse::LineDraw>& lines,
  hyperverse::Vec2 center_ndc,
  hyperverse::OreTint tint
) {
  std::array<hyperverse::Vec2, 5> top{};
  std::array<hyperverse::Vec2, 5> bottom{};
  for (int index = 0; index < 5; ++index) {
    const float angle = -std::numbers::pi_v<float> * 0.5F + (static_cast<float>(index) / 5.0F) * std::numbers::pi_v<float> * 2.0F;
    const hyperverse::Vec2 top_world = rotated_point(angle, 52.0F, 28.0F) + hyperverse::Vec2{.x = 0.0F, .y = -20.0F};
    const hyperverse::Vec2 bottom_world = rotated_point(angle + 0.32F, 46.0F, 26.0F) + hyperverse::Vec2{.x = 0.0F, .y = 24.0F};
    top[static_cast<std::size_t>(index)] = top_world * 0.00028F;
    bottom[static_cast<std::size_t>(index)] = bottom_world * 0.00028F;
  }

  const hyperverse::Vec2 top_center{.x = 0.0F, .y = -20.0F * 0.00028F};
  const hyperverse::Vec2 bottom_center{.x = 0.0F, .y = 24.0F * 0.00028F};
  for (int index = 0; index < 5; ++index) {
    const int next = (index + 1) % 5;
    const float side_shade = 0.58F + (static_cast<float>(index) * 0.055F);
    triangles.push_back({
      .a = cargo_legend_vertex(center_ndc, top[static_cast<std::size_t>(index)], tint, side_shade),
      .b = cargo_legend_vertex(center_ndc, bottom[static_cast<std::size_t>(index)], tint, side_shade * 0.82F),
      .c = cargo_legend_vertex(center_ndc, bottom[static_cast<std::size_t>(next)], tint, side_shade * 0.72F),
    });
    triangles.push_back({
      .a = cargo_legend_vertex(center_ndc, top[static_cast<std::size_t>(index)], tint, side_shade),
      .b = cargo_legend_vertex(center_ndc, bottom[static_cast<std::size_t>(next)], tint, side_shade * 0.72F),
      .c = cargo_legend_vertex(center_ndc, top[static_cast<std::size_t>(next)], tint, side_shade * 1.08F),
    });
    triangles.push_back({
      .a = cargo_legend_vertex(center_ndc, top_center, tint, 0.92F),
      .b = cargo_legend_vertex(center_ndc, top[static_cast<std::size_t>(index)], tint, 0.84F),
      .c = cargo_legend_vertex(center_ndc, top[static_cast<std::size_t>(next)], tint, 0.98F),
    });
    triangles.push_back({
      .a = cargo_legend_vertex(center_ndc, bottom_center, tint, 0.42F),
      .b = cargo_legend_vertex(center_ndc, bottom[static_cast<std::size_t>(next)], tint, 0.48F),
      .c = cargo_legend_vertex(center_ndc, bottom[static_cast<std::size_t>(index)], tint, 0.36F),
    });
  }

  for (int index = 0; index < 5; ++index) {
    const int next = (index + 1) % 5;
    add_hud_line(lines, center_ndc, top[static_cast<std::size_t>(index)], top[static_cast<std::size_t>(next)], tint.r, tint.g, tint.b);
    add_hud_line(lines, center_ndc, bottom[static_cast<std::size_t>(index)], bottom[static_cast<std::size_t>(next)], tint.r, tint.g, tint.b);
    add_hud_line(lines, center_ndc, top[static_cast<std::size_t>(index)], bottom[static_cast<std::size_t>(index)], tint.r, tint.g, tint.b);
  }
  add_hud_line(lines, center_ndc, {-0.006F, -0.001F}, {0.006F, 0.002F}, 1.0F, 1.0F, 1.0F, 0.48F);
}

void add_cargo_container(
  std::vector<hyperverse::TriangleDraw>& triangles,
  std::vector<hyperverse::LineDraw>& lines,
  hyperverse::Vec2 position,
  hyperverse::OreTint tint,
  hyperverse::Vec2 camera_position,
  const hyperverse::SectorTuning& sector,
  std::uint32_t width,
  std::uint32_t height
) {
  const hyperverse::Vec2 center_ndc = world_to_ndc(position, camera_position, sector, width, height);
  std::array<hyperverse::Vec2, 5> top{};
  std::array<hyperverse::Vec2, 5> bottom{};
  for (int index = 0; index < 5; ++index) {
    const float angle = -std::numbers::pi_v<float> * 0.5F + (static_cast<float>(index) / 5.0F) * std::numbers::pi_v<float> * 2.0F;
    top[static_cast<std::size_t>(index)] = rotated_point(angle, 52.0F, 28.0F) + hyperverse::Vec2{.x = 0.0F, .y = -20.0F};
    bottom[static_cast<std::size_t>(index)] = rotated_point(angle + 0.32F, 46.0F, 26.0F) + hyperverse::Vec2{.x = 0.0F, .y = 24.0F};
  }

  const hyperverse::Vec2 top_center{.x = 0.0F, .y = -20.0F};
  const hyperverse::Vec2 bottom_center{.x = 0.0F, .y = 24.0F};
  for (int index = 0; index < 5; ++index) {
    const int next = (index + 1) % 5;
    const float side_shade = 0.58F + (static_cast<float>(index) * 0.055F);
    triangles.push_back({
      .a = cargo_vertex(center_ndc, top[static_cast<std::size_t>(index)], tint, side_shade, width, height),
      .b = cargo_vertex(center_ndc, bottom[static_cast<std::size_t>(index)], tint, side_shade * 0.82F, width, height),
      .c = cargo_vertex(center_ndc, bottom[static_cast<std::size_t>(next)], tint, side_shade * 0.72F, width, height),
    });
    triangles.push_back({
      .a = cargo_vertex(center_ndc, top[static_cast<std::size_t>(index)], tint, side_shade, width, height),
      .b = cargo_vertex(center_ndc, bottom[static_cast<std::size_t>(next)], tint, side_shade * 0.72F, width, height),
      .c = cargo_vertex(center_ndc, top[static_cast<std::size_t>(next)], tint, side_shade * 1.08F, width, height),
    });
    triangles.push_back({
      .a = cargo_vertex(center_ndc, top_center, tint, 0.92F, width, height),
      .b = cargo_vertex(center_ndc, top[static_cast<std::size_t>(index)], tint, 0.84F, width, height),
      .c = cargo_vertex(center_ndc, top[static_cast<std::size_t>(next)], tint, 0.98F, width, height),
    });
    triangles.push_back({
      .a = cargo_vertex(center_ndc, bottom_center, tint, 0.42F, width, height),
      .b = cargo_vertex(center_ndc, bottom[static_cast<std::size_t>(next)], tint, 0.48F, width, height),
      .c = cargo_vertex(center_ndc, bottom[static_cast<std::size_t>(index)], tint, 0.36F, width, height),
    });
  }

  for (int index = 0; index < 5; ++index) {
    const int next = (index + 1) % 5;
    add_world_line(lines, position, top[static_cast<std::size_t>(index)], top[static_cast<std::size_t>(next)], camera_position, sector, width, height, tint.r, tint.g, tint.b);
    add_world_line(lines, position, bottom[static_cast<std::size_t>(index)], bottom[static_cast<std::size_t>(next)], camera_position, sector, width, height, tint.r, tint.g, tint.b);
    add_world_line(lines, position, top[static_cast<std::size_t>(index)], bottom[static_cast<std::size_t>(index)], camera_position, sector, width, height, tint.r, tint.g, tint.b);
  }

  add_world_line(lines, position, {-22.0F, -4.0F}, {22.0F, 6.0F}, camera_position, sector, width, height, 1.0F, 1.0F, 1.0F, 0.55F);
  add_world_line(lines, position, {-14.0F, 16.0F}, {14.0F, 20.0F}, camera_position, sector, width, height, 1.0F, 1.0F, 1.0F, 0.42F);
}

void add_jump_gate_lines(
  std::vector<hyperverse::LineDraw>& lines,
  hyperverse::Vec2 position,
  float pulse,
  hyperverse::Vec2 camera_position,
  const hyperverse::SectorTuning& sector,
  std::uint32_t width,
  std::uint32_t height
) {
  const float scale = 86.0F + (std::sin(pulse * 4.0F) * 9.0F);
  std::array<hyperverse::Vec2, 5> belt{};
  for (int index = 0; index < 5; ++index) {
    const float angle = -std::numbers::pi_v<float> * 0.5F + (static_cast<float>(index) / 5.0F) * std::numbers::pi_v<float> * 2.0F;
    belt[static_cast<std::size_t>(index)] = rotated_point(angle, scale * 0.64F, scale * 0.46F);
  }
  const hyperverse::Vec2 top{.x = 0.0F, .y = -scale * 0.72F};
  const hyperverse::Vec2 bottom{.x = 0.0F, .y = scale * 0.72F};
  const float glow = 0.68F + (std::sin(pulse * 6.0F) * 0.22F);
  for (int index = 0; index < 5; ++index) {
    const int next = (index + 1) % 5;
    add_world_line(lines, position, belt[static_cast<std::size_t>(index)], belt[static_cast<std::size_t>(next)], camera_position, sector, width, height, 0.35F, 0.9F, 1.0F, glow);
    add_world_line(lines, position, top, belt[static_cast<std::size_t>(index)], camera_position, sector, width, height, 0.55F, 1.0F, 0.86F, glow);
    add_world_line(lines, position, bottom, belt[static_cast<std::size_t>(index)], camera_position, sector, width, height, 0.35F, 0.74F, 1.0F, glow);
  }
  add_world_line(lines, position, {-scale * 0.35F, 0.0F}, {scale * 0.35F, 0.0F}, camera_position, sector, width, height, 0.75F, 1.0F, 1.0F, 0.5F);
}

void add_gathering_edge_indicator(
  std::vector<hyperverse::SpriteDraw>& sprites,
  std::vector<hyperverse::LineDraw>& lines,
  hyperverse::Vec2 ship_position,
  hyperverse::Vec2 gathering_position,
  const hyperverse::SectorTuning& sector,
  std::uint32_t width,
  std::uint32_t height
) {
  const hyperverse::Vec2 delta = hyperverse::wrapped_delta(ship_position, gathering_position, sector);
  const hyperverse::Vec2 ndc_direction{
    .x = (delta.x * hyperverse::PixelsPerWorldUnit * 2.0F) / static_cast<float>(width),
    .y = (delta.y * hyperverse::PixelsPerWorldUnit * 2.0F) / static_cast<float>(height),
  };
  if (hyperverse::length(ndc_direction) <= 0.0001F) {
    return;
  }

  constexpr float edge = 0.92F;
  const float scale_x = std::abs(ndc_direction.x) > 0.0001F ? edge / std::abs(ndc_direction.x) : std::numeric_limits<float>::max();
  const float scale_y = std::abs(ndc_direction.y) > 0.0001F ? edge / std::abs(ndc_direction.y) : std::numeric_limits<float>::max();
  const float scale = std::min(scale_x, scale_y);
  const hyperverse::Vec2 center = ndc_direction * scale;
  hyperverse::SpriteDraw indicator{
    .texture = hyperverse::SpriteTexture::Reticle,
    .center_x_ndc = center.x,
    .center_y_ndc = center.y,
    .half_width_ndc = 0.035F,
    .half_height_ndc = 0.035F,
  };
  add_box_lines(lines, indicator, 0.72F, 1.0F, 0.58F, 0.88F);
  add_hud_text(sprites, "GTH", std::clamp(center.x - 0.04F, -0.96F, 0.86F), std::clamp(center.y - 0.045F, -0.96F, 0.96F), 0.026F, 0.72F, 1.0F, 0.58F);
}

[[nodiscard]] hyperverse::SpriteDraw make_world_sprite(
  hyperverse::SpriteTexture texture,
  hyperverse::Vec2 world_position,
  hyperverse::Vec2 camera_position,
  const hyperverse::SectorTuning& sector,
  std::uint32_t width,
  std::uint32_t height,
  float pixel_size,
  float rotation_radians = 0.0F
) {
  return make_world_sprite(texture, world_position, camera_position, sector, width, height, pixel_size, pixel_size, rotation_radians);
}

[[nodiscard]] float asteroid_sprite_size(const hyperverse::AsteroidBody& asteroid) {
  constexpr float fresh_scale = 0.45F;
  return asteroid.radius * fresh_scale;
}

[[nodiscard]] hyperverse::SpriteDraw make_laser_sprite(
  hyperverse::Vec2 from_world,
  hyperverse::Vec2 to_world,
  hyperverse::Vec2 camera_position,
  const hyperverse::SectorTuning& sector,
  std::uint32_t width,
  std::uint32_t height
) {
  const hyperverse::Vec2 delta = hyperverse::wrapped_delta(from_world, to_world, sector);
  const hyperverse::Vec2 midpoint = hyperverse::wrap_position(from_world + (delta * 0.5F), sector);
  const float pixel_length = hyperverse::length(delta) * hyperverse::PixelsPerWorldUnit;
  return make_world_sprite(
    hyperverse::SpriteTexture::Laser,
    midpoint,
    camera_position,
    sector,
    width,
    height,
    std::max(24.0F, pixel_length),
    18.0F,
    std::atan2(delta.y, delta.x)
  );
}

}  // namespace

namespace hyperverse {

SpriteFrame build_sprite_frame(
  AccountCtx& account,
  entt::entity player,
  const std::vector<entt::entity>& mining_drones,
  entt::entity raider_entity,
  const FlightHudSnapshot& hud,
  const SemanticInputFrame& input,
  const SectorTuning& sector,
  std::uint32_t width,
  std::uint32_t height
) {
  const ShipMotion& ship = account.registry().get<ShipMotion>(player);
  const ShipHealth& ship_health = account.registry().get<ShipHealth>(player);
  const ShipComputer& ship_computer = account.registry().get<ShipComputer>(player);
  const RoundTimer& round_timer = account.registry().get<RoundTimer>(player);
  const CameraState& camera = account.registry().get<CameraState>(player);
  const TargetLockModel& target_lock = account.registry().get<TargetLockModel>(player);
  const GravitySlingHudSnapshot& gravity_sling_hud = account.registry().get<GravitySlingHudSnapshot>(player);
  const MiningHudSnapshot& mining_hud = account.registry().get<MiningHudSnapshot>(player);
  const CargoHudSnapshot& cargo_hud = account.registry().get<CargoHudSnapshot>(player);
  const CargoEscortHudSnapshot& escort_hud = account.registry().get<CargoEscortHudSnapshot>(player);
  const CargoTrainHudSnapshot& train_hud = account.registry().get<CargoTrainHudSnapshot>(player);
  const CargoEscortRouteHudSnapshot& route_hud = account.registry().get<CargoEscortRouteHudSnapshot>(player);
  const SectorPressureHudSnapshot& pressure_hud = account.registry().get<SectorPressureHudSnapshot>(player);
  const MiningDroneHudSnapshot& drone_hud = account.registry().get<MiningDroneHudSnapshot>(player);
  const RadarHudModel& radar_model = account.registry().get<RadarHudModel>(player);
  const ParticleCannonHudSnapshot& particle_hud = account.registry().get<ParticleCannonHudSnapshot>(player);
  const RaiderHudSnapshot& raider_hud = account.registry().get<RaiderHudSnapshot>(player);
  const CargoRecoveryHudSnapshot& recovery_hud = account.registry().get<CargoRecoveryHudSnapshot>(player);
  const CollisionHudSnapshot& collision_hud = account.registry().get<CollisionHudSnapshot>(player);
  const HudNotice& hud_notice = account.registry().get<HudNotice>(player);
  const ExtractionSite* gathering_site = account.registry().try_get<ExtractionSite>(player);

  SpriteFrame frame{
    .state = {
      .speed_fraction = hud.speed_fraction,
      .wrap_warning = hud.wrap_warning,
      .target_locked = has_locked_target(target_lock),
      .mining_active = mining_hud.beam_active,
    },
  };
  add_starfield(frame.stars, camera.position, width, height);

  for (auto [entity, asteroid] : account.registry().view<AsteroidBody>().each()) {
    OreTint tint{.r = 0.82F, .g = 0.78F, .b = 0.70F};
    if (const MiningResource* resource = account.registry().try_get<MiningResource>(entity); resource != nullptr) {
      tint = ore_tint(resource->tier);
    } else if (const MineralComposition* composition = account.registry().try_get<MineralComposition>(entity); composition != nullptr) {
      tint = ore_tint(*composition);
    }
    float heat_r = 1.0F;
    float heat_g = 1.0F;
    float heat_b = 1.0F;
    if (entity == mining_hud.target && mining_hud.blowout) {
      heat_r = 1.8F;
      heat_g = 0.45F;
      heat_b = 0.22F;
    } else if (entity == mining_hud.target && mining_hud.unstable) {
      heat_r = 1.45F;
      heat_g = 0.95F;
      heat_b = 0.35F;
    }
    if (const AsteroidGeometry* geometry = account.registry().try_get<AsteroidGeometry>(entity); geometry != nullptr) {
      add_asteroid_mesh(frame.triangles, asteroid, *geometry, tint, camera.position, sector, width, height, heat_r, heat_g, heat_b);
    } else {
      SpriteDraw asteroid_sprite = make_world_sprite(
        SpriteTexture::Rock,
        asteroid.position,
        camera.position,
        sector,
        width,
        height,
        asteroid_sprite_size(asteroid),
        asteroid.rotation_radians
      );
      tint_sprite(asteroid_sprite, tint.r * heat_r, tint.g * heat_g, tint.b * heat_b);
      frame.sprites.push_back(asteroid_sprite);
    }
  }

  if (has_locked_target(target_lock) && account.registry().valid(target_lock.target)) {
    const AsteroidBody& target = account.registry().get<AsteroidBody>(target_lock.target);
    SpriteDraw reticle = make_world_sprite(SpriteTexture::Reticle, target.position, camera.position, sector, width, height, (target.radius * 0.55F) + 24.0F);
    if (collision_hud.contact) {
      tint_sprite(reticle, 1.0F, 0.20F, 0.15F);
    } else if (collision_hud.warning) {
      tint_sprite(reticle, 1.0F, 0.85F, 0.20F);
    }
    frame.sprites.push_back(reticle);
  }
  if (mining_hud.beam_active) {
    SpriteDraw laser = make_laser_sprite(ship.position, mining_hud.beam_end_position, camera.position, sector, width, height);
    tint_sprite(laser, 1.0F, 0.72F, 0.22F);
    frame.sprites.push_back(laser);
  }
  for (auto [entity, particle] : account.registry().view<ParticleShot>().each()) {
    (void)entity;
    SpriteDraw particle_sprite = make_world_sprite(SpriteTexture::Particle, particle.position, camera.position, sector, width, height, 16.0F);
    if (particle.owner == ProjectileOwner::Raider) {
      tint_sprite(particle_sprite, 1.0F, 0.24F, 0.18F);
    } else {
      tint_sprite(particle_sprite, 0.72F, 0.92F, 1.0F);
    }
    frame.sprites.push_back(particle_sprite);
  }
  const EngineTrailTuning engine_trail_tuning{};
  for (auto [entity, engine_trail] : account.registry().view<EngineTrailModel>().each()) {
    (void)entity;
    for (const EngineTrailEngine& engine : engine_trail.engines) {
      append_engine_trail_ribbon(frame.engine_trails, engine, camera.position, sector, width, height, engine_trail_tuning);
    }
    for (std::size_t index = 0; index < engine_trail.active_sources; ++index) {
      append_engine_trail_source(frame.engine_trails, engine_trail.sources[index], camera.position, sector, width, height, engine_trail_tuning);
    }
  }
  for (entt::entity drone_entity : mining_drones) {
    const MiningDrone& drone = account.registry().get<MiningDrone>(drone_entity);
    SpriteDraw drone_sprite =
      make_world_sprite(SpriteTexture::Drone, drone.position, camera.position, sector, width, height, 28.0F, ship_sprite_rotation(drone.facing_radians));
    if (drone.phase == MiningDronePhase::Mining) {
      tint_sprite(drone_sprite, 0.55F, 1.0F, 0.65F);
    }
    frame.sprites.push_back(drone_sprite);
  }
  (void)raider_entity;
  for (auto [entity, raider] : account.registry().view<RaiderShip>().each()) {
    (void)entity;
    if (raider.integrity <= 0.0F || (raider.role == RaiderRole::CargoThief && !raider_hud.active)) {
      continue;
    }
    SpriteDraw raider_sprite = make_world_sprite(
      SpriteTexture::Drone,
      raider.position,
      camera.position,
      sector,
      width,
      height,
      46.0F,
      ship_sprite_rotation(raider.facing_radians)
    );
    const float cloak_alpha = std::clamp(raider.cloak_fade_seconds / RaiderCloakFadeSeconds, 0.0F, 1.0F);
    if (raider.role == RaiderRole::Combat) {
      tint_sprite(raider_sprite, 0.95F, 0.18F, 0.14F, cloak_alpha);
    } else if (raider.phase == RaiderPhase::Disrupting) {
      tint_sprite(raider_sprite, 0.86F, 0.32F, 0.12F, cloak_alpha);
    } else {
      tint_sprite(raider_sprite, 0.55F, 0.34F, 0.16F, cloak_alpha);
    }
    frame.sprites.push_back(raider_sprite);
  }
  frame.sprites.push_back(make_world_sprite(SpriteTexture::Ship, ship.position, camera.position, sector, width, height, 56.0F, ship_sprite_rotation(ship.facing_radians)));
  if (!hud_notice.message.empty()) {
    add_hud_text(frame.sprites, hud_notice.message, -0.40F, 0.96F, 0.04F, 1.0F, 0.88F, 0.24F);
  }
  if (gathering_site != nullptr) {
    add_gathering_edge_indicator(frame.sprites, frame.lines, ship.position, gathering_site->position, sector, width, height);
  }

  add_urgency_hud(frame.sprites, frame.lines, round_timer, pressure_hud);
  add_asteroid_radar_panel(frame.lines, ship.position, account.registry(), sector);
  add_face_button_legend(frame.sprites, input);
  add_hud_text(frame.sprites, "MINERALS", -0.96F, -0.955F, 0.026F, 0.78F, 0.92F, 1.0F);
  std::array<float, 7> mineral_mass{};
  for (auto [entity, composition, mass] : account.registry().view<MineralComposition, AsteroidMass>().each()) {
    (void)entity;
    mineral_mass[0] += mass.remaining_mass * composition.silicate;
    mineral_mass[1] += mass.remaining_mass * composition.ferrite;
    mineral_mass[2] += mass.remaining_mass * composition.nickel;
    mineral_mass[3] += mass.remaining_mass * composition.cobalt;
    mineral_mass[4] += mass.remaining_mass * composition.iridium;
    mineral_mass[5] += mass.remaining_mass * composition.exotic_crystal;
    mineral_mass[6] += mass.remaining_mass * composition.anomalous_matter;
  }
  float legend_y = -0.922F;
  constexpr std::array<std::string_view, 7> names{
    "SILICATE",
    "FERRITE",
    "NICKEL",
    "COBALT",
    "IRIDIUM",
    "EXOTIC CRYSTAL",
    "ANOMALOUS MATTER",
  };
  constexpr std::array<OreTint, 7> tints{
    OreTint{.r = 0.68F, .g = 0.82F, .b = 0.72F},
    OreTint{.r = 0.92F, .g = 0.66F, .b = 0.50F},
    OreTint{.r = 0.70F, .g = 0.78F, .b = 0.92F},
    OreTint{.r = 0.58F, .g = 0.82F, .b = 1.0F},
    OreTint{.r = 0.94F, .g = 0.78F, .b = 1.0F},
    OreTint{.r = 1.0F, .g = 0.46F, .b = 0.92F},
    OreTint{.r = 0.36F, .g = 1.0F, .b = 0.74F},
  };
  for (std::size_t index = 0; index < mineral_mass.size(); ++index) {
    if (mineral_mass[index] <= 0.5F) {
      continue;
    }
    add_hud_text(
      frame.sprites,
      std::string{names[index]} + " " + std::to_string(static_cast<int>(std::round(mineral_mass[index]))),
      -0.96F,
      legend_y,
      0.021F,
      tints[index].r,
      tints[index].g,
      tints[index].b
    );
    legend_y += 0.027F;
  }
  legend_y += 0.018F;
  add_hud_text(frame.sprites, "CARGO COLORS", -0.96F, legend_y, 0.021F, 0.78F, 0.92F, 1.0F);
  legend_y += 0.032F;
  for (const OreTier tier : {OreTier::Common, OreTier::Industrial, OreTier::Rare, OreTier::Exotic, OreTier::Anomalous}) {
    const OreTint tint = ore_tint(tier);
    add_hud_cargo_container(frame.triangles, frame.lines, {.x = -0.94F, .y = legend_y + 0.006F}, tint);
    add_hud_text(frame.sprites, ore_tier_name(tier), -0.90F, legend_y, 0.019F, tint.r, tint.g, tint.b);
    legend_y += 0.027F;
  }
  add_hud_text(frame.sprites, "SPD " + std::to_string(static_cast<int>(hud.speed)), -0.96F, 0.92F, 0.045F);
  add_hud_text(frame.sprites, "SHD", -0.96F, 0.52F, 0.033F, 0.45F, 0.85F, 1.0F);
  add_hud_bar(frame.lines, -0.86F, 0.497F, 0.28F, ship_health.shields / std::max(ship_health.max_shields, 1.0F), 0.3F, 0.85F, 1.0F);
  add_hud_text(frame.sprites, "ARM", -0.96F, 0.47F, 0.033F, 1.0F, 0.72F, 0.34F);
  add_hud_bar(frame.lines, -0.86F, 0.447F, 0.28F, ship_health.armor / std::max(ship_health.max_armor, 1.0F), 1.0F, 0.58F, 0.24F);
  add_hud_text(frame.sprites, "RND", -0.96F, 0.42F, 0.033F, 0.72F, 1.0F, 0.72F);
  add_hud_bar(frame.lines, -0.86F, 0.397F, 0.28F, round_timer.elapsed_seconds / std::max(round_timer.duration_seconds, 1.0F), 0.72F, 1.0F, 0.72F);
  add_hud_text(frame.sprites, "HUD " + std::to_string(static_cast<int>(ship_computer.hud_effectiveness * 100.0F)), -0.96F, 0.37F, 0.033F, 0.72F, 0.92F, 1.0F);
  add_hud_text(frame.sprites, "POS " + std::to_string(static_cast<int>(ship.position.x)) + " " + std::to_string(static_cast<int>(ship.position.y)), -0.96F, 0.68F, 0.035F);
  add_hud_text(frame.sprites, "ORE " + std::to_string(static_cast<int>(cargo_hud.delivered_mass)), -0.96F, 0.86F, 0.045F, 0.72F, 1.0F, 0.72F);
  add_hud_text(frame.sprites, "BOX " + std::to_string(cargo_hud.cargo_boxes), -0.96F, 0.80F, 0.045F, 0.72F, 1.0F, 0.72F);
  add_hud_text(frame.sprites, "CSH " + std::to_string(static_cast<int>(cargo_hud.cash)), -0.96F, 0.31F, 0.033F, 0.86F, 1.0F, 0.52F);
  add_hud_text(frame.sprites, "SCR " + std::to_string(cargo_hud.score), -0.96F, 0.26F, 0.033F, 0.86F, 1.0F, 0.52F);
  add_hud_text(frame.sprites, "PRS " + std::to_string(pressure_hud.escalation_level), -0.96F, 0.74F, 0.045F, 1.0F, 0.82F, 0.42F);
  add_hud_text(frame.sprites, "PCT " + std::to_string(static_cast<int>(pressure_hud.pressure_fraction * 100.0F)), -0.96F, 0.63F, 0.035F, 1.0F, 0.82F, 0.42F);
  if (has_locked_target(target_lock)) {
    add_hud_text(frame.sprites, "TGT " + std::to_string(static_cast<int>(target_lock.wrapped_distance)), 0.56F, 0.92F, 0.045F);
    add_hud_text(frame.sprites, "SCN " + std::to_string(static_cast<int>(target_lock.scan_confidence * 100.0F)), 0.56F, 0.81F, 0.035F);
  }
  if (mining_hud.beam_active) {
    add_hud_text(frame.sprites, "ZAP", 0.56F, 0.86F, 0.045F, 1.0F, 0.72F, 0.24F);
    add_hud_text(frame.sprites, "INT " + std::to_string(static_cast<int>(mining_hud.target_integrity)), 0.56F, 0.76F, 0.035F, 1.0F, 0.72F, 0.24F);
    add_hud_text(frame.sprites, "HET " + std::to_string(static_cast<int>(mining_hud.target_heat)), 0.56F, 0.72F, 0.035F, 1.0F, 0.72F, 0.24F);
    add_hud_text(frame.sprites, "STR " + std::to_string(static_cast<int>(mining_hud.target_structural_stress)), 0.56F, 0.68F, 0.035F, 1.0F, 0.72F, 0.24F);
  } else if (has_locked_target(target_lock) && mining_hud.target != entt::null) {
    add_hud_text(frame.sprites, mining_hud.target_in_range ? "MINE RDY" : "OUT RNG", 0.56F, 0.76F, 0.035F, 1.0F, 0.72F, 0.24F);
  }
  if (particle_hud.active_particles > 0 || particle_hud.impacts > 0) {
    add_hud_text(frame.sprites, "PCN " + std::to_string(particle_hud.active_particles), 0.56F, 0.68F, 0.045F, 0.72F, 0.92F, 1.0F);
  }
  if (gravity_sling_hud.phase != GravitySlingPhase::FreeFlight) {
    add_hud_text(frame.sprites, "SLG " + std::to_string(static_cast<int>(hyperverse::length(gravity_sling_hud.release_velocity))), 0.56F, 0.63F, 0.04F, 0.95F, 0.82F, 1.0F);
    add_hud_text(
      frame.sprites,
      "RAD " + std::to_string(static_cast<int>(gravity_sling_hud.radius)) + "/" + std::to_string(static_cast<int>(gravity_sling_hud.max_radius)),
      0.56F,
      0.59F,
      0.032F,
      0.95F,
      0.82F,
      1.0F
    );
  } else if (gravity_sling_hud.acquisition_failed) {
    add_hud_text(frame.sprites, "NO SLING", 0.56F, 0.63F, 0.04F, 0.95F, 0.42F, 0.35F);
  }
  if (escort_hud.cargo_train_active) {
    add_hud_text(frame.sprites, "GATE " + std::to_string(static_cast<int>(route_hud.gate_distance)), 0.48F, 0.80F, 0.045F);
    add_hud_text(frame.sprites, "TRN " + std::to_string(static_cast<int>(train_hud.train_length)) + " STR " + std::to_string(static_cast<int>(train_hud.max_coupling_stress * 100.0F)), 0.48F, 0.75F, 0.035F);
  }
  if (raider_hud.active) {
    add_hud_text(frame.sprites, "RAIDER", 0.56F, 0.74F, 0.045F, 1.0F, 0.28F, 0.22F);
    add_hud_text(frame.sprites, "RAID " + std::to_string(static_cast<int>(raider_hud.target_distance)), 0.56F, 0.63F, 0.035F, 1.0F, 0.28F, 0.22F);
  }
  if (drone_hud.target != entt::null) {
    add_hud_text(frame.sprites, "DRN " + std::to_string(static_cast<int>(drone_hud.target_distance)), -0.96F, 0.58F, 0.035F, 0.55F, 1.0F, 0.65F);
  }
  if (recovery_hud.stolen_box_near) {
    add_hud_text(frame.sprites, "RECOVER", 0.52F, 0.68F, 0.045F, 1.0F, 0.34F, 0.25F);
  }
  if (collision_hud.contact) {
    add_hud_text(frame.sprites, "COL " + std::to_string(static_cast<int>(collision_hud.impact_speed)), 0.48F, 0.58F, 0.04F, 1.0F, 0.2F, 0.15F);
  } else if (collision_hud.warning) {
    add_hud_text(frame.sprites, "IMP " + std::to_string(static_cast<int>(collision_hud.time_to_contact_seconds)), 0.48F, 0.58F, 0.04F, 1.0F, 0.85F, 0.2F);
  }

  for (const RadarTrackedTarget& tracked : radar_model.tracked_targets) {
    if (!account.registry().valid(tracked.target) || !account.registry().all_of<AsteroidBody>(tracked.target)) {
      continue;
    }
    const AsteroidBody& asteroid = account.registry().get<AsteroidBody>(tracked.target);
    const SpriteDraw radar_bounds =
      make_world_sprite(SpriteTexture::Reticle, asteroid.position, camera.position, sector, width, height, (asteroid.radius * 0.48F) + 18.0F);
    if (!sprite_overlaps_screen(radar_bounds)) {
      continue;
    }
    const float reveal_fraction = std::clamp(tracked.reveal_seconds / 0.5F, 0.0F, 1.0F);
    const float eased = reveal_fraction * reveal_fraction * (3.0F - (2.0F * reveal_fraction));
    add_box_lines(frame.lines, scaled_bounds(radar_bounds, std::max(0.02F, eased)), 0.22F, 0.86F, 1.0F, 0.42F);
  }
  if (has_locked_target(target_lock) && account.registry().valid(target_lock.target)) {
    const AsteroidBody& target = account.registry().get<AsteroidBody>(target_lock.target);
    add_target_inspection_panel(
      frame.sprites,
      frame.lines,
      account.registry().try_get<AsteroidMass>(target_lock.target),
      account.registry().try_get<MiningResource>(target_lock.target),
      account.registry().try_get<MineralComposition>(target_lock.target)
    );
    const SpriteDraw reticle_bounds =
      make_world_sprite(SpriteTexture::Reticle, target.position, camera.position, sector, width, height, (target.radius * 0.55F) + 24.0F);
    if (collision_hud.contact) {
      add_target_bracket_lines(frame.lines, reticle_bounds, 1.0F, 0.2F, 0.15F);
    } else if (collision_hud.warning) {
      add_target_bracket_lines(frame.lines, reticle_bounds, 1.0F, 0.85F, 0.2F);
    } else {
      add_target_bracket_lines(frame.lines, reticle_bounds, 0.45F, 0.9F, 1.0F);
    }
  }
  if (route_hud.active) {
    add_jump_gate_lines(frame.lines, route_hud.gate_position, round_timer.elapsed_seconds, camera.position, sector, width, height);
    add_world_link_line(frame.lines, ship.position, route_hud.gate_position, camera.position, sector, width, height, 0.2F, 0.55F, 0.85F);
  }
  if (gravity_sling_hud.phase != GravitySlingPhase::FreeFlight && account.registry().valid(gravity_sling_hud.target) &&
      account.registry().all_of<AsteroidBody>(gravity_sling_hud.target)) {
    const AsteroidBody& target = account.registry().get<AsteroidBody>(gravity_sling_hud.target);
    add_world_link_line(frame.lines, ship.position, target.position, camera.position, sector, width, height, 0.95F, 0.82F, 1.0F);
    const float release_strength = std::clamp(length(gravity_sling_hud.release_velocity) * 0.18F, 80.0F, 420.0F);
    add_world_link_line(
      frame.lines,
      ship.position,
      ship.position + (normalize_or_zero(gravity_sling_hud.release_velocity) * release_strength),
      camera.position,
      sector,
      width,
      height,
      0.65F,
      1.0F,
      0.9F
    );
  }
  for (auto [entity, box] : account.registry().view<CargoBox>().each()) {
    (void)entity;
    if (box.state == CargoBoxState::Extracted) {
      continue;
    }
    OreTint tint = ore_tint(box.tier);
    if (escort_hud.cargo_train_active) {
      if (box.state == CargoBoxState::Lost) {
        tint = {.r = 0.55F, .g = 0.1F, .b = 0.1F};
      } else if (box.state == CargoBoxState::Stolen) {
        tint = {.r = 1.0F, .g = 0.2F, .b = 0.15F};
      }
    }
    add_cargo_container(frame.triangles, frame.lines, box.position, tint, camera.position, sector, width, height);
  }
  if (escort_hud.cargo_train_active) {
    std::vector<const CargoBox*> linked_boxes;
    for (auto [entity, box] : account.registry().view<CargoBox>().each()) {
      (void)entity;
      if (box.state == CargoBoxState::Linked) {
        linked_boxes.push_back(&box);
      }
    }
    std::ranges::sort(linked_boxes, [](const CargoBox* lhs, const CargoBox* rhs) { return lhs->index < rhs->index; });

    Vec2 anchor = ship.position;
    for (const CargoBox* box : linked_boxes) {
      add_world_link_line(frame.lines, anchor, box->position, camera.position, sector, width, height, 0.35F, 0.9F, 1.0F);
      anchor = box->position;
    }
  }

  return frame;
}

}  // namespace hyperverse
