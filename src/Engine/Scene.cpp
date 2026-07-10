#include "Engine/Scene.h"
#include "Engine/Components.h"
#include "Engine/AssetManager.h"
#include "Engine/PhysicsManager.h"
#include "Engine/InputManager.h"
#include "Engine/AudioManager.h"
#include "Engine/ScriptEntity.h"
#include <DirectXMath.h>

using namespace DirectX;

namespace Engine
{
    entt::entity Scene::CreateEntity(const std::string& name)
    {
        entt::entity e = registry.create();

        static uint64_t s_NextID = 1;
        registry.emplace<IDComponent>(e, IDComponent{ s_NextID++ });
        registry.emplace<NameComponent>(e, NameComponent{ name });
        registry.emplace<TransformComponent>(e, TransformComponent{});

        return e;
    }


    entt::entity Scene::CreateSampleEntity(const std::string& name)
    {
        entt::entity e = CreateEntity(name);

		// Add MeshRendererComponent
        registry.emplace<MeshRendererComponent>(e, MeshRendererComponent{});

		// Position at z = 5 looking toward -Z in LH space
        auto& tf = registry.get<TransformComponent>(e);
        tf.position = DirectX::XMFLOAT3{ 0.0f, 0.0f, 5.0f };
		// rotate 90 degrees around Y to face -Z
        /*XMVECTOR qy = XMQuaternionRotationAxis(XMVectorSet(0,0,1,0), XM_PIDIV2);
		XMStoreFloat4(&tf.rotation, qy);*/
		tf.scale = DirectX::XMFLOAT3{ 0.1f, 0.1f, 0.1f };   // temporary scale down since model is huge
        tf.isDirty = true;

        return e;
    }


    void Scene::SetDefaultAssets(int shaderID, int debugShaderID, UUID cubeID, UUID sphereID, UUID capsuleID) {
        m_defaultShaderID = shaderID;
        m_debugShaderID = debugShaderID;
        m_cubeMeshID = cubeID;
        m_sphereMeshID = sphereID;
        m_capsuleMeshID = capsuleID;
    }


    entt::entity Scene::CreateCube(const std::string& name) {
        entt::entity e = CreateEntity(name);
        auto& mesh = registry.emplace<MeshRendererComponent>(e);
        mesh.meshID = m_cubeMeshID;
        return e;
    }


    entt::entity Scene::CreateSphere(const std::string& name) {
        entt::entity e = CreateEntity(name);
        auto& mesh = registry.emplace<MeshRendererComponent>(e);
        mesh.meshID = m_sphereMeshID;
        return e;
    }


    entt::entity Scene::CreateCapsule(const std::string& name) {
        entt::entity e = CreateEntity(name);
        auto& mesh = registry.emplace<MeshRendererComponent>(e);
        mesh.meshID = m_capsuleMeshID;
        return e;
    }


    entt::entity Scene::CreateEditorCamera(const std::string& name, unsigned width, unsigned height)
    {
        entt::entity e = CreateEntity(name);

        // Position at z = -5 looking toward +Z in LH space
        auto& tf = registry.get<TransformComponent>(e);
        tf.position = DirectX::XMFLOAT3{ 0.0f, 0.0f, -10.0f };
        tf.rotation = DirectX::XMFLOAT4{ 0.0f, 0.0f, 0.0f, 1.0f };
        tf.scale    = DirectX::XMFLOAT3{ 1.0f, 1.0f, 1.0f };
        tf.isDirty = true;

        // Attach camera, viewport, editor cam control
        registry.emplace<CameraComponent>(e, CameraComponent{});
        registry.emplace<ViewportComponent>(e, ViewportComponent{ width, height });
        registry.emplace<EditorCamControlComponent>(e, EditorCamControlComponent{});
        if (m_activeRenderCamera == entt::null)
            m_activeRenderCamera = e;
        return e;
    }


    entt::entity Scene::CreateGameCamera(const std::string& name, unsigned width, unsigned height)
    {
        entt::entity e = CreateEntity(name);

        // Position at z = -5 looking toward +Z in LH space
        auto& tf = registry.get<TransformComponent>(e);
        tf.position = DirectX::XMFLOAT3{ 0.0f, 0.0f, -10.0f };
        tf.rotation = DirectX::XMFLOAT4{ 0.0f, 0.0f, 0.0f, 1.0f };
        tf.scale    = DirectX::XMFLOAT3{ 1.0f, 1.0f, 1.0f };
        tf.isDirty = true;

        // Attach camera and viewport
        registry.emplace<CameraComponent>(e, CameraComponent{});
        registry.emplace<ViewportComponent>(e, ViewportComponent{ width, height });
        
        return e;
	}


    entt::entity Scene::CreateDirectionalLight(const char* name)
    {
        entt::entity e = registry.create();
        registry.emplace<NameComponent>(e, std::string(name));

        // Direction from Transform's rotation (identity then pitch down)
        TransformComponent tc{};
        // pitch down by ~45 degrees around X
        XMVECTOR qx = XMQuaternionRotationAxis(XMVectorSet(1,0,0,0), XM_PIDIV4);
        XMStoreFloat4(&tc.rotation, qx);
        tc.position = XMFLOAT3(0, 0, 0);
        tc.isDirty = true;
        registry.emplace<TransformComponent>(e, tc);

        // White light, intensity 5.0
        LightComponent lc{};
        lc.color = XMFLOAT3(1.0f, 1.0f, 1.0f);
        lc.intensity = 5.0f;
        lc.type = LightType::Directional;
        registry.emplace<LightComponent>(e, lc);

        return e;
    }


    entt::entity Scene::CreatePointLight(const char* name,
                                         const DirectX::XMFLOAT3& position,
                                         const DirectX::XMFLOAT3& color,
                                         float intensity,
                                         float range)
    {
        entt::entity e = registry.create();
        registry.emplace<NameComponent>(e, std::string(name));

        TransformComponent tc{};
        tc.position = position;
        tc.rotation = XMFLOAT4{ 0.0f, 0.0f, 0.0f, 1.0f };
        tc.scale    = XMFLOAT3{ 1.0f, 1.0f, 1.0f };
        tc.isDirty = true;
        registry.emplace<TransformComponent>(e, tc);

        LightComponent lc{};
        lc.color     = color;
        lc.intensity = intensity;
        lc.type      = LightType::Point;
        lc.range     = range;
        // spotAngle unused for point
        registry.emplace<LightComponent>(e, lc);

        return e;
    }


    entt::entity Scene::CreateSpotLight(const char* name,
                                        const DirectX::XMFLOAT3& position,
                                        const DirectX::XMFLOAT3& direction, // world-space forward
                                        const DirectX::XMFLOAT3& color,
                                        float intensity,
                                        float range,
                                        float spotAngleRadians)
    {
        entt::entity e = registry.create();
        registry.emplace<NameComponent>(e, std::string(name));

        // Build a quaternion that aligns +Z with desired direction (LH)
        // Compute basis from forward and world up
        XMVECTOR fwd = XMVector3Normalize(XMLoadFloat3(&direction));
        XMVECTOR worldUp = XMVectorSet(0, 1, 0, 0);
        // Handle degenerate up direction by adjusting if needed
        XMVECTOR right = XMVector3Normalize(XMVector3Cross(worldUp, fwd));
        XMVECTOR up = XMVector3Normalize(XMVector3Cross(fwd, right));

		// Create rotation matrix
        XMMATRIX basis;
        basis.r[0] = XMVectorSet(XMVectorGetX(right), XMVectorGetY(right), XMVectorGetZ(right), 0.0f);
        basis.r[1] = XMVectorSet(XMVectorGetX(up),    XMVectorGetY(up),    XMVectorGetZ(up),    0.0f);
        basis.r[2] = XMVectorSet(XMVectorGetX(fwd),   XMVectorGetY(fwd),   XMVectorGetZ(fwd),   0.0f);
        basis.r[3] = XMVectorSet(0, 0, 0, 1);

		// Convert to quaternion
        XMVECTOR q = XMQuaternionRotationMatrix(basis);
        q = XMQuaternionNormalize(q);

        TransformComponent tc{};
        tc.position = position;
        XMStoreFloat4(&tc.rotation, q);
        tc.scale = XMFLOAT3{ 1.0f, 1.0f, 1.0f };
        tc.isDirty = true;
        registry.emplace<TransformComponent>(e, tc);

        LightComponent lc{};
        lc.color     = color;
        lc.intensity = intensity;
        lc.type      = LightType::Spot;
        lc.range     = range;
        lc.spotAngle = spotAngleRadians;
        registry.emplace<LightComponent>(e, lc);

        return e;
    }


    void Scene::UnparentEntity(entt::entity child)
    {
        if (!registry.valid(child) || !registry.all_of<RelationshipComponent>(child)) return;
        auto& childRel = registry.get<RelationshipComponent>(child);
        if (childRel.parent == entt::null) return; // Already a root

        auto& parentRel = registry.get<RelationshipComponent>(childRel.parent);

        // Remove from parent's child list
        if (parentRel.firstChild == child) {
            parentRel.firstChild = childRel.nextSibling;
        }

        // Repair sibling chain
        if (childRel.prevSibling != entt::null) {
            registry.get<RelationshipComponent>(childRel.prevSibling).nextSibling = childRel.nextSibling;
        }
        if (childRel.nextSibling != entt::null) {
            registry.get<RelationshipComponent>(childRel.nextSibling).prevSibling = childRel.prevSibling;
        }

        // Preserve World Transform
        if (registry.all_of<TransformComponent>(child)) {
            auto& tc = registry.get<TransformComponent>(child);
            DirectX::XMMATRIX world = DirectX::XMLoadFloat4x4(&tc.worldMatrix);
            DirectX::XMVECTOR s, r, t;
            DirectX::XMMatrixDecompose(&s, &r, &t, world);
            DirectX::XMStoreFloat3(&tc.scale, s);
            DirectX::XMStoreFloat4(&tc.rotation, r);
            DirectX::XMStoreFloat3(&tc.position, t);
            tc.isDirty = true;
        }

        // Clear child's links
        childRel.parent = entt::null;
        childRel.prevSibling = entt::null;
        childRel.nextSibling = entt::null;

        // Force matrix recalculation
        if (registry.all_of<TransformComponent>(child)) registry.get<TransformComponent>(child).isDirty = true;
    }

    void Scene::ParentEntity(entt::entity child, entt::entity parent)
    {
        if (child == parent || !registry.valid(child) || !registry.valid(parent)) return;

        // Check for cycles (is the new parent actually a descendant of the child?)
        entt::entity ancestor = parent;
        while (ancestor != entt::null) {
            if (ancestor == child) return; // Cycle detected, abort!
            if (!registry.all_of<RelationshipComponent>(ancestor)) break;
            ancestor = registry.get<RelationshipComponent>(ancestor).parent;
        }

        // Unparent from current parent
        UnparentEntity(child);

        // Convert World to Local relative to new parent
        if (registry.all_of<TransformComponent>(child) && registry.all_of<TransformComponent>(parent)) {
            auto& childTc = registry.get<TransformComponent>(child);
            auto& parentTc = registry.get<TransformComponent>(parent);

            DirectX::XMMATRIX childWorld = DirectX::XMLoadFloat4x4(&childTc.worldMatrix);
            DirectX::XMMATRIX parentWorld = DirectX::XMLoadFloat4x4(&parentTc.worldMatrix);

            DirectX::XMVECTOR det;
            DirectX::XMMATRIX parentInv = DirectX::XMMatrixInverse(&det, parentWorld);

            // New Local = Child World * Inverse(Parent World)
            DirectX::XMMATRIX newLocal = childWorld * parentInv;

            DirectX::XMVECTOR s, r, t;
            DirectX::XMMatrixDecompose(&s, &r, &t, newLocal);
            DirectX::XMStoreFloat3(&childTc.scale, s);
            DirectX::XMStoreFloat4(&childTc.rotation, r);
            DirectX::XMStoreFloat3(&childTc.position, t);
            childTc.isDirty = true;
        }

        // 3. Ensure both have RelationshipComponents
        auto& childRel = registry.get_or_emplace<RelationshipComponent>(child);
        auto& parentRel = registry.get_or_emplace<RelationshipComponent>(parent);

        // 4. Attach to new parent
        childRel.parent = parent;
        if (parentRel.firstChild == entt::null) {
            parentRel.firstChild = child;
        } else {
            // Find the last sibling and append
            entt::entity current = parentRel.firstChild;
            while (registry.get<RelationshipComponent>(current).nextSibling != entt::null) {
                current = registry.get<RelationshipComponent>(current).nextSibling;
            }
            registry.get<RelationshipComponent>(current).nextSibling = child;
            childRel.prevSibling = current;
        }

        // Force matrix recalculation
        if (registry.all_of<TransformComponent>(child)) registry.get<TransformComponent>(child).isDirty = true;
    }


    void Scene::DestroyEntity(entt::entity entity, Engine::PhysicsManager& physicsManager)
    {
        if (!registry.valid(entity)) return;

        // Cascade delete to all children
        if (registry.all_of<RelationshipComponent>(entity)) {
            auto& rel = registry.get<RelationshipComponent>(entity);
            entt::entity currentChild = rel.firstChild;
            while (currentChild != entt::null) {
                // Cache the next sibling before destroying the current child!
                entt::entity next = registry.get<RelationshipComponent>(currentChild).nextSibling;

                // Recursively destroy the child
                DestroyEntity(currentChild, physicsManager);

                currentChild = next;
            }
        }

        // Unparent this entity so its parent cleanly removes it from the linked list
        UnparentEntity(entity);

        // Existing Physics Cleanup
        if (registry.all_of<RigidBodyComponent>(entity)) {
            auto& rbc = registry.get<RigidBodyComponent>(entity);
            physicsManager.RemoveRigidBody(rbc.bodyID);
        }

        // Destroy the entity
        registry.destroy(entity);
    }


    void Scene::CopyToBackup()
    {
		// Clear backup registry and copy all entities and core components from main registry
        m_backupRegistry.clear();
        for (auto entity : registry.view<entt::entity>())
        {
            // Create the exact same entity ID in the backup registry
            auto copy = m_backupRegistry.create(entity);

            // Copy all core components if they exist
            if (registry.all_of<IDComponent>(entity)) m_backupRegistry.emplace<IDComponent>(copy, registry.get<IDComponent>(entity));
            if (registry.all_of<NameComponent>(entity)) m_backupRegistry.emplace<NameComponent>(copy, registry.get<NameComponent>(entity));
            if (registry.all_of<TransformComponent>(entity)) m_backupRegistry.emplace<TransformComponent>(copy, registry.get<TransformComponent>(entity));
            if (registry.all_of<RigidBodyComponent>(entity)) m_backupRegistry.emplace<RigidBodyComponent>(copy, registry.get<RigidBodyComponent>(entity));
            if (registry.all_of<MeshRendererComponent>(entity)) m_backupRegistry.emplace<MeshRendererComponent>(copy, registry.get<MeshRendererComponent>(entity));
            if (registry.all_of<LightComponent>(entity)) m_backupRegistry.emplace<LightComponent>(copy, registry.get<LightComponent>(entity));
            if (registry.all_of<CameraComponent>(entity)) m_backupRegistry.emplace<CameraComponent>(copy, registry.get<CameraComponent>(entity));
            if (registry.all_of<ViewportComponent>(entity)) m_backupRegistry.emplace<ViewportComponent>(copy, registry.get<ViewportComponent>(entity));
            if (registry.all_of<EditorCamControlComponent>(entity)) m_backupRegistry.emplace<EditorCamControlComponent>(copy, registry.get<EditorCamControlComponent>(entity));
            if (registry.all_of<RelationshipComponent>(entity)) m_backupRegistry.emplace<RelationshipComponent>(copy, registry.get<RelationshipComponent>(entity));
			if (registry.all_of<LuaScriptComponent>(entity)) m_backupRegistry.emplace<LuaScriptComponent>(copy, registry.get<LuaScriptComponent>(entity));
            if (registry.all_of<AudioComponent>(entity)) m_backupRegistry.emplace<AudioComponent>(copy, registry.get<AudioComponent>(entity));
        }
    }


    void Scene::RestoreFromBackup(Engine::PhysicsManager& physicsManager)
    {
        // Completely destroy all current Jolt bodies before resetting the registry
        auto physView = registry.view<RigidBodyComponent>();
        for (auto entity : physView)
        {
            physicsManager.RemoveRigidBody(physView.get<RigidBodyComponent>(entity).bodyID);
        }

        // Destroy all active audio handles before resetting
        auto audioView = registry.view<AudioComponent>();
        for (auto entity : audioView) {
            if (m_audioManager) {
                auto& ac = audioView.get<AudioComponent>(entity);
                if (ac.soundHandle) m_audioManager->DestroyAudio(ac.soundHandle);
            }
        }

		// Clear main registry and copy all entities and core components from backup registry
        registry.clear();
        for (auto entity : m_backupRegistry.view<entt::entity>())
        {
            auto restored = registry.create(entity);
            // Manually copy all core components back from the backup
            if (m_backupRegistry.all_of<IDComponent>(entity)) registry.emplace<IDComponent>(restored, m_backupRegistry.get<IDComponent>(entity));
            if (m_backupRegistry.all_of<NameComponent>(entity)) registry.emplace<NameComponent>(restored, m_backupRegistry.get<NameComponent>(entity));
            if (m_backupRegistry.all_of<TransformComponent>(entity)) registry.emplace<TransformComponent>(restored, m_backupRegistry.get<TransformComponent>(entity));
            if (m_backupRegistry.all_of<RigidBodyComponent>(entity)) 
            {
                auto rb = m_backupRegistry.get<RigidBodyComponent>(entity);
                // Invalidate the runtime state so Jolt creates a fresh body next frame
                rb.bodyID = JPH::BodyID();
                rb.bodyCreated = false;
                registry.emplace<RigidBodyComponent>(restored, rb);
            }
            if (m_backupRegistry.all_of<MeshRendererComponent>(entity)) registry.emplace<MeshRendererComponent>(restored, m_backupRegistry.get<MeshRendererComponent>(entity));
            if (m_backupRegistry.all_of<LightComponent>(entity)) registry.emplace<LightComponent>(restored, m_backupRegistry.get<LightComponent>(entity));
            if (m_backupRegistry.all_of<CameraComponent>(entity)) registry.emplace<CameraComponent>(restored, m_backupRegistry.get<CameraComponent>(entity));
            if (m_backupRegistry.all_of<ViewportComponent>(entity)) registry.emplace<ViewportComponent>(restored, m_backupRegistry.get<ViewportComponent>(entity));
            if (m_backupRegistry.all_of<EditorCamControlComponent>(entity)) registry.emplace<EditorCamControlComponent>(restored, m_backupRegistry.get<EditorCamControlComponent>(entity));
            if (m_backupRegistry.all_of<RelationshipComponent>(entity)) registry.emplace<RelationshipComponent>(restored, m_backupRegistry.get<RelationshipComponent>(entity));
			if (m_backupRegistry.all_of<LuaScriptComponent>(entity)) registry.emplace<LuaScriptComponent>(restored, m_backupRegistry.get<LuaScriptComponent>(entity));
            if (m_backupRegistry.all_of<AudioComponent>(entity)) {
                auto ac = m_backupRegistry.get<AudioComponent>(entity);
                // Reset runtime handle so it dynamically reloads a fresh copy on Play
                ac.soundHandle = nullptr;
                ac.isPlaying = false;
                registry.emplace<AudioComponent>(restored, ac);
            }
        }

        // NOTE: Bodies are rebuilt by PhysicsSystem on the next frame from restored ECS state.
    }


    void Scene::SubmitForDestruction(entt::entity entity)
    {
        // Add to queue if not already queued
        if (std::find(m_entitiesToDestroy.begin(), m_entitiesToDestroy.end(), entity) == m_entitiesToDestroy.end()) {
            m_entitiesToDestroy.push_back(entity);
        }
    }

    void Scene::ProcessDestructionQueue(Engine::PhysicsManager& physicsManager)
    {
        for (auto entity : m_entitiesToDestroy) {
            DestroyEntity(entity, physicsManager);
        }
        m_entitiesToDestroy.clear();
    }


    void Scene::InitializeLuaBindings(Engine::InputManager* inputManager, Engine::PhysicsManager* physicsManager)
    {
        m_physicsManager = physicsManager;

		// Open basic Lua libraries
        m_lua.open_libraries(sol::lib::base, sol::lib::math, sol::lib::table, sol::lib::string);

        // Bind DirectX::XMFLOAT3 as Vector3
        m_lua.new_usertype<DirectX::XMFLOAT3>("Vector3",
            sol::constructors<DirectX::XMFLOAT3(), DirectX::XMFLOAT3(float, float, float)>(),
            "x", &DirectX::XMFLOAT3::x,
            "y", &DirectX::XMFLOAT3::y,
            "z", &DirectX::XMFLOAT3::z
        );

        // Bind TransformComponent
        m_lua.new_usertype<TransformComponent>("TransformComponent",
			// Expose position and scale as properties that mark the component dirty when set
            "position", sol::property(
                [](TransformComponent& tc) -> DirectX::XMFLOAT3 { return tc.position; },
                [](TransformComponent& tc, const DirectX::XMFLOAT3& pos) { tc.position = pos; tc.isDirty = true; }
            ),
            "scale", sol::property(
                [](TransformComponent& tc) -> DirectX::XMFLOAT3 { return tc.scale; },
                [](TransformComponent& tc, const DirectX::XMFLOAT3& scl) { tc.scale = scl; tc.isDirty = true; }
            ),
			// Alternatively, expose explicit setter methods for position and scale
            "SetPosition", [](TransformComponent& tc, float x, float y, float z) {
                tc.position = DirectX::XMFLOAT3(x, y, z);
                tc.isDirty = true;
            },
            "SetScale", [](TransformComponent& tc, float x, float y, float z) {
                tc.scale = DirectX::XMFLOAT3(x, y, z);
                tc.isDirty = true;
            }
        );

        // Bind Component Enums
        m_lua.new_enum<Engine::MeshType>("MeshType", {
            { "Cube", Engine::MeshType::Cube },
            { "Sphere", Engine::MeshType::Sphere },
            { "Capsule", Engine::MeshType::Capsule },
            { "Custom", Engine::MeshType::Custom }
        });

        m_lua.new_enum<Engine::RBShape>("RBShape", {
            { "Box", Engine::RBShape::Box },
            { "Sphere", Engine::RBShape::Sphere },
            { "Capsule", Engine::RBShape::Capsule },
            { "Mesh", Engine::RBShape::Mesh }
        });

        m_lua.new_enum<Engine::RBMotion>("RBMotion", {
            { "Static", Engine::RBMotion::Static },
            { "Dynamic", Engine::RBMotion::Dynamic }
        });

        // Bind ScriptEntity wrapper
        m_lua.new_usertype<ScriptEntity>("Entity",
            sol::constructors<ScriptEntity(entt::entity, Engine::Scene*)>(),
            "GetTransform", &ScriptEntity::GetTransform,
            "GetName", &ScriptEntity::GetName,
            "ApplyLinearImpulse", &ScriptEntity::ApplyLinearImpulse,
            "Instantiate", &ScriptEntity::Instantiate,
            "InstantiateCube", &ScriptEntity::InstantiateCube,
            "InstantiateSphere", &ScriptEntity::InstantiateSphere,
            "Destroy", &ScriptEntity::Destroy,
            "AddMeshRenderer", &ScriptEntity::AddMeshRenderer,
            "AddRigidBody", &ScriptEntity::AddRigidBody,
            "AddLuaScript", &ScriptEntity::AddLuaScript,
            "AddAudioComponent", &ScriptEntity::AddAudioComponent,
            "PlayAudio", &ScriptEntity::PlayAudio,
            "StopAudio", &ScriptEntity::StopAudio,
            "SetAudioVolume", &ScriptEntity::SetAudioVolume
        );

        // Expose Engine::Key Enum 
        m_lua.new_enum<Engine::Key>("Key", {
            { "A", Engine::Key::A }, { "B", Engine::Key::B }, { "C", Engine::Key::C }, { "D", Engine::Key::D },
            { "E", Engine::Key::E }, { "F", Engine::Key::F }, { "G", Engine::Key::G }, { "H", Engine::Key::H },
            { "I", Engine::Key::I }, { "J", Engine::Key::J }, { "K", Engine::Key::K }, { "L", Engine::Key::L },
            { "M", Engine::Key::M }, { "N", Engine::Key::N }, { "O", Engine::Key::O }, { "P", Engine::Key::P },
            { "Q", Engine::Key::Q }, { "R", Engine::Key::R }, { "S", Engine::Key::S }, { "T", Engine::Key::T },
            { "U", Engine::Key::U }, { "V", Engine::Key::V }, { "W", Engine::Key::W }, { "X", Engine::Key::X },
            { "Y", Engine::Key::Y }, { "Z", Engine::Key::Z },
            { "Num0", Engine::Key::Num0 }, { "Num1", Engine::Key::Num1 }, { "Num2", Engine::Key::Num2 },
            { "Num3", Engine::Key::Num3 }, { "Num4", Engine::Key::Num4 }, { "Num5", Engine::Key::Num5 },
            { "Num6", Engine::Key::Num6 }, { "Num7", Engine::Key::Num7 }, { "Num8", Engine::Key::Num8 },
            { "Num9", Engine::Key::Num9 },
            { "Up", Engine::Key::Up }, { "Down", Engine::Key::Down }, { "Left", Engine::Key::Left }, { "Right", Engine::Key::Right },
            { "Space", Engine::Key::Space }, { "Escape", Engine::Key::Escape }, { "Enter", Engine::Key::Enter },
            { "Tab", Engine::Key::Tab }, { "Backspace", Engine::Key::Backspace },
            { "LShift", Engine::Key::LShift }, { "RShift", Engine::Key::RShift },
            { "LControl", Engine::Key::LControl }, { "RControl", Engine::Key::RControl },
            { "LAlt", Engine::Key::LAlt }, { "RAlt", Engine::Key::RAlt }
            });

        // Bind InputManager
        m_lua.new_usertype<Engine::InputManager>("InputManager",
            "IsKeyDown", &Engine::InputManager::IsKeyDown,
            "IsKeyPressed", &Engine::InputManager::IsKeyPressed,
            "IsKeyReleased", &Engine::InputManager::IsKeyReleased
        );

        // Register Global Input Pointer
        m_lua["Input"] = inputManager;
    }
}