#include "Engine/PhysicsManager.h"
#include <thread>
#include <DirectXMath.h>
#include "Engine/Components.h"
#include "Engine/MeshManager.h"

// Jolt shapes for meshes
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Collision/Shape/ConvexHullShape.h>
#include <Jolt/Physics/Collision/Shape/ScaledShape.h>  // apply runtime scaling

using namespace JPH;

namespace Engine {

bool PhysicsManager::Initialize() {
    // Register allocator and factory/types
    RegisterDefaultAllocator();
    Factory::sInstance = new Factory();
    RegisterTypes();

    // Temp allocator ~10 MB
    const uint32_t tempSize = 10 * 1024 * 1024;
    m_tempAllocator = new TempAllocatorImpl(tempSize);

    // Job system: use hardware_concurrency - 1 worker threads (minimum 1)
	// here we are using the maximum number of cpu cores (at least 1 minumum) for physics minus one to leave one core free for the main thread
    const uint32_t workerThreads = std::max(1u, std::thread::hardware_concurrency() > 0 ? std::thread::hardware_concurrency() - 1 : 1u);
	// create the multi-threaded job system
    m_jobSystem = new JobSystemThreadPool(cMaxPhysicsJobs, cMaxPhysicsBarriers, workerThreads);

    // Layer helpers
    m_bpLayerInterface   = new BPLayerInterfaceImpl();
    m_objVsBpLayerFilter = new ObjectVsBroadPhaseLayerFilterImpl();
    m_objLayerPairFilter = new ObjectLayerPairFilterImpl();

    // Physics system and settings
    m_physicsSystem = new PhysicsSystem();

    // World limits / capacity (tune later as needed)
    const uint32_t maxBodies = 1024;
    const uint32_t numBodyMutexes = 1024;
    const uint32_t maxBodyPairs = 1024;
    const uint32_t maxContactConstraints = 1024;

    // No activation listener for now
    BodyActivationListener* activationListener = nullptr;

    // Settings (default is fine initially)
    PhysicsSettings settings;

    m_physicsSystem->Init(
        maxBodies,
        numBodyMutexes,
        maxBodyPairs,
        maxContactConstraints,
        *m_bpLayerInterface,
        *m_objVsBpLayerFilter,
        *m_objLayerPairFilter
    );

    // Optionally set gravity (default is (0, -9.81f, 0))
    m_physicsSystem->SetGravity(Vec3(0.0f, -9.81f, 0.0f));

    // Activation listener can be set if needed later
    m_physicsSystem->SetBodyActivationListener(activationListener);

    return true;
}


void PhysicsManager::Shutdown() {
    // Clear cached shapes before tearing down Jolt to avoid leaks / asserts
    m_meshShapeCache.clear();

    // Destroy physics system and helpers in reverse order
    if (m_physicsSystem)      { delete m_physicsSystem;      m_physicsSystem = nullptr; }
    if (m_objLayerPairFilter) { delete m_objLayerPairFilter; m_objLayerPairFilter = nullptr; }
    if (m_objVsBpLayerFilter) { delete m_objVsBpLayerFilter; m_objVsBpLayerFilter = nullptr; }
    if (m_bpLayerInterface)   { delete m_bpLayerInterface;   m_bpLayerInterface = nullptr; }
    if (m_jobSystem)          { delete m_jobSystem;          m_jobSystem = nullptr; }
    if (m_tempAllocator)      { delete m_tempAllocator;      m_tempAllocator = nullptr; }

    // Unregister factory/types
    if (Factory::sInstance) {
        delete Factory::sInstance;
        Factory::sInstance = nullptr;
    }
}


void PhysicsManager::Update(float deltaTime) {
    if (!m_physicsSystem) return;

    // One step per frame for now; sub-steps/accumulator later if needed
    const int collisionSteps = 1;
    m_physicsSystem->Update(deltaTime, collisionSteps, m_tempAllocator, m_jobSystem);
}


JPH::BodyInterface& PhysicsManager::GetBodyInterface() {
    return m_physicsSystem->GetBodyInterface();
}


// Helpers to convert DirectX types to Jolt types (local to this TU)
static inline Vec3 ToJolt(const DirectX::XMFLOAT3& v) { return Vec3(v.x, v.y, v.z); }

static inline Quat ToJolt(const DirectX::XMFLOAT4& q) { return Quat(q.x, q.y, q.z, q.w); }


JPH::BodyID PhysicsManager::CreateRigidBody(const TransformComponent& tc, const RigidBodyComponent& rbc, const MeshManager& meshManager) {
    if (!m_physicsSystem) return JPH::BodyID(); // invalid

    // Step 1: Create Base Shape (Local Space / Unscaled)
    JPH::ShapeRefC baseShape;

    switch (rbc.shape) {
    case RBShape::Box: {
        // Use half extents from RigidBodyComponent (no visual scale yet)
        BoxShapeSettings boxSettings(ToJolt(rbc.halfExtent));
        auto res = boxSettings.Create();
        if (res.HasError()) return JPH::BodyID();
        baseShape = res.Get();
        break;
    }
    case RBShape::Sphere: {
        SphereShapeSettings sphereSettings(rbc.radius);
        auto res = sphereSettings.Create();
        if (res.HasError()) return JPH::BodyID();
        baseShape = res.Get();
        break;
    }
    case RBShape::Capsule: {
        // Capsule shape: half-height along Y, plus hemispheres with radius
        // rbc.height is total length; Jolt requires half-height (straight section)
        float halfHeight = rbc.height * 0.5f;
        CapsuleShapeSettings capsuleSettings(halfHeight, rbc.radius);
        auto res = capsuleSettings.Create();
        if (res.HasError()) return JPH::BodyID();
        baseShape = res.Get();
        break;
    }
    case RBShape::Mesh: {
		// Check cache first for existing shape
        auto it = m_meshShapeCache.find(rbc.meshID);

		// if found in cache, reuse shape
        if (it != m_meshShapeCache.end()) {
            baseShape = it->second;
		// else build convex hull from mesh
        } else {
			// Get mesh positions from MeshManager
            const auto& positions = meshManager.GetMeshPositions(rbc.meshID);
            if (positions.empty()) return JPH::BodyID();

            // Build convex hull from raw positions (do not apply tc.scale here yet)
            JPH::Array<JPH::Vec3> hull_vertices;
            hull_vertices.reserve(positions.size());
            for (const auto& p : positions) {
                hull_vertices.push_back(JPH::Vec3(p.x, p.y, p.z));
            }

			// Create convex hull shape
            JPH::ConvexHullShapeSettings hull(hull_vertices);
            auto res = hull.Create();
            if (res.HasError()) return JPH::BodyID();
            baseShape = res.Get();

            // store in cache
            m_meshShapeCache.emplace(rbc.meshID, baseShape);
        }
        break;
    }
    default:
        return JPH::BodyID();
    }

    // Step 2: Apply Transform Scale (visual scaling only)
    const JPH::Vec3 scale = ToJolt(tc.scale);
    JPH::ShapeRefC finalShape;

	// check if the scale is nearly identical to (1,1,1)
    const bool isIdentityScale =
        fabsf(scale.GetX() - 1.0f) < 1e-6f &&
        fabsf(scale.GetY() - 1.0f) < 1e-6f &&
        fabsf(scale.GetZ() - 1.0f) < 1e-6f;

	// if identity, use base shape directly
    if (isIdentityScale) {
        finalShape = baseShape;
    // else wrap in ScaledShape
    } else {
		// Apply scaling via ScaledShapeSettings
        JPH::ScaledShapeSettings scaled(baseShape, scale);
        auto res = scaled.Create();
        if (res.HasError()) return JPH::BodyID();
        finalShape = res.Get();
    }

    // Step 3: Instantiate Body
    BodyCreationSettings creation(
        finalShape,
        ToJolt(tc.position),
        ToJolt(tc.rotation),
        rbc.motionType == RBMotion::Static ? EMotionType::Static : EMotionType::Dynamic,
        rbc.motionType == RBMotion::Static ? Layers::NON_MOVING : Layers::MOVING
    );

    // Physical properties
    creation.mFriction = rbc.friction;
    creation.mRestitution = rbc.restitution;

    // Mass properties (only relevant for dynamic bodies)
    if (rbc.motionType == RBMotion::Dynamic) {
        // Let Jolt compute mass properties from the shape, then override mass
        creation.mOverrideMassProperties = EOverrideMassProperties::CalculateInertia;
        creation.mMassPropertiesOverride.mMass = rbc.mass;

		// inertia tensor here cam also be set here if needed; for now let Jolt compute it
    }

    BodyInterface& bi = m_physicsSystem->GetBodyInterface();

	// Create body
    Body* body = bi.CreateBody(creation);
    if (!body)
        return JPH::BodyID();

    // Add to world and activate
    bi.AddBody(body->GetID(), EActivation::Activate);

    return body->GetID();
}


void PhysicsManager::RemoveRigidBody(JPH::BodyID bodyID) {
    if (!m_physicsSystem || bodyID.IsInvalid()) return;
    BodyInterface& bi = m_physicsSystem->GetBodyInterface();
    bi.RemoveBody(bodyID);
    bi.DestroyBody(bodyID);
}

}