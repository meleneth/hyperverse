#include "hyperverse/sprite_frame_builder.hpp"

#include "hyperverse/camera.hpp"
#include "hyperverse/cargo_box.hpp"
#include "hyperverse/cargo_escort.hpp"
#include "hyperverse/cargo_manifest.hpp"
#include "hyperverse/cargo_route.hpp"
#include "hyperverse/cargo_train.hpp"
#include "hyperverse/collision.hpp"
#include "hyperverse/drone.hpp"
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
#include <numbers>
#include <string>

namespace {

constexpr float PixelsPerWorldUnit = 0.35F;
constexpr float ScreenAnchorYFraction = 0.75F;

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
  const hyperverse::Vec2 relative = hyperverse::wrapped_delta(camera_position, world_position, sector) * PixelsPerWorldUnit;
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

[[nodiscard]] bool sprite_overlaps_screen(const hyperverse::SpriteDraw& sprite) {
  return sprite.center_x_ndc + sprite.half_width_ndc >= -1.0F && sprite.center_x_ndc - sprite.half_width_ndc <= 1.0F &&
         sprite.center_y_ndc + sprite.half_height_ndc >= -1.0F && sprite.center_y_ndc - sprite.half_height_ndc <= 1.0F;
}

[[nodiscard]] const char* ore_tier_code(hyperverse::OreTier tier) {
  switch (tier) {
    case hyperverse::OreTier::Common:
      return "COM";
    case hyperverse::OreTier::Industrial:
      return "IND";
    case hyperverse::OreTier::Rare:
      return "RAR";
    case hyperverse::OreTier::Exotic:
      return "EXO";
    case hyperverse::OreTier::Anomalous:
      return "ANO";
  }
  return "ORE";
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

[[nodiscard]] float asteroid_sprite_size(const hyperverse::AsteroidBody& asteroid, const hyperverse::MiningResource* resource) {
  constexpr float depleted_scale = 0.24F;
  constexpr float fresh_scale = 0.45F;
  if (resource == nullptr) {
    return asteroid.radius * fresh_scale;
  }

  const float integrity_fraction = std::clamp(resource->integrity / 100.0F, 0.0F, 1.0F);
  return asteroid.radius * (depleted_scale + ((fresh_scale - depleted_scale) * integrity_fraction));
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
  const float pixel_length = hyperverse::length(delta) * PixelsPerWorldUnit;
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

  SpriteFrame frame{
    .state = {
      .speed_fraction = hud.speed_fraction,
      .wrap_warning = hud.wrap_warning,
      .target_locked = has_locked_target(target_lock),
      .mining_active = mining_hud.beam_active,
    },
  };

  for (auto [entity, asteroid] : account.registry().view<AsteroidBody>().each()) {
    SpriteDraw asteroid_sprite = make_world_sprite(
      SpriteTexture::Rock,
      asteroid.position,
      camera.position,
      sector,
      width,
      height,
      asteroid_sprite_size(asteroid, account.registry().try_get<MiningResource>(entity)),
      asteroid.rotation_radians
    );
    if (const MineralComposition* composition = account.registry().try_get<MineralComposition>(entity); composition != nullptr) {
      const OreTint tint = ore_tint(*composition);
      tint_sprite(asteroid_sprite, tint.r, tint.g, tint.b);
    } else if (const MiningResource* resource = account.registry().try_get<MiningResource>(entity); resource != nullptr) {
      const OreTint tint = ore_tint(resource->tier);
      tint_sprite(asteroid_sprite, tint.r, tint.g, tint.b);
    }
    if (entity == mining_hud.target && mining_hud.blowout) {
      tint_sprite(asteroid_sprite, 1.0F, 0.24F, 0.12F);
    } else if (entity == mining_hud.target && mining_hud.unstable) {
      tint_sprite(asteroid_sprite, 1.0F, 0.68F, 0.18F);
    }
    frame.sprites.push_back(asteroid_sprite);
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
  for (entt::entity drone_entity : mining_drones) {
    const MiningDrone& drone = account.registry().get<MiningDrone>(drone_entity);
    SpriteDraw drone_sprite =
      make_world_sprite(SpriteTexture::Drone, drone.position, camera.position, sector, width, height, 28.0F, ship_sprite_rotation(drone.facing_radians));
    if (drone.phase == MiningDronePhase::Mining) {
      tint_sprite(drone_sprite, 0.55F, 1.0F, 0.65F);
    }
    frame.sprites.push_back(drone_sprite);
  }
  const RaiderShip& raider = account.registry().get<RaiderShip>(raider_entity);
  if (raider_hud.active) {
    SpriteDraw raider_sprite = make_world_sprite(
      SpriteTexture::Drone,
      raider.position,
      camera.position,
      sector,
      width,
      height,
      46.0F,
      ship_sprite_rotation(std::atan2(raider.velocity.y, raider.velocity.x))
    );
    if (raider.phase == RaiderPhase::Disrupting) {
      tint_sprite(raider_sprite, 0.86F, 0.32F, 0.12F);
    } else {
      tint_sprite(raider_sprite, 0.55F, 0.34F, 0.16F);
    }
    frame.sprites.push_back(raider_sprite);
  }
  frame.sprites.push_back(make_world_sprite(SpriteTexture::Ship, ship.position, camera.position, sector, width, height, 56.0F, ship_sprite_rotation(ship.facing_radians)));
  if (!hud_notice.message.empty()) {
    add_hud_text(frame.sprites, hud_notice.message, -0.40F, 0.96F, 0.04F, 1.0F, 0.88F, 0.24F);
  }
  add_hud_text(frame.sprites, "MIN", -0.96F, 0.985F, 0.028F, 0.78F, 0.92F, 1.0F);
  std::array<bool, OreTierCount> spawned_tiers{};
  for (auto [entity, resource] : account.registry().view<MiningResource>().each()) {
    (void)entity;
    spawned_tiers[static_cast<std::size_t>(resource.tier)] = true;
  }
  float legend_x = -0.86F;
  for (int tier_index = 0; tier_index < OreTierCount; ++tier_index) {
    if (!spawned_tiers[static_cast<std::size_t>(tier_index)]) {
      continue;
    }
    const OreTier tier = static_cast<OreTier>(tier_index);
    const OreTint tint = ore_tint(tier);
    add_hud_text(frame.sprites, ore_tier_code(tier), legend_x, 0.985F, 0.028F, tint.r, tint.g, tint.b);
    legend_x += 0.068F;
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
    const SpriteDraw gate_bounds = make_world_sprite(SpriteTexture::Reticle, route_hud.gate_position, camera.position, sector, width, height, 96.0F);
    if (route_hud.gate_reached) {
      add_box_lines(frame.lines, gate_bounds, 0.65F, 1.0F, 0.45F);
    } else {
      add_box_lines(frame.lines, gate_bounds, 0.35F, 0.9F, 1.0F);
    }
    add_world_link_line(frame.lines, ship.position, route_hud.gate_position, camera.position, sector, width, height, 0.2F, 0.55F, 0.85F);
  }
  for (auto [entity, box] : account.registry().view<CargoBox>().each()) {
    (void)entity;
    const SpriteDraw box_bounds = make_world_sprite(SpriteTexture::Reticle, box.position, camera.position, sector, width, height, 28.0F);
    if (escort_hud.cargo_train_active) {
      if (box.state == CargoBoxState::Lost) {
        add_box_lines(frame.lines, box_bounds, 0.55F, 0.1F, 0.1F);
      } else if (box.state == CargoBoxState::Stolen) {
        add_box_lines(frame.lines, box_bounds, 1.0F, 0.2F, 0.15F);
      } else {
        const OreTint tint = ore_tint(box.tier);
        add_box_lines(frame.lines, box_bounds, tint.r, tint.g, tint.b);
      }
    } else {
      const OreTint tint = ore_tint(box.tier);
      add_box_lines(frame.lines, box_bounds, tint.r, tint.g, tint.b);
    }
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
