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
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyActivationListener.h>
#include <Jolt/Physics/Body/MotionType.h>

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

// Forward declarations to avoid including full components header here
struct TransformComponent;
struct RigidBodyComponent;

class PhysicsManager {
public:
    bool Initialize();
    void Shutdown();
    void Update(float deltaTime);

    JPH::PhysicsSystem* GetSystem() const { return m_physicsSystem; }
    JPH::BodyInterface& GetBodyInterface();

    // ECS bridge: create/remove bodies from components
    JPH::BodyID CreateRigidBody(const TransformComponent& tc, const RigidBodyComponent& rbc);
    void RemoveRigidBody(JPH::BodyID bodyID);

private:
    JPH::TempAllocatorImpl* m_tempAllocator = nullptr;
    JPH::JobSystemThreadPool* m_jobSystem = nullptr;
    JPH::PhysicsSystem* m_physicsSystem = nullptr;
    BPLayerInterfaceImpl* m_bpLayerInterface = nullptr;
    ObjectVsBroadPhaseLayerFilterImpl* m_objVsBpLayerFilter = nullptr;
    ObjectLayerPairFilterImpl* m_objLayerPairFilter = nullptr;
};

}