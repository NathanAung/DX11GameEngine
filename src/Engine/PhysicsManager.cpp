#include "Engine/PhysicsManager.h"
#include <thread>
#include <DirectXMath.h>
#include "Engine/Components.h"
#include "Engine/MeshManager.h"
#include "Engine/ScriptEntity.h"

// Jolt shapes for meshes
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Collision/Shape/ConvexHullShape.h>
#include <Jolt/Physics/Collision/Shape/ScaledShape.h>  // apply runtime scaling

// Jolt raycasting
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/CastResult.h>

using namespace JPH;

namespace Engine {

    bool PhysicsManager::Initialize(Scene* scene) {
        // Register allocator and factory/types
        RegisterDefaultAllocator();
        Factory::sInstance = new Factory();
        RegisterTypes();

        // Temp allocator ~128 MB
        const uint32_t tempSize = 128 * 1024 * 1024;
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

        // World limits / capacity
        const uint32_t maxBodies = 10240;
	    const uint32_t numBodyMutexes = 0; // let Jolt decide the number of mutexes based on the number of worker threads
        const uint32_t maxBodyPairs = 65536;
        const uint32_t maxContactConstraints = 65536;

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

        // Create and register the contact listener
        m_contactListener = new PhysicsContactListener(scene);
        m_physicsSystem->SetContactListener(m_contactListener);

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
        if (m_contactListener)    { delete m_contactListener;    m_contactListener = nullptr; }
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

	    // Clamp deltaTime to avoid large steps
        float safeDT = std::min(deltaTime, 1.0f / 30.0f);
        // Increase collision steps to reduce tunneling at higher speeds
        m_physicsSystem->Update(safeDT, 4, m_tempAllocator, m_jobSystem);

        // Execute all Lua collision callbacks sequentially on the main thread
        if (m_contactListener) {
            m_contactListener->FlushEvents();
        }
    }


    JPH::BodyInterface& PhysicsManager::GetBodyInterface() {
        return m_physicsSystem->GetBodyInterface();
    }


    entt::entity PhysicsManager::CastRay(const Engine::Math::Ray& ray, entt::registry& registry)
    {
        if (!m_physicsSystem)
            return entt::null;

	    // Convert Engine ray to Jolt ray (scale direction for longer raycast)
        JPH::Vec3 origin(ray.origin.x, ray.origin.y, ray.origin.z);
        JPH::Vec3 direction(ray.direction.x * 1000.0f, ray.direction.y * 1000.0f, ray.direction.z * 1000.0f);

        JPH::RRayCast joltRay{ origin, direction };
        JPH::RayCastResult hit;

	    // Perform raycast against the physics world
        if (m_physicsSystem->GetNarrowPhaseQuery().CastRay(joltRay, hit))
        {
            JPH::BodyID hitID = hit.mBodyID;

		    // Iterate through entities with RigidBodyComponent to find matching BodyID
            auto view = registry.view<Engine::RigidBodyComponent>();
            for (auto ent : view)
            {
                if (view.get<Engine::RigidBodyComponent>(ent).bodyID == hitID)
                {
                    return ent; // Match found
                }
            }
        }

        return entt::null;
    }


    // Helpers to convert DirectX types to Jolt types (local to this TU)
    static inline Vec3 ToJolt(const DirectX::XMFLOAT3& v) { return Vec3(v.x, v.y, v.z); }

    static inline Quat ToJolt(const DirectX::XMFLOAT4& q) { return Quat(q.x, q.y, q.z, q.w); }


    JPH::ShapeRefC PhysicsManager::CreatePhysicsShape(const TransformComponent& tc, const RigidBodyComponent& rbc, const MeshManager& meshManager)
    {
        // Step 1: Create Base Shape (Local Space / Unscaled)
        JPH::ShapeRefC baseShape;

        switch (rbc.shape) {
        case RBShape::Box: {
            // Use half extents from RigidBodyComponent (no visual scale yet)
            BoxShapeSettings boxSettings(ToJolt(rbc.halfExtent));
            auto res = boxSettings.Create();
            if (res.HasError()) return nullptr;
            baseShape = res.Get();
            break;
        }
        case RBShape::Sphere: {
            SphereShapeSettings sphereSettings(rbc.radius);
            auto res = sphereSettings.Create();
            if (res.HasError()) return nullptr;
            baseShape = res.Get();
            break;
        }
        case RBShape::Capsule: {
            // Capsule shape: half-height along Y, plus hemispheres with radius
            // rbc.height is total length; Jolt requires half-height (straight section)
            float halfHeight = rbc.height * 0.5f;
            CapsuleShapeSettings capsuleSettings(halfHeight, rbc.radius);
            auto res = capsuleSettings.Create();
            if (res.HasError()) return nullptr;
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
                if (positions.empty()) return nullptr;

                // Convert the mesh vertices to Jolt format for hull creation
                JPH::Array<JPH::Vec3> hull_vertices;
                hull_vertices.reserve(positions.size());

                for (const auto& p : positions) {
                    hull_vertices.push_back(JPH::Vec3(p.x, p.y, p.z));
                }

			    // static bodies can use concave mesh if available
                if (rbc.motionType == RBMotion::Static) {
				    // Get full vertex list
                    VertexList vertex_list;
                    vertex_list.reserve(positions.size());

                    for (const auto& p : positions) {
                        vertex_list.push_back(JPH::Float3(p.x, p.y, p.z));
                    }

				    // Get mesh indices from MeshManager
                    const auto& indices = meshManager.GetMeshIndices(rbc.meshID);

				    // If no indices or not multiple of 3, use convex hull
                    if (indices.empty() || (indices.size() % 3) != 0) {
                        JPH::ConvexHullShapeSettings hull(hull_vertices);
                        auto res = hull.Create();
                        if (res.HasError()) return nullptr;
                        baseShape = res.Get();
                    }
                    else {
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

                        // Create mesh shape from vertices and triangles
                        JPH::MeshShapeSettings meshSettings(vertex_list, tri_list);
                        auto res = meshSettings.Create();
                        if (res.HasError()) return nullptr;
                        baseShape = res.Get();
                    }
                }
			    // dynamic bodies use convex hull
                else {
                    // Create convex hull shape
                    JPH::ConvexHullShapeSettings hull(hull_vertices);
                    auto res = hull.Create();
                    if (res.HasError()) return nullptr;
                    baseShape = res.Get();
                }

                // store in cache
                m_meshShapeCache.emplace(rbc.meshID, baseShape);
            }
            break;
        }
        default:
            return nullptr;
        }

        // Step 2: Apply Transform Scale combined with local Collider Scale (World Space)
        DirectX::XMVECTOR s, r, t;
        DirectX::XMMatrixDecompose(&s, &r, &t, DirectX::XMLoadFloat4x4(&tc.worldMatrix));
        DirectX::XMFLOAT3 worldScale;
        DirectX::XMStoreFloat3(&worldScale, s);

        JPH::Vec3 scale = ToJolt(worldScale);
        if (rbc.shape == RBShape::Mesh) {
            scale = JPH::Vec3(scale.GetX() * rbc.colliderScale.x,
                              scale.GetY() * rbc.colliderScale.y,
                              scale.GetZ() * rbc.colliderScale.z);
        }

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
            if (res.HasError()) return nullptr;
            finalShape = res.Get();
        }

        return finalShape;
    }


    void PhysicsManager::ResetBodyTransform(const TransformComponent& tc, RigidBodyComponent& rbc, const MeshManager& meshManager)
    {
        if (rbc.bodyID.IsInvalid()) return;
        JPH::BodyInterface& bi = m_physicsSystem->GetBodyInterface();

        // Decompose cached World matrix so teleports happen in World Space
        DirectX::XMVECTOR worldS, worldR, worldT;
        DirectX::XMMatrixDecompose(&worldS, &worldR, &worldT, DirectX::XMLoadFloat4x4(&tc.worldMatrix));

        DirectX::XMFLOAT3 worldScale{};
        DirectX::XMFLOAT4 worldRot{};
        DirectX::XMFLOAT3 worldPos{};
        DirectX::XMStoreFloat3(&worldScale, worldS);
        DirectX::XMStoreFloat4(&worldRot, worldR);
        DirectX::XMStoreFloat3(&worldPos, worldT);

        // Evaluate Scale
        JPH::ShapeRefC currentShape = bi.GetShape(rbc.bodyID);
        JPH::Vec3 currentScale(1.0f, 1.0f, 1.0f);

        if (currentShape->GetSubType() == JPH::EShapeSubType::Scaled) {
            currentScale = static_cast<const JPH::ScaledShape*>(currentShape.GetPtr())->GetScale();
        }

        JPH::Vec3 targetScale = ToJolt(worldScale);
        if (rbc.shape == RBShape::Mesh) {
            targetScale = JPH::Vec3(targetScale.GetX() * rbc.colliderScale.x,
                                    targetScale.GetY() * rbc.colliderScale.y,
                                    targetScale.GetZ() * rbc.colliderScale.z);
        }

        // Check for scale deviation
        bool scaleChanged = 
            fabsf(currentScale.GetX() - targetScale.GetX()) > 1e-5f ||
            fabsf(currentScale.GetY() - targetScale.GetY()) > 1e-5f ||
            fabsf(currentScale.GetZ() - targetScale.GetZ()) > 1e-5f;

        if (scaleChanged) {
            // Fully rebuild the body to perfectly recalculate mass and inertia properties
            RemoveRigidBody(rbc.bodyID);
			rbc.bodyID = JPH::BodyID(); // reset to invalid
            return;
        }

        // If scale hasn't changed, just teleport body and kill momentum (World Space)
        bi.SetPositionAndRotation(rbc.bodyID, ToJolt(worldPos), ToJolt(worldRot), JPH::EActivation::Activate);
        bi.SetLinearAndAngularVelocity(rbc.bodyID, JPH::Vec3::sZero(), JPH::Vec3::sZero());
    }


    JPH::BodyID PhysicsManager::CreateRigidBody(entt::entity entity, const TransformComponent& tc, const RigidBodyComponent& rbc, const MeshManager& meshManager) {
        if (!m_physicsSystem) return JPH::BodyID(); // invalid

        // Decompose cached World matrix so bodies are spawned in World Space
        DirectX::XMVECTOR worldS, worldR, worldT;
        DirectX::XMMatrixDecompose(&worldS, &worldR, &worldT, DirectX::XMLoadFloat4x4(&tc.worldMatrix));

        DirectX::XMFLOAT3 worldScale{};
        DirectX::XMFLOAT4 worldRot{};
        DirectX::XMFLOAT3 worldPos{};
        DirectX::XMStoreFloat3(&worldScale, worldS);
        DirectX::XMStoreFloat4(&worldRot, worldR);
        DirectX::XMStoreFloat3(&worldPos, worldT);

        // Feed world scale into CreatePhysicsShape (scaling should be evaluated in world space)
        TransformComponent worldTc = tc;
        worldTc.scale = worldScale;

        JPH::ShapeRefC finalShape = CreatePhysicsShape(worldTc, rbc, meshManager);
        if (!finalShape) return JPH::BodyID();

        // Step 3: Instantiate Body
        BodyCreationSettings creation(
            finalShape,
            ToJolt(worldPos),
            ToJolt(worldRot),
            rbc.motionType == RBMotion::Static ? JPH::EMotionType::Static : JPH::EMotionType::Dynamic,
            rbc.motionType == RBMotion::Static ? Layers::NON_MOVING : Layers::MOVING
        );

        // Physical properties
        creation.mFriction = rbc.friction;
        creation.mRestitution = rbc.restitution;
        creation.mLinearDamping = rbc.linearDamping;

        // Mass properties (only relevant for dynamic bodies)
        if (rbc.motionType == RBMotion::Dynamic) {
            // Let Jolt compute mass properties from the shape, then override mass
            creation.mOverrideMassProperties = EOverrideMassProperties::CalculateInertia;
            creation.mMassPropertiesOverride.mMass = rbc.mass;
        }

        // Store the EnTT entity ID inside the Jolt body
        creation.mUserData = static_cast<uint64_t>(static_cast<uint32_t>(entity));

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


    // --- CONTACT LISTENER IMPLEMENTATION ---

    void PhysicsContactListener::FireCollisionEvent(entt::entity targetEntity, entt::entity otherEntity, const std::string& functionName)
    {
        if (!m_scene || !m_scene->registry.valid(targetEntity) || !m_scene->registry.valid(otherEntity)) return;

        // Check if the target has a Lua script
        if (m_scene->registry.all_of<LuaScriptComponent>(targetEntity))
        {
            auto& lsc = m_scene->registry.get<LuaScriptComponent>(targetEntity);
            for (auto& script : lsc.scripts)
            {
                if (script.isInitialized)
                {
                    // Verify the requested function exists in this script's environment
                    sol::function callback = script.env[functionName];
                    if (callback.valid())
                    {
                        // Wrap the OTHER entity in a ScriptEntity to pass as the argument
                        ScriptEntity otherScriptEnt(otherEntity, m_scene);

                        auto result = callback(otherScriptEnt);
                        if (!result.valid()) {
                            sol::error err = result;
                            std::fprintf(stderr, "Lua Collision Error (%s): %s\n", functionName.c_str(), err.what());
                        }
                    }
                }
            }
        }
    }


    void PhysicsContactListener::OnContactAdded(const JPH::Body& body1, const JPH::Body& body2, const JPH::ContactManifold& manifold, JPH::ContactSettings& ioSettings)
    {
        // Safely lock and push raw Body IDs to the queue from the worker thread
        std::lock_guard<std::mutex> lock(m_eventMutex);
        m_eventQueue.push_back({ body1.GetID(), body2.GetID(), true });
        m_eventQueue.push_back({ body2.GetID(), body1.GetID(), true });
    }


    void PhysicsContactListener::OnContactRemoved(const JPH::SubShapeIDPair& subShapePair)
    {
        // SubShapeIDPair gives us the Body IDs instantly without needing any locks
        std::lock_guard<std::mutex> lock(m_eventMutex);
        m_eventQueue.push_back({ subShapePair.GetBody1ID(), subShapePair.GetBody2ID(), false });
        m_eventQueue.push_back({ subShapePair.GetBody2ID(), subShapePair.GetBody1ID(), false });
    }


    void PhysicsContactListener::FlushEvents()
    {
        std::vector<CollisionEvent> eventsToProcess;

        // Swiftly lock, swap the data into a local vector, and unlock
        {
            std::lock_guard<std::mutex> lock(m_eventMutex);
            eventsToProcess = std::move(m_eventQueue);
            m_eventQueue.clear();
        }

        if (eventsToProcess.empty() || !m_scene) return;

        // Map all active body IDs to their ECS entities to avoid O(N) lookups per event
        std::unordered_map<uint32_t, entt::entity> bodyToEntity;
        auto view = m_scene->registry.view<RigidBodyComponent>();
        for (auto ent : view) {
            JPH::BodyID id = view.get<RigidBodyComponent>(ent).bodyID;
            if (!id.IsInvalid()) {
                // GetIndexAndSequenceNumber() provides a stable, unique uint32 key for the body
                bodyToEntity[id.GetIndexAndSequenceNumber()] = ent;
            }
        }

        // Execute Lua scripts safely on the main thread
        for (const auto& ev : eventsToProcess)
        {
            auto it1 = bodyToEntity.find(ev.body1.GetIndexAndSequenceNumber());
            auto it2 = bodyToEntity.find(ev.body2.GetIndexAndSequenceNumber());

            // Only fire if both entities are still valid and tracked in the ECS
            if (it1 != bodyToEntity.end() && it2 != bodyToEntity.end()) {
                FireCollisionEvent(it1->second, it2->second, ev.isEnter ? "OnCollisionEnter" : "OnCollisionExit");
            }
        }
    }

}