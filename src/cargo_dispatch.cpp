#include "hyperverse/cargo_dispatch.hpp"

#include <algorithm>

namespace hyperverse {
namespace {

void emit_job_queued(DomainEventBus* event_bus, entt::entity box, Vec2 destination) {
  if (event_bus == nullptr) {
    return;
  }
  event_bus->enqueue(
    DomainEventType::CargoDroneJobQueued,
    DomainEvent{.type = DomainEventType::CargoDroneJobQueued, .subject = box, .position = destination}
  );
}

void emit_job_assigned(DomainEventBus* event_bus, entt::entity drone, entt::entity box, Vec2 destination) {
  if (event_bus == nullptr) {
    return;
  }
  event_bus->enqueue(
    DomainEventType::CargoDroneJobAssigned,
    DomainEvent{.type = DomainEventType::CargoDroneJobAssigned, .subject = drone, .target = box, .position = destination}
  );
}

[[nodiscard]] bool valid_job_box(entt::registry& registry, entt::entity box_entity) {
  if (box_entity == entt::null || !registry.valid(box_entity) || !registry.all_of<CargoBox>(box_entity)) {
    return false;
  }
  const CargoBox& box = registry.get<CargoBox>(box_entity);
  return box.state == CargoBoxState::PendingPickup || box.state == CargoBoxState::BeingHauled;
}

[[nodiscard]] bool drone_available(entt::registry& registry, entt::entity drone_entity) {
  if (drone_entity == entt::null || !registry.valid(drone_entity) || !registry.all_of<MiningDrone>(drone_entity)) {
    return false;
  }
  const MiningDrone& drone = registry.get<MiningDrone>(drone_entity);
  return drone.cargo_target == entt::null && drone.target == entt::null &&
         (drone.phase == MiningDronePhase::Idle || drone.phase == MiningDronePhase::Travelling);
}

void complete_delivered_job(CargoDispatchModel& dispatch, entt::entity box) {
  std::erase_if(dispatch.jobs, [box](const CargoDroneJob& job) { return job.box == box; });
}

}  // namespace

Vec2 cargo_gathering_slot_position(
  const ExtractionSite& gathering_site,
  const CargoBoxTuning& tuning,
  const SectorTuning& sector,
  int box_index
) {
  const int columns = std::max(1, tuning.gathering_columns);
  const int column = box_index % columns;
  const int row = box_index / columns;
  const float centered_column = static_cast<float>(column) - ((static_cast<float>(columns) - 1.0F) * 0.5F);
  return wrap_position(
    {
      .x = gathering_site.position.x + (centered_column * tuning.box_spacing),
      .y = gathering_site.position.y + (static_cast<float>(row) * tuning.box_spacing),
    },
    sector
  );
}

void schedule_cargo_delivery_job(
  CargoDispatchModel& dispatch,
  entt::registry& registry,
  entt::entity box,
  const ExtractionSite& gathering_site,
  const CargoBoxTuning& tuning,
  const SectorTuning& sector,
  DomainEventBus* event_bus
) {
  if (!valid_job_box(registry, box)) {
    return;
  }
  const CargoBox& cargo_box = registry.get<CargoBox>(box);
  const auto existing = std::ranges::find_if(dispatch.jobs, [box](const CargoDroneJob& job) { return job.box == box; });
  if (existing != dispatch.jobs.end()) {
    return;
  }
  const Vec2 destination = cargo_gathering_slot_position(gathering_site, tuning, sector, cargo_box.index);
  dispatch.jobs.push_back(CargoDroneJob{.box = box, .destination = destination});
  emit_job_queued(event_bus, box, destination);
}

int dispatch_cargo_drone_jobs(
  CargoDispatchModel& dispatch,
  entt::registry& registry,
  const std::vector<entt::entity>& drones,
  DomainEventBus* event_bus
) {
  std::erase_if(dispatch.jobs, [&registry](const CargoDroneJob& job) { return !valid_job_box(registry, job.box); });

  int assigned = 0;
  for (CargoDroneJob& job : dispatch.jobs) {
    if (
      job.assigned_drone != entt::null &&
      (!registry.valid(job.assigned_drone) || !registry.all_of<MiningDrone>(job.assigned_drone) ||
       registry.get<MiningDrone>(job.assigned_drone).cargo_target != job.box)
    ) {
      job.assigned_drone = entt::null;
    }
    if (job.assigned_drone != entt::null) {
      continue;
    }
    const auto drone = std::ranges::find_if(drones, [&registry](entt::entity drone_entity) { return drone_available(registry, drone_entity); });
    if (drone == drones.end()) {
      break;
    }

    MiningDrone& assigned_drone = registry.get<MiningDrone>(*drone);
    assigned_drone.target = entt::null;
    assigned_drone.cargo_target = job.box;
    assigned_drone.cargo_destination = job.destination;
    job.assigned_drone = *drone;
    emit_job_assigned(event_bus, *drone, job.box, job.destination);
    ++assigned;
  }
  return assigned;
}

void install_cargo_dispatch_event_handlers(
  CargoDispatchModel& dispatch,
  entt::registry& registry,
  const std::vector<entt::entity>& drones,
  const ExtractionSite& gathering_site,
  const CargoBoxTuning& tuning,
  const SectorTuning& sector,
  DomainEventBus& event_bus
) {
  event_bus.appendListener(DomainEventType::CargoBoxCreated, [&dispatch, &registry, &drones, &gathering_site, &tuning, &sector, &event_bus](const DomainEvent& event) {
    schedule_cargo_delivery_job(dispatch, registry, event.subject, gathering_site, tuning, sector, &event_bus);
    (void)dispatch_cargo_drone_jobs(dispatch, registry, drones, &event_bus);
  });
  event_bus.appendListener(DomainEventType::CargoBoxDeliveredToGathering, [&dispatch, &registry, &drones, &event_bus](const DomainEvent& event) {
    complete_delivered_job(dispatch, event.target);
    (void)dispatch_cargo_drone_jobs(dispatch, registry, drones, &event_bus);
  });
}

}  // namespace hyperverse
