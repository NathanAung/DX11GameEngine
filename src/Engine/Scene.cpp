#include "Engine/Scene.h"
#include "Engine/Components.h"
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

        return e;
    }


    entt::entity Scene::CreateEditorCamera(const std::string& name, unsigned width, unsigned height)
    {
        entt::entity e = CreateEntity(name);

        // Position at z = -5 looking toward +Z in LH space
        auto& tf = registry.get<TransformComponent>(e);
        tf.position = DirectX::XMFLOAT3{ 0.0f, 0.0f, -100.0f };
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
        registry.emplace<LightComponent>(e, lc);

        return e;
    }
}