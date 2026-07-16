#include "hyperverse/physics.hpp"

#include "hyperverse/flight.hpp"
#include "hyperverse/targeting.hpp"

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
#include <mutex>
#include <numbers>
#include <thread>
#include <vector>

namespace hyperverse {
namespace {

namespace Layers {
constexpr JPH::ObjectLayer Moving = 0;
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

[[nodiscard]] int worker_count() {
  return static_cast<int>(std::max(1U, std::thread::hardware_concurrency()));
}

}  // namespace

PhysicsWorld::PhysicsWorld() {
  ensure_jolt_physics_ready();
}

void PhysicsWorld::integrate_ship(ShipMotion& ship, const SectorTuning& sector, float dt_seconds) {
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
  JPH::JobSystemThreadPool job_system(JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, worker_count());
  physics_system.Update(dt_seconds, 1, &temp_allocator, &job_system);

  const JPH::RVec3 position = body_interface.GetPosition(body_id);
  const JPH::Vec3 velocity = body_interface.GetLinearVelocity(body_id);
  ship.position = wrap_position({.x = static_cast<float>(position.GetX()), .y = static_cast<float>(position.GetY())}, sector);
  ship.velocity = {.x = velocity.GetX(), .y = velocity.GetY()};
}

void PhysicsWorld::integrate_asteroids(entt::registry& registry, const SectorTuning& sector, float dt_seconds) {
  std::vector<entt::entity> entities;
  for (auto [entity, asteroid] : registry.view<AsteroidBody>().each()) {
    (void)asteroid;
    entities.push_back(entity);
  }
  if (entities.empty()) {
    return;
  }

  BroadPhaseLayerInterface broad_phase_layer_interface;
  ObjectVsBroadPhaseLayerFilter object_vs_broadphase_layer_filter;
  ObjectLayerPairFilter object_layer_pair_filter;
  JPH::PhysicsSystem physics_system;
  physics_system.Init(
    static_cast<JPH::uint>(entities.size()),
    0,
    static_cast<JPH::uint>(entities.size() * entities.size()),
    static_cast<JPH::uint>(entities.size() * entities.size()),
    broad_phase_layer_interface,
    object_vs_broadphase_layer_filter,
    object_layer_pair_filter
  );

  JPH::BodyInterface& body_interface = physics_system.GetBodyInterface();
  std::vector<JPH::BodyID> body_ids;
  body_ids.reserve(entities.size());

  for (entt::entity entity : entities) {
    AsteroidBody& asteroid = registry.get<AsteroidBody>(entity);
    JPH::BodyCreationSettings settings(
      new JPH::SphereShape(asteroid.radius),
      JPH::RVec3(asteroid.position.x, asteroid.position.y, 0.0F),
      JPH::Quat::sRotation(JPH::Vec3::sAxisZ(), asteroid.rotation_radians),
      JPH::EMotionType::Dynamic,
      Layers::Moving
    );
    settings.mGravityFactor = 0.0F;
    settings.mLinearDamping = 0.0F;
    settings.mAngularDamping = 0.0F;
    settings.mLinearVelocity = JPH::Vec3(asteroid.velocity.x, asteroid.velocity.y, 0.0F);
    settings.mAngularVelocity = JPH::Vec3(0.0F, 0.0F, asteroid.angular_velocity);
    body_ids.push_back(body_interface.CreateAndAddBody(settings, JPH::EActivation::Activate));
  }

  JPH::TempAllocatorImpl temp_allocator(2 * 1024 * 1024);
  JPH::JobSystemThreadPool job_system(JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, worker_count());
  physics_system.OptimizeBroadPhase();
  physics_system.Update(dt_seconds, 1, &temp_allocator, &job_system);

  constexpr float full_turn = std::numbers::pi_v<float> * 2.0F;
  for (std::size_t index = 0; index < entities.size(); ++index) {
    AsteroidBody& asteroid = registry.get<AsteroidBody>(entities[index]);
    const JPH::RVec3 position = body_interface.GetPosition(body_ids[index]);
    const JPH::Vec3 velocity = body_interface.GetLinearVelocity(body_ids[index]);
    const JPH::Vec3 angular_velocity = body_interface.GetAngularVelocity(body_ids[index]);
    asteroid.position = wrap_position({.x = static_cast<float>(position.GetX()), .y = static_cast<float>(position.GetY())}, sector);
    asteroid.velocity = {.x = velocity.GetX(), .y = velocity.GetY()};
    asteroid.angular_velocity = angular_velocity.GetZ();
    asteroid.rotation_radians = std::fmod(asteroid.rotation_radians + (asteroid.angular_velocity * dt_seconds), full_turn);
  }
}

}  // namespace hyperverse
