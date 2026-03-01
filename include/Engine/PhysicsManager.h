#pragma once

// Jolt core
#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>

// Physics system
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/Shape.h> // JPH::ShapeRefC
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyActivationListener.h>
#include <Jolt/Physics/Body/MotionType.h>

#include "Engine/MathUtils.h"
#include <entt/entt.hpp>

// for cache
#include <unordered_map>

// PhysicsManager handles Jolt initialization, update, and rigidbody creation/removal

namespace Layers {
    // Object layers
    constexpr JPH::ObjectLayer NON_MOVING = 0;
    constexpr JPH::ObjectLayer MOVING     = 1;
    constexpr JPH::ObjectLayer NUM_LAYERS = 2;
}

namespace BroadPhaseLayers {
    // Broadphase layers
    static const JPH::BroadPhaseLayer NON_MOVING(0);
    static const JPH::BroadPhaseLayer MOVING(1);
    static const JPH::uint NUM_LAYERS = 2;
}

// Broad phase layer interface
class BPLayerInterfaceImpl final : public JPH::BroadPhaseLayerInterface {
public:
    BPLayerInterfaceImpl() = default;

    // Map object layer -> broadphase layer
    JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer inLayer) const override {
        switch (inLayer) {
        case Layers::NON_MOVING: return BroadPhaseLayers::NON_MOVING;
        case Layers::MOVING:     return BroadPhaseLayers::MOVING;
        default:                 return BroadPhaseLayers::MOVING;
        }
    }

#if defined(JPH_EXTERNAL_PROFILE) && JPH_EXTERNAL_PROFILE == JPH_EXTERNAL_PROFILE_COUNTERS
    // Optional: return a name for profiling
    const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer inLayer) const override {
        switch (inLayer.GetValue()) {
        case 0: return "NON_MOVING";
        case 1: return "MOVING";
        default: return "UNKNOWN";
        }
    }
#endif

	// Number of broadphase layers (some jolt builds require this)
    virtual uint32_t GetNumBroadPhaseLayers() const override {
        return BroadPhaseLayers::NUM_LAYERS;
    }
};

// Filter: object layer vs broadphase layer
class ObjectVsBroadPhaseLayerFilterImpl final : public JPH::ObjectVsBroadPhaseLayerFilter {
public:
    bool ShouldCollide(JPH::ObjectLayer inLayer1, JPH::BroadPhaseLayer inLayer2) const override {
        // Simple rules:
        // Static (NON_MOVING) should collide with MOVING
        // Dynamic (MOVING) collides with NON_MOVING and MOVING
        if (inLayer1 == Layers::NON_MOVING)
            return inLayer2 == BroadPhaseLayers::MOVING;
        if (inLayer1 == Layers::MOVING)
            return (inLayer2 == BroadPhaseLayers::NON_MOVING) || (inLayer2 == BroadPhaseLayers::MOVING);
        return true;
    }
};

// Filter: object layer vs object layer
class ObjectLayerPairFilterImpl final : public JPH::ObjectLayerPairFilter {
public:
    bool ShouldCollide(JPH::ObjectLayer inLayer1, JPH::ObjectLayer inLayer2) const override {
        // Static hits Dynamic
        // Dynamic hits Dynamic
        // Static ignores Static
        if (inLayer1 == Layers::NON_MOVING && inLayer2 == Layers::NON_MOVING) return false;
        return true;
    }
};

namespace Engine {

// Forward declarations
struct TransformComponent;
struct RigidBodyComponent;
class MeshManager;

class PhysicsManager {
public:
	// Initialize Jolt physics system
    bool Initialize();
	// Shutdown and cleanup
    void Shutdown();
	// Update physics simulation
    void Update(float deltaTime);

	// Access physics system for advanced usage
    JPH::PhysicsSystem* GetSystem() const { return m_physicsSystem; }
	// Access body interface for body manipulation
    JPH::BodyInterface& GetBodyInterface();

    // ECS bridge: create/remove bodies from components
	// Create the base shape first, then apply scaling and create body
    JPH::BodyID CreateRigidBody(const TransformComponent& tc, const RigidBodyComponent& rbc, const MeshManager& meshManager);
    
	// Remove body by ID
    void RemoveRigidBody(JPH::BodyID bodyID);

	// Raycast and return hit entity (or null if no hit)
    entt::entity CastRay(const Engine::Math::Ray& ray, entt::registry& registry);

    // Teleport a body to match the current TransformComponent, and rebuild it if scale changed
    void ResetBodyTransform(const TransformComponent& tc, RigidBodyComponent& rbc, const MeshManager& meshManager);

private:
    // Helper: build the correct Jolt collision shape using RigidBodyComponent + Transform scale
    JPH::ShapeRefC CreatePhysicsShape(const TransformComponent& tc, const RigidBodyComponent& rbc, const MeshManager& meshManager);

private:
	JPH::TempAllocatorImpl* m_tempAllocator = nullptr;                  // for per-frame temporary allocations
	JPH::JobSystemThreadPool* m_jobSystem = nullptr;                    // for multithreading
	JPH::PhysicsSystem* m_physicsSystem = nullptr;                      // main physics system
	BPLayerInterfaceImpl* m_bpLayerInterface = nullptr;                 // broadphase layer interface
	ObjectVsBroadPhaseLayerFilterImpl* m_objVsBpLayerFilter = nullptr;  // object vs broadphase layer filter
	ObjectLayerPairFilterImpl* m_objLayerPairFilter = nullptr;          // object layer pair filter

    // Cache convex hull shapes per meshID to avoid rebuilding each time
    std::unordered_map<int, JPH::ShapeRefC> m_meshShapeCache;
};

}