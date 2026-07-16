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
#include <mutex>
#include <numbers>
#include <thread>
#include <vector>

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

void clear_lock(hyperverse::TargetLockModel& lock) {
  lock.phase = hyperverse::TargetLockPhase::Unlocked;
  lock.target = entt::null;
  lock.relative_position = {};
  lock.relative_velocity = {};
  lock.wrapped_distance = 0.0F;
  lock.closing_speed = 0.0F;
  lock.time_to_contact_seconds = 0.0F;
  lock.scan_confidence = 0.0F;
}

void refresh_lock(
  hyperverse::TargetLockModel& lock,
  const hyperverse::AsteroidBody& target,
  hyperverse::Vec2 observer_position,
  hyperverse::Vec2 observer_velocity,
  const hyperverse::SectorTuning& sector
) {
  lock.relative_position = hyperverse::wrapped_delta(observer_position, target.position, sector);
  lock.relative_velocity = target.velocity - observer_velocity;
  lock.wrapped_distance = hyperverse::length(lock.relative_position);
  const hyperverse::Vec2 target_direction = hyperverse::normalize_or_zero(lock.relative_position);
  lock.closing_speed = std::max(0.0F, -hyperverse::dot(target_direction, lock.relative_velocity));
  const float clearance = std::max(0.0F, lock.wrapped_distance - target.radius);
  lock.time_to_contact_seconds = lock.closing_speed > 0.0001F ? clearance / lock.closing_speed : 0.0F;
  lock.scan_confidence = target.scan_confidence;
}

[[nodiscard]] entt::entity nearest_asteroid(
  entt::registry& registry,
  hyperverse::Vec2 observer_position,
  const hyperverse::SectorTuning& sector,
  float lock_range,
  entt::entity ignored = entt::null
) {
  entt::entity nearest = entt::null;
  float nearest_distance = std::numeric_limits<float>::max();

  for (auto [entity, asteroid] : registry.view<hyperverse::AsteroidBody>().each()) {
    if (entity == ignored) {
      continue;
    }

    const float distance = hyperverse::wrapped_distance(observer_position, asteroid.position, sector);
    if (distance <= lock_range && distance < nearest_distance) {
      nearest = entity;
      nearest_distance = distance;
    }
  }

  return nearest;
}

}  // namespace

namespace hyperverse {

bool has_locked_target(const TargetLockModel& lock) {
  return lock.phase == TargetLockPhase::Locked && lock.target != entt::null;
}

void update_target_lock(
  TargetLockModel& lock,
  entt::registry& registry,
  Vec2 observer_position,
  Vec2 observer_velocity,
  const SemanticInputFrame& input,
  const SectorTuning& sector,
  const TargetingTuning& tuning
) {
  if (input.cancel_requested) {
    clear_lock(lock);
    return;
  }

  if (has_locked_target(lock)) {
    if (!registry.valid(lock.target) || !registry.all_of<AsteroidBody>(lock.target)) {
      clear_lock(lock);
      return;
    }

    if (input.target_cycle_requested) {
      const entt::entity next_target = nearest_asteroid(registry, observer_position, sector, tuning.lock_range, lock.target);
      if (next_target != entt::null) {
        lock.target = next_target;
      }
    }

    refresh_lock(lock, registry.get<AsteroidBody>(lock.target), observer_position, observer_velocity, sector);
    if (lock.wrapped_distance > tuning.release_range) {
      clear_lock(lock);
    }
    return;
  }

  if (!input.target_cycle_requested) {
    return;
  }

  const entt::entity target = nearest_asteroid(registry, observer_position, sector, tuning.lock_range);
  if (target == entt::null) {
    return;
  }

  lock.phase = TargetLockPhase::Locked;
  lock.target = target;
  refresh_lock(lock, registry.get<AsteroidBody>(target), observer_position, observer_velocity, sector);
}

void update_asteroid_motion(entt::registry& registry, const SectorTuning& sector, float dt_seconds) {
  ensure_jolt_physics_ready();

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
  const int worker_count = static_cast<int>(std::max(1U, std::thread::hardware_concurrency()));
  JPH::JobSystemThreadPool job_system(JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, worker_count);
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
