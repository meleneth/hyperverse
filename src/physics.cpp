#include "hyperverse/physics.hpp"

#include "hyperverse/asteroid_collision.hpp"
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
#include <limits>
#include <memory>
#include <mutex>
#include <numbers>
#include <thread>
#include <unordered_map>
#include <unordered_set>
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

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
  [[nodiscard]] const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer layer) const override {
    (void)layer;
    return "MOVING";
  }
#endif
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

[[nodiscard]] float jolt_velocity_limit(Vec2 velocity) {
  return std::max(1.0F, length(velocity) + 1.0F);
}

[[nodiscard]] JPH::Quat rotation_z(float radians) {
  return JPH::Quat::sRotation(JPH::Vec3::sAxisZ(), radians);
}

[[nodiscard]] JPH::Vec3 linear_velocity(Vec2 velocity) {
  return JPH::Vec3(velocity.x, velocity.y, 0.0F);
}

[[nodiscard]] JPH::BodyID create_dynamic_sphere(
  JPH::PhysicsSystem& physics_system,
  float radius,
  Vec2 position,
  float rotation_radians,
  Vec2 velocity,
  float angular_velocity = 0.0F
) {
  JPH::BodyCreationSettings settings(
    new JPH::SphereShape(radius),
    JPH::RVec3(position.x, position.y, 0.0F),
    rotation_z(rotation_radians),
    JPH::EMotionType::Dynamic,
    Layers::Moving
  );
  settings.mGravityFactor = 0.0F;
  settings.mLinearDamping = 0.0F;
  settings.mAngularDamping = 0.0F;
  settings.mMaxLinearVelocity = jolt_velocity_limit(velocity);
  settings.mMaxAngularVelocity = std::max(1.0F, std::abs(angular_velocity) + 1.0F);
  settings.mLinearVelocity = linear_velocity(velocity);
  settings.mAngularVelocity = JPH::Vec3(0.0F, 0.0F, angular_velocity);
  return physics_system.GetBodyInterface().CreateAndAddBody(settings, JPH::EActivation::Activate);
}

void destroy_body(JPH::PhysicsSystem& physics_system, JPH::BodyID body_id) {
  if (body_id.IsInvalid()) {
    return;
  }

  JPH::BodyInterface& body_interface = physics_system.GetBodyInterface();
  body_interface.RemoveBody(body_id);
  body_interface.DestroyBody(body_id);
}

void sync_body_motion(
  JPH::PhysicsSystem& physics_system,
  JPH::BodyID body_id,
  Vec2 position,
  float rotation_radians,
  Vec2 velocity,
  float angular_velocity = 0.0F
) {
  JPH::BodyInterface& body_interface = physics_system.GetBodyInterface();
  body_interface.SetMaxLinearVelocity(body_id, jolt_velocity_limit(velocity));
  body_interface.SetMaxAngularVelocity(body_id, std::max(1.0F, std::abs(angular_velocity) + 1.0F));
  body_interface.SetPositionRotationAndVelocity(
    body_id,
    JPH::RVec3(position.x, position.y, 0.0F),
    rotation_z(rotation_radians),
    linear_velocity(velocity),
    JPH::Vec3(0.0F, 0.0F, angular_velocity)
  );
}

struct AsteroidBodyHandle {
  JPH::BodyID body_id{};
  float radius{0.0F};
};

}  // namespace

struct PhysicsWorld::Runtime {
  Runtime()
      : temp_allocator{2 * 1024 * 1024},
        job_system{JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, worker_count()} {
    ship_system.Init(1, 0, 1, 1, broad_phase_layer_interface, object_vs_broadphase_layer_filter, object_layer_pair_filter);
    asteroid_system.Init(
      256,
      0,
      1024,
      1024,
      broad_phase_layer_interface,
      object_vs_broadphase_layer_filter,
      object_layer_pair_filter
    );
  }

  ~Runtime() {
    destroy_body(ship_system, ship_body);
    for (auto& [entity, body] : asteroid_bodies) {
      (void)entity;
      destroy_body(asteroid_system, body.body_id);
    }
  }

  BroadPhaseLayerInterface broad_phase_layer_interface;
  ObjectVsBroadPhaseLayerFilter object_vs_broadphase_layer_filter;
  ObjectLayerPairFilter object_layer_pair_filter;
  JPH::PhysicsSystem ship_system;
  JPH::PhysicsSystem asteroid_system;
  JPH::TempAllocatorImpl temp_allocator;
  JPH::JobSystemThreadPool job_system;
  JPH::BodyID ship_body{};
  std::unordered_map<entt::entity, AsteroidBodyHandle> asteroid_bodies;
};

PhysicsWorld::PhysicsWorld() {
  ensure_jolt_physics_ready();
  runtime_ = std::make_unique<Runtime>();
}

PhysicsWorld::~PhysicsWorld() = default;

void PhysicsWorld::integrate_ship(ShipMotion& ship, const SectorTuning& sector, float dt_seconds) {
  if (runtime_->ship_body.IsInvalid()) {
    runtime_->ship_body = create_dynamic_sphere(runtime_->ship_system, 32.0F, ship.position, ship.facing_radians, ship.velocity);
    runtime_->ship_system.OptimizeBroadPhase();
  } else {
    sync_body_motion(runtime_->ship_system, runtime_->ship_body, ship.position, ship.facing_radians, ship.velocity);
  }

  runtime_->ship_system.Update(dt_seconds, 1, &runtime_->temp_allocator, &runtime_->job_system);

  JPH::BodyInterface& body_interface = runtime_->ship_system.GetBodyInterface();
  const JPH::RVec3 position = body_interface.GetPosition(runtime_->ship_body);
  const JPH::Vec3 velocity = body_interface.GetLinearVelocity(runtime_->ship_body);
  ship.position = wrap_position({.x = static_cast<float>(position.GetX()), .y = static_cast<float>(position.GetY())}, sector);
  ship.velocity = {.x = velocity.GetX(), .y = velocity.GetY()};
}

void PhysicsWorld::integrate_asteroids(entt::registry& registry, const SectorTuning& sector, float dt_seconds) {
  std::vector<entt::entity> entities;
  std::unordered_set<entt::entity> live_entities;
  for (auto [entity, asteroid] : registry.view<AsteroidBody>().each()) {
    (void)asteroid;
    entities.push_back(entity);
    live_entities.insert(entity);
  }

  for (auto iterator = runtime_->asteroid_bodies.begin(); iterator != runtime_->asteroid_bodies.end();) {
    if (live_entities.contains(iterator->first)) {
      ++iterator;
      continue;
    }

    destroy_body(runtime_->asteroid_system, iterator->second.body_id);
    iterator = runtime_->asteroid_bodies.erase(iterator);
  }

  if (entities.empty()) {
    return;
  }

  JPH::BodyInterface& body_interface = runtime_->asteroid_system.GetBodyInterface();
  bool broadphase_dirty = false;

  for (entt::entity entity : entities) {
    AsteroidBody& asteroid = registry.get<AsteroidBody>(entity);
    auto [iterator, inserted] = runtime_->asteroid_bodies.try_emplace(entity);
    AsteroidBodyHandle& body = iterator->second;

    if (inserted || body.body_id.IsInvalid()) {
      const float solid_radius = asteroid_solid_radius(asteroid.radius);
      body.body_id = create_dynamic_sphere(
        runtime_->asteroid_system,
        solid_radius,
        asteroid.position,
        asteroid.rotation_radians,
        asteroid.velocity,
        asteroid.angular_velocity
      );
      body.radius = solid_radius;
      broadphase_dirty = true;
    } else {
      const float solid_radius = asteroid_solid_radius(asteroid.radius);
      if (std::abs(body.radius - solid_radius) > std::numeric_limits<float>::epsilon()) {
        body_interface.SetShape(body.body_id, new JPH::SphereShape(solid_radius), true, JPH::EActivation::Activate);
        body.radius = solid_radius;
        broadphase_dirty = true;
      }
      sync_body_motion(
        runtime_->asteroid_system,
        body.body_id,
        asteroid.position,
        asteroid.rotation_radians,
        asteroid.velocity,
        asteroid.angular_velocity
      );
    }
  }

  if (broadphase_dirty) {
    runtime_->asteroid_system.OptimizeBroadPhase();
  }
  runtime_->asteroid_system.Update(dt_seconds, 1, &runtime_->temp_allocator, &runtime_->job_system);

  constexpr float full_turn = std::numbers::pi_v<float> * 2.0F;
  for (entt::entity entity : entities) {
    AsteroidBody& asteroid = registry.get<AsteroidBody>(entity);
    const JPH::BodyID body_id = runtime_->asteroid_bodies.at(entity).body_id;
    const JPH::RVec3 position = body_interface.GetPosition(body_id);
    const JPH::Vec3 velocity = body_interface.GetLinearVelocity(body_id);
    const JPH::Vec3 angular_velocity = body_interface.GetAngularVelocity(body_id);
    asteroid.position = wrap_position({.x = static_cast<float>(position.GetX()), .y = static_cast<float>(position.GetY())}, sector);
    asteroid.velocity = {.x = velocity.GetX(), .y = velocity.GetY()};
    asteroid.angular_velocity = angular_velocity.GetZ();
    asteroid.rotation_radians = std::fmod(asteroid.rotation_radians + (asteroid.angular_velocity * dt_seconds), full_turn);
  }
}

}  // namespace hyperverse
