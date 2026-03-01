#include "Engine/Scene.h"
#include "Engine/Components.h"
#include "Engine/PhysicsManager.h"
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

        return e;
    }


    void Scene::SetDefaultAssets(int shaderID, int cubeID, int sphereID, int capsuleID) {
        m_defaultShaderID = shaderID;
        m_cubeMeshID = cubeID;
        m_sphereMeshID = sphereID;
        m_capsuleMeshID = capsuleID;
    }


    entt::entity Scene::CreateCube(const std::string& name) {
        entt::entity e = CreateEntity(name);
        auto& mesh = registry.emplace<MeshRendererComponent>(e);
        mesh.meshID = m_cubeMeshID;
        mesh.materialID = m_defaultShaderID;
        return e;
    }


    entt::entity Scene::CreateSphere(const std::string& name) {
        entt::entity e = CreateEntity(name);
        auto& mesh = registry.emplace<MeshRendererComponent>(e);
        mesh.meshID = m_sphereMeshID;
        mesh.materialID = m_defaultShaderID;
        return e;
    }


    entt::entity Scene::CreateCapsule(const std::string& name) {
        entt::entity e = CreateEntity(name);
        auto& mesh = registry.emplace<MeshRendererComponent>(e);
        mesh.meshID = m_capsuleMeshID;
        mesh.materialID = m_defaultShaderID;
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


    void Scene::DestroyEntity(entt::entity entity, Engine::PhysicsManager& physicsManager)
    {
        if (!registry.valid(entity)) return;

        // Safely remove physics body from Jolt world before destroying the entity
        if (registry.all_of<RigidBodyComponent>(entity))
        {
            auto& rb = registry.get<RigidBodyComponent>(entity);
            if (!rb.bodyID.IsInvalid())
            {
                physicsManager.RemoveRigidBody(rb.bodyID);
            }
        }

        // Destroy the entity and all its components in EnTT
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
        }

        // NOTE: Bodies are rebuilt by PhysicsSystem on the next frame from restored ECS state.
    }
}