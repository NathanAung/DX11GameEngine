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
        tf.position = DirectX::XMFLOAT3{ 0.0f, 20.0f, 0.0f };
		// rotate 90 degrees around Y to face -Z
        XMVECTOR qy = XMQuaternionRotationAxis(XMVectorSet(0,0,1,0), XM_PIDIV2);
		XMStoreFloat4(&tf.rotation, qy);
		tf.scale = DirectX::XMFLOAT3{ 0.1f, 0.1f, 0.1f };   // temporary scale down since model is huge

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


    entt::entity Scene::CreateDirectionalLight(const char* name)
    {
        entt::entity e = registry.create();
        registry.emplace<NameComponent>(e, std::string(name));

        // Direction from Transform's rotation (identity then pitch down)
        TransformComponent tc{};
        // pitch down by ~45 degrees around X
        XMVECTOR qx = XMQuaternionRotationAxis(XMVectorSet(-1,0,0,0), XM_PIDIV2);
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
}