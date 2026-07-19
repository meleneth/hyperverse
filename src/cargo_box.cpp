#include "hyperverse/cargo_box.hpp"

#include <boost/sml.hpp>

#include <algorithm>
#include <limits>

namespace hyperverse {
namespace {

namespace sml = boost::sml;

struct pending_pickup {};
struct being_hauled {};
struct linked {};
struct gate_bound {};
struct extracting {};
struct extracted {};
struct detached {};
struct stolen {};
struct lost {};
struct queue_pickup {};
struct start_haul {};
struct link_box {};
struct send_to_gate {};
struct start_extraction {};
struct finish_extraction {};
struct detach_box {};
struct steal_box {};
struct lose_box {};

struct CargoBoxMachine {
  auto operator()() const {
    using namespace sml;
    return make_transition_table(
      *state<linked> + event<queue_pickup> = state<pending_pickup>,
      state<pending_pickup> + event<start_haul> = state<being_hauled>,
      state<pending_pickup> + event<link_box> = state<linked>,
      state<being_hauled> + event<link_box> = state<linked>,
      state<linked> + event<send_to_gate> = state<gate_bound>,
      state<pending_pickup> + event<send_to_gate> = state<gate_bound>,
      state<being_hauled> + event<send_to_gate> = state<gate_bound>,
      state<gate_bound> + event<start_extraction> = state<extracting>,
      state<extracting> + event<send_to_gate> = state<gate_bound>,
      state<extracting> + event<finish_extraction> = state<extracted>,
      state<linked> + event<detach_box> = state<detached>,
      state<linked> + event<steal_box> = state<stolen>,
      state<detached> + event<steal_box> = state<stolen>,
      state<stolen> + event<link_box> = state<linked>,
      state<stolen> + event<lose_box> = state<lost>
    );
  }
};

void replay_cargo_box_state(sml::sm<CargoBoxMachine>& machine, CargoBoxState state) {
  switch (state) {
    case CargoBoxState::Linked:
      return;
    case CargoBoxState::PendingPickup:
      machine.process_event(queue_pickup{});
      return;
    case CargoBoxState::BeingHauled:
      machine.process_event(queue_pickup{});
      machine.process_event(start_haul{});
      return;
    case CargoBoxState::GateBound:
      machine.process_event(send_to_gate{});
      return;
    case CargoBoxState::Extracting:
      machine.process_event(send_to_gate{});
      machine.process_event(start_extraction{});
      return;
    case CargoBoxState::Extracted:
      machine.process_event(send_to_gate{});
      machine.process_event(start_extraction{});
      machine.process_event(finish_extraction{});
      return;
    case CargoBoxState::Detached:
      machine.process_event(detach_box{});
      return;
    case CargoBoxState::Stolen:
      machine.process_event(steal_box{});
      return;
    case CargoBoxState::Lost:
      machine.process_event(steal_box{});
      machine.process_event(lose_box{});
      return;
  }
}

[[nodiscard]] CargoBoxState read_cargo_box_state(const sml::sm<CargoBoxMachine>& machine) {
  if (machine.is(sml::state<pending_pickup>)) {
    return CargoBoxState::PendingPickup;
  }
  if (machine.is(sml::state<being_hauled>)) {
    return CargoBoxState::BeingHauled;
  }
  if (machine.is(sml::state<gate_bound>)) {
    return CargoBoxState::GateBound;
  }
  if (machine.is(sml::state<extracting>)) {
    return CargoBoxState::Extracting;
  }
  if (machine.is(sml::state<extracted>)) {
    return CargoBoxState::Extracted;
  }
  if (machine.is(sml::state<detached>)) {
    return CargoBoxState::Detached;
  }
  if (machine.is(sml::state<stolen>)) {
    return CargoBoxState::Stolen;
  }
  if (machine.is(sml::state<lost>)) {
    return CargoBoxState::Lost;
  }
  return CargoBoxState::Linked;
}

[[nodiscard]] OreTier ore_tier_for_box(const CargoManifest& manifest, int box_index, float box_mass) {
  const float midpoint_mass = (static_cast<float>(box_index) + 0.5F) * box_mass;
  float cumulative_mass = 0.0F;
  for (int tier_index = 0; tier_index < OreTierCount; ++tier_index) {
    cumulative_mass += manifest.delivered_mass_by_tier[static_cast<std::size_t>(tier_index)];
    if (midpoint_mass <= cumulative_mass) {
      return static_cast<OreTier>(tier_index);
    }
  }
  return OreTier::Common;
}

void emit_box_state_changed(DomainEventBus* event_bus, entt::entity box, const CargoBox& cargo_box) {
  if (event_bus == nullptr) {
    return;
  }
  event_bus->enqueue(
    DomainEventType::CargoBoxStateChanged,
    DomainEvent{
      .type = DomainEventType::CargoBoxStateChanged,
      .subject = box,
      .position = cargo_box.position,
      .amount = cargo_box.mass,
      .count = static_cast<int>(cargo_box.state),
    }
  );
}

void emit_box_created(DomainEventBus* event_bus, entt::entity box, const CargoBox& cargo_box) {
  if (event_bus == nullptr) {
    return;
  }
  event_bus->enqueue(
    DomainEventType::CargoBoxCreated,
    DomainEvent{
      .type = DomainEventType::CargoBoxCreated,
      .subject = box,
      .position = cargo_box.position,
      .amount = cargo_box.mass,
      .count = cargo_box.index,
    }
  );
}

[[nodiscard]] Vec2 cargo_slot_position(const ExtractionSite& gathering_site, const CargoBoxTuning& tuning, const SectorTuning& sector, int box_index) {
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

void move_box_toward(CargoBox& box, Vec2 target, const SectorTuning& sector, float dt_seconds, const CargoBoxTuning& tuning) {
  const float scaled_dt = std::max(0.0F, dt_seconds);
  const Vec2 delta = wrapped_delta(box.position, target, sector);
  const float distance = length(delta);
  if (distance <= 0.001F || scaled_dt <= 0.0F) {
    box.velocity = {};
    return;
  }

  const Vec2 desired_velocity = clamp_length(delta * tuning.gathering_follow_rate, tuning.gathering_max_speed);
  const Vec2 step = desired_velocity * scaled_dt;
  if (length(step) >= distance) {
    box.velocity = delta * (1.0F / std::max(scaled_dt, std::numeric_limits<float>::epsilon()));
    box.position = target;
  } else {
    box.velocity = desired_velocity;
    box.position = wrap_position(box.position + step, sector);
  }
}

}  // namespace

bool transition_cargo_box(
  CargoBox& box,
  CargoBoxTransition transition,
  entt::entity box_entity,
  DomainEventBus* event_bus
) {
  sml::sm<CargoBoxMachine> machine;
  replay_cargo_box_state(machine, box.state);
  const CargoBoxState previous = box.state;
  switch (transition) {
    case CargoBoxTransition::QueuePickup:
      machine.process_event(queue_pickup{});
      break;
    case CargoBoxTransition::StartHaul:
      machine.process_event(start_haul{});
      break;
    case CargoBoxTransition::Link:
      machine.process_event(link_box{});
      break;
    case CargoBoxTransition::SendToGate:
      machine.process_event(send_to_gate{});
      break;
    case CargoBoxTransition::StartExtraction:
      machine.process_event(start_extraction{});
      break;
    case CargoBoxTransition::FinishExtraction:
      machine.process_event(finish_extraction{});
      break;
    case CargoBoxTransition::Detach:
      machine.process_event(detach_box{});
      break;
    case CargoBoxTransition::Steal:
      machine.process_event(steal_box{});
      break;
    case CargoBoxTransition::Lose:
      machine.process_event(lose_box{});
      break;
  }
  box.state = read_cargo_box_state(machine);
  if (box.state == previous) {
    return false;
  }
  emit_box_state_changed(event_bus, box_entity, box);
  return true;
}

int sync_cargo_boxes(
  entt::registry& registry,
  const CargoManifest& manifest,
  const ExtractionSite& extraction_site,
  const CargoBoxTuning& tuning,
  Vec2 pickup_origin,
  DomainEventBus* event_bus
) {
  int existing_boxes = 0;
  for (auto [entity, box] : registry.view<CargoBox>().each()) {
    (void)entity;
    box.index = existing_boxes;
    box.mass = tuning.box_mass;
    box.tier = ore_tier_for_box(manifest, existing_boxes, tuning.box_mass);
    ++existing_boxes;
  }

  while (existing_boxes < manifest.cargo_boxes) {
    const bool has_pickup_origin = pickup_origin.x != 0.0F || pickup_origin.y != 0.0F;
    const Vec2 spawn_position = has_pickup_origin ? pickup_origin : Vec2{.x = extraction_site.position.x + (static_cast<float>(existing_boxes) * tuning.box_spacing), .y = extraction_site.position.y};
    const entt::entity box_entity = registry.create();
    CargoBox box{
      .position = spawn_position,
      .mass = tuning.box_mass,
      .index = existing_boxes,
      .tier = ore_tier_for_box(manifest, existing_boxes, tuning.box_mass),
    };
    if (has_pickup_origin) {
      (void)transition_cargo_box(box, CargoBoxTransition::QueuePickup);
    }
    registry.emplace<CargoBox>(
      box_entity,
      box
    );
    emit_box_created(event_bus, box_entity, registry.get<CargoBox>(box_entity));
    ++existing_boxes;
  }

  return existing_boxes;
}

int update_gathered_cargo_boxes(
  entt::registry& registry,
  const ExtractionSite& gathering_site,
  const SectorTuning& sector,
  float dt_seconds,
  const CargoBoxTuning& tuning
) {
  int moved_boxes = 0;
  for (auto [entity, box] : registry.view<CargoBox>().each()) {
    (void)entity;
    if (box.state != CargoBoxState::Linked) {
      continue;
    }
    move_box_toward(box, cargo_slot_position(gathering_site, tuning, sector, box.index), sector, dt_seconds, tuning);
    ++moved_boxes;
  }
  return moved_boxes;
}

}  // namespace hyperverse
