#include "hyperverse/flight.hpp"

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#endif
#include <Jolt/Jolt.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/Memory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/RegisterTypes.h>
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

#include <algorithm>
#include <cmath>
#include <limits>
#include <mutex>
#include <numbers>
#include <thread>

namespace {

namespace Layers {
constexpr JPH::ObjectLayer Moving = 0;
constexpr JPH::ObjectLayer Count = 1;
}  // namespace Layers

namespace BroadPhaseLayers {
const JPH::BroadPhaseLayer Moving{0};
constexpr JPH::uint Count = 1;
}  // namespace BroadPhaseLayers

class ObjectLayerPairFilter final : public JPH::ObjectLayerPairFilter {
public:
  [[nodiscard]] bool ShouldCollide(JPH::ObjectLayer object1, JPH::ObjectLayer object2) const override {
    return object1 == Layers::Moving && object2 == Layers::Moving;
  }
};

class BroadPhaseLayerInterface final : public JPH::BroadPhaseLayerInterface {
public:
  [[nodiscard]] JPH::uint GetNumBroadPhaseLayers() const override {
    return BroadPhaseLayers::Count;
  }

  [[nodiscard]] JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer layer) const override {
    (void)layer;
    return BroadPhaseLayers::Moving;
  }

  [[nodiscard]] const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer layer) const override {
    (void)layer;
    return "MOVING";
  }
};

class ObjectVsBroadPhaseLayerFilter final : public JPH::ObjectVsBroadPhaseLayerFilter {
public:
  [[nodiscard]] bool ShouldCollide(JPH::ObjectLayer layer, JPH::BroadPhaseLayer broad_phase_layer) const override {
    (void)broad_phase_layer;
    return layer == Layers::Moving;
  }
};

void ensure_jolt_physics_ready() {
  static std::once_flag jolt_registration;
  std::call_once(jolt_registration, [] {
    JPH::RegisterDefaultAllocator();
    if (JPH::Factory::sInstance == nullptr) {
      JPH::Factory::sInstance = new JPH::Factory();
    }
    JPH::RegisterTypes();
  });
}

[[nodiscard]] float shortest_angle_delta(float from, float to) {
  float delta = std::fmod(to - from, std::numbers::pi_v<float> * 2.0F);
  if (delta > std::numbers::pi_v<float>) {
    delta -= std::numbers::pi_v<float> * 2.0F;
  } else if (delta < -std::numbers::pi_v<float>) {
    delta += std::numbers::pi_v<float> * 2.0F;
  }
  return delta;
}

[[nodiscard]] float angle_of(hyperverse::Vec2 value) {
  return std::atan2(value.y, value.x);
}

[[nodiscard]] float nearest_wrap_edge_distance(hyperverse::Vec2 position, const hyperverse::SectorTuning& sector) {
  return std::min({position.x, sector.width - position.x, position.y, sector.height - position.y});
}

void integrate_ship_body_with_jolt(hyperverse::ShipMotion& ship, const hyperverse::SectorTuning& sector, float dt_seconds) {
  ensure_jolt_physics_ready();

  BroadPhaseLayerInterface broad_phase_layer_interface;
  ObjectVsBroadPhaseLayerFilter object_vs_broadphase_layer_filter;
  ObjectLayerPairFilter object_layer_pair_filter;
  JPH::PhysicsSystem physics_system;
  physics_system.Init(1, 0, 1, 1, broad_phase_layer_interface, object_vs_broadphase_layer_filter, object_layer_pair_filter);

  JPH::BodyInterface& body_interface = physics_system.GetBodyInterface();
  JPH::BodyCreationSettings settings(
    new JPH::SphereShape(32.0F),
    JPH::RVec3(ship.position.x, ship.position.y, 0.0F),
    JPH::Quat::sRotation(JPH::Vec3::sAxisZ(), ship.facing_radians),
    JPH::EMotionType::Dynamic,
    Layers::Moving
  );
  settings.mGravityFactor = 0.0F;
  settings.mLinearDamping = 0.0F;
  settings.mAngularDamping = 0.0F;
  settings.mLinearVelocity = JPH::Vec3(ship.velocity.x, ship.velocity.y, 0.0F);

  const JPH::BodyID body_id = body_interface.CreateAndAddBody(settings, JPH::EActivation::Activate);
  JPH::TempAllocatorImpl temp_allocator(512 * 1024);
  const int worker_count = static_cast<int>(std::max(1U, std::thread::hardware_concurrency()));
  JPH::JobSystemThreadPool job_system(JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, worker_count);
  physics_system.Update(dt_seconds, 1, &temp_allocator, &job_system);

  const JPH::RVec3 position = body_interface.GetPosition(body_id);
  const JPH::Vec3 velocity = body_interface.GetLinearVelocity(body_id);
  ship.position = hyperverse::wrap_position({.x = static_cast<float>(position.GetX()), .y = static_cast<float>(position.GetY())}, sector);
  ship.velocity = {.x = velocity.GetX(), .y = velocity.GetY()};
}

}  // namespace

namespace hyperverse {

void simulate_assisted_flight(
  ShipMotion& ship,
  const SemanticInputFrame& input,
  const FlightTuning& flight,
  const SectorTuning& sector,
  float dt_seconds
) {
  const Vec2 desired_velocity = input.desired_movement * flight.max_speed;
  const Vec2 velocity_error = desired_velocity - ship.velocity;
  const float response = length(input.desired_movement) > 0.0F ? flight.acceleration : flight.braking;
  ship.velocity += clamp_length(velocity_error, response * dt_seconds);
  ship.velocity = clamp_length(ship.velocity, flight.max_speed);
  integrate_ship_body_with_jolt(ship, sector, dt_seconds);

  const Vec2 facing_intent = length(input.desired_movement) > 0.0F ? input.desired_movement : input.primary_aim;
  if (length(facing_intent) > 0.0F) {
    const float target_angle = angle_of(facing_intent);
    const float max_turn = flight.turn_rate * dt_seconds;
    const float delta = std::clamp(shortest_angle_delta(ship.facing_radians, target_angle), -max_turn, max_turn);
    ship.facing_radians += delta;
  }
}

FlightHudSnapshot make_flight_hud_snapshot(
  const ShipMotion& ship,
  const SemanticInputFrame& input,
  const FlightTuning& flight,
  const SectorTuning& sector,
  const FlightHudTuning& hud
) {
  const float speed = length(ship.velocity);
  const float max_speed = std::max(flight.max_speed, std::numeric_limits<float>::epsilon());
  const float nearest_edge = nearest_wrap_edge_distance(ship.position, sector);

  return {
    .position = ship.position,
    .velocity = ship.velocity,
    .speed = speed,
    .speed_fraction = std::clamp(speed / max_speed, 0.0F, 1.0F),
    .facing_radians = ship.facing_radians,
    .desired_movement = input.desired_movement,
    .nearest_wrap_edge_distance = nearest_edge,
    .wrap_warning = nearest_edge <= hud.wrap_warning_distance,
    .control_mapping = input.control_mapping,
  };
}

}  // namespace hyperverse
