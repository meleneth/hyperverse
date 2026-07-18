#pragma once

#include "hyperverse/cargo_box.hpp"
#include "hyperverse/domain_events.hpp"
#include "hyperverse/drone.hpp"

#include <entt/entity/entity.hpp>
#include <entt/entity/registry.hpp>

#include <vector>

namespace hyperverse {

struct CargoDroneJob {
  entt::entity box{entt::null};
  entt::entity assigned_drone{entt::null};
  Vec2 destination{};
};

struct CargoDispatchModel {
  std::vector<CargoDroneJob> jobs{};
};

[[nodiscard]] Vec2 cargo_gathering_slot_position(
  const ExtractionSite& gathering_site,
  const CargoBoxTuning& tuning,
  const SectorTuning& sector,
  int box_index
);

void schedule_cargo_delivery_job(
  CargoDispatchModel& dispatch,
  entt::registry& registry,
  entt::entity box,
  const ExtractionSite& gathering_site,
  const CargoBoxTuning& tuning,
  const SectorTuning& sector,
  DomainEventBus* event_bus = nullptr
);

int dispatch_cargo_drone_jobs(
  CargoDispatchModel& dispatch,
  entt::registry& registry,
  const std::vector<entt::entity>& drones,
  DomainEventBus* event_bus = nullptr
);

void install_cargo_dispatch_event_handlers(
  CargoDispatchModel& dispatch,
  entt::registry& registry,
  const std::vector<entt::entity>& drones,
  const ExtractionSite& gathering_site,
  const CargoBoxTuning& tuning,
  const SectorTuning& sector,
  DomainEventBus& event_bus
);

}  // namespace hyperverse
