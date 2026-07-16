#include "hyperverse/hud_title.hpp"

#include "hyperverse/version.hpp"

#include <entt/entity/entity.hpp>

#include <iomanip>
#include <sstream>

namespace hyperverse {

std::string make_title(
  const FlightHudSnapshot& hud,
  const CameraState& camera,
  const TargetLockModel& target_lock,
  const MiningHudSnapshot& mining,
  const CargoHudSnapshot& cargo,
  const CargoEscortHudSnapshot& escort,
  const CargoTrainHudSnapshot& train,
  const CargoEscortRouteHudSnapshot& route,
  const SectorPressureHudSnapshot& pressure,
  const MiningDroneHudSnapshot& drone,
  const RaiderHudSnapshot& raider,
  const CargoRecoveryHudSnapshot& recovery,
  const CollisionHudSnapshot& collision
) {
  const char* mapping = hud.control_mapping == ControlMapping::Gamepad ? "gamepad" : "keyboard";
  std::ostringstream title;
  title << application_name() << " " << version() << " | pos " << std::fixed << std::setprecision(0)
        << hud.position.x << "," << hud.position.y << " | speed " << hud.speed << " (" << std::setprecision(0)
        << (hud.speed_fraction * 100.0F) << "%)"
        << " | cam " << camera.position.x << "," << camera.position.y << " | edge " << hud.nearest_wrap_edge_distance;
  if (hud.wrap_warning) {
    title << " WRAP";
  }
  if (has_locked_target(target_lock)) {
    title << " | target " << target_lock.wrapped_distance << " scan " << std::setprecision(0)
          << (target_lock.scan_confidence * 100.0F) << "%"
          << " close " << target_lock.closing_speed;
  } else if (mining.target != entt::null) {
    title << " | mining target";
  }
  if (has_locked_target(target_lock) || mining.target != entt::null) {
    title << " | rock integrity " << mining.target_integrity << " heat " << mining.target_heat << " stress "
          << mining.target_structural_stress << " gas " << mining.target_volatile_pressure << " ore " << mining.extracted_mass;
    if (!mining.target_in_range) {
      title << " OUT OF RANGE";
    }
  } else {
    title << " | target none";
  }
  if (mining.beam_active) {
    title << " | ZAP";
  }
  if (mining.blowout) {
    title << " | BLOWOUT";
  } else if (mining.unstable) {
    title << " | UNSTABLE";
  } else if (mining.gas_venting) {
    title << " | VENTING";
  }
  title << " | cargo " << cargo.delivered_mass << "/" << cargo.required_mass << " boxes " << cargo.cargo_boxes
        << " payout x" << std::setprecision(2) << cargo.payout_multiplier;
  if (escort.cargo_train_active) {
    title << " TRAIN ACTIVE";
    title << " len " << train.train_length << " stress " << train.max_coupling_stress;
    title << " gate " << route.gate_distance;
    if (route.gate_reached) {
      title << " ARRIVED";
    }
  } else if (escort.phase == CargoEscortPhase::Complete) {
    title << " DELIVERED";
  } else if (escort.phase == CargoEscortPhase::Authorized) {
    title << " ESCORT ARMED";
  } else if (cargo.extraction_authorized) {
    title << " EXTRACT";
  }
  title << " | pressure L" << pressure.escalation_level << " " << (pressure.pressure_fraction * 100.0F) << "%"
        << " next " << pressure.next_escalation_seconds << "s";
  if (pressure.escalation_announced) {
    title << " ESCALATION";
  }
  const char* drone_phase = "idle";
  if (drone.phase == MiningDronePhase::Travelling) {
    drone_phase = "travel";
  } else if (drone.phase == MiningDronePhase::Mining) {
    drone_phase = "mine";
  }
  title << " | drone " << drone_phase << " d" << drone.target_distance << " ore " << drone.extracted_mass;
  if (raider.active) {
    const char* raider_phase = "idle";
    if (raider.phase == RaiderPhase::Approaching) {
      raider_phase = "approach";
    } else if (raider.phase == RaiderPhase::Disrupting) {
      raider_phase = "disrupt";
    } else if (raider.phase == RaiderPhase::Towing) {
      raider_phase = "towing";
    } else if (raider.phase == RaiderPhase::Escaped) {
      raider_phase = "escaped";
    }
    title << " | raider " << raider_phase << " d" << raider.target_distance << " hack " << (raider.disruption_fraction * 100.0F) << "%";
    if (raider.phase == RaiderPhase::Towing || raider.phase == RaiderPhase::Escaped) {
      title << " escape " << raider.escape_distance;
    }
  }
  if (recovery.recovered) {
    title << " | CARGO RECOVERED";
  } else if (recovery.stolen_box_near) {
    title << " | RECOVER CARGO " << recovery.nearest_stolen_distance;
  }
  if (collision.contact) {
    title << " | COLLISION " << collision.impact_speed;
  } else if (collision.warning) {
    title << " | IMPACT " << collision.time_to_contact_seconds << "s";
  }
  title << " | " << mapping;
  return title.str();
}

}  // namespace hyperverse
