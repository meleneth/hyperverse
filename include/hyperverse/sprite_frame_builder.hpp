#pragma once

#include "hyperverse/account_context.hpp"
#include "hyperverse/flight.hpp"
#include "hyperverse/render_frame.hpp"
#include "hyperverse/sector.hpp"

#include <entt/entity/entity.hpp>

#include <cstdint>
#include <vector>

namespace hyperverse {

[[nodiscard]] SpriteFrame build_sprite_frame(
  AccountCtx& account,
  entt::entity player,
  const std::vector<entt::entity>& mining_drones,
  entt::entity raider_entity,
  const FlightHudSnapshot& hud,
  const SemanticInputFrame& input,
  const SectorTuning& sector,
  std::uint32_t width,
  std::uint32_t height
);

}  // namespace hyperverse
