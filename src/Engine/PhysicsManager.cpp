#include "Engine/PhysicsManager.h"
#include <thread>
#include <DirectXMath.h>
#include "Engine/Components.h"
#include "Engine/MeshManager.h"

// Jolt shapes for meshes
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Collision/Shape/ConvexHullShape.h>

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
    const uint32_t workerThreads = std::max(1u, std::thread::hardware_concurrency() > 0 ? std::thread::hardware_concurrency() - 1 : 1u);
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

    // Build shape from component
    RefConst<Shape> shapeRef;

    // Switch on shape type, create settings, then call Create()
    switch (rbc.shape) {
    case RBShape::Box: {
        // Jolt expects half extents vector
        BoxShapeSettings boxSettings(ToJolt(rbc.halfExtent));
        auto res = boxSettings.Create();
        if (res.HasError()) return JPH::BodyID();
        shapeRef = res.Get();
        break;
    }
    case RBShape::Sphere: {
        SphereShapeSettings sphereSettings(rbc.radius);
        auto res = sphereSettings.Create();
        if (res.HasError()) return JPH::BodyID();
        shapeRef = res.Get();
        break;
    }
    case RBShape::Capsule: {
        // Capsule shape: half-height along Y, plus hemispheres with radius
        // rbc.height is total length; Jolt requires half-height (straight section)
        float halfHeight = rbc.height * 0.5f;
        CapsuleShapeSettings capsuleSettings(halfHeight, rbc.radius);
        auto res = capsuleSettings.Create();
        if (res.HasError()) return JPH::BodyID();
        shapeRef = res.Get();
        break;
    }
    case RBShape::Mesh: {
        const auto& positions = meshManager.GetMeshPositions(rbc.meshID);
        if (positions.empty()) return JPH::BodyID();

        // Build both: VertexList for MeshShapeSettings and Array<Vec3> for ConvexHullShapeSettings
        VertexList vertex_list;
        vertex_list.reserve(positions.size());

        JPH::Array<JPH::Vec3> hull_vertices;
        hull_vertices.reserve(positions.size());

        for (const auto& p : positions) {
            vertex_list.push_back(JPH::Float3(p.x, p.y, p.z)); // VertexList element type
            hull_vertices.push_back(JPH::Vec3(p.x, p.y, p.z)); // ConvexHull needs Vec3
        }

        const auto& indices = meshManager.GetMeshIndices(rbc.meshID);

        if (rbc.motionType == RBMotion::Static) {
            if (indices.empty() || (indices.size() % 3) != 0) {
                // Convex hull path must use Array<Vec3>
                JPH::ConvexHullShapeSettings hull(hull_vertices);
                auto res = hull.Create();
                if (res.HasError()) return JPH::BodyID();
                shapeRef = res.Get();
            } else {
                // Indexed triangles for concave static mesh
                IndexedTriangleList tri_list;
                tri_list.reserve(indices.size() / 3);
                for (size_t i = 0; i + 2 < indices.size(); i += 3) {
                    tri_list.push_back(JPH::IndexedTriangle(
                        static_cast<JPH::uint32>(indices[i]),
                        static_cast<JPH::uint32>(indices[i + 1]),
                        static_cast<JPH::uint32>(indices[i + 2]),
                        0 // material index
                    ));
                }

                JPH::MeshShapeSettings meshSettings(vertex_list, tri_list);
                auto res = meshSettings.Create();
                if (res.HasError()) return JPH::BodyID();
                shapeRef = res.Get();
            }
        } else {
            // Dynamic bodies: convex hull using Array<Vec3>
            JPH::ConvexHullShapeSettings hull(hull_vertices);
            auto res = hull.Create();
            if (res.HasError()) return JPH::BodyID();
            shapeRef = res.Get();
        }
        break;
    }
    default:
        return JPH::BodyID();
    }

    BodyCreationSettings creation(
        shapeRef,
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
        // Compute default mass properties:
        // Set mOverrideMassProperties if needed to strictly enforce mass
        creation.mOverrideMassProperties = EOverrideMassProperties::CalculateInertia;
        creation.mMassPropertiesOverride.mMass = rbc.mass;

		// inertia tensor here cam also be set here if needed; for now let Jolt compute it
    }

    BodyInterface& bi = m_physicsSystem->GetBodyInterface();

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