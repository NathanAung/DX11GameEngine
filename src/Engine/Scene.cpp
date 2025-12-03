#include "Engine/Scene.h"
#include <DirectXMath.h>

using namespace Engine;

entt::entity Scene::CreateEntity(const std::string& name)
{
    entt::entity e = registry.create();

    static uint64_t s_NextID = 1;
    registry.emplace<IDComponent>(e, IDComponent{ s_NextID++ });
    registry.emplace<NameComponent>(e, NameComponent{ name });
    registry.emplace<TransformComponent>(e, TransformComponent{});

    return e;
}

entt::entity Scene::CreateCube(const std::string& name)
{
    entt::entity e = CreateEntity(name);

    // Placeholder resource IDs (to be wired to actual resources)
    registry.emplace<MeshRendererComponent>(e, MeshRendererComponent{});

    // Put cube in front of our LH camera (camera at z=-5 looking +Z)
    auto& tf = registry.get<TransformComponent>(e);
    tf.position = DirectX::XMFLOAT3{ 0.0f, 0.0f, 5.0f };

    return e;
}

entt::entity Scene::CreateCamera(const std::string& name, unsigned width, unsigned height)
{
    entt::entity e = CreateEntity(name);

    // Position at z = -5 looking toward +Z in LH space
    auto& tf = registry.get<TransformComponent>(e);
    tf.position = DirectX::XMFLOAT3{ 0.0f, 0.0f, -5.0f };
    tf.rotation = DirectX::XMFLOAT4{ 0.0f, 0.0f, 0.0f, 1.0f };
    tf.scale    = DirectX::XMFLOAT3{ 1.0f, 1.0f, 1.0f };

    // Default camera params
    registry.emplace<CameraComponent>(e, CameraComponent{ DirectX::XM_PIDIV4, 0.1f, 100.0f, true });

    // Viewport and movement/look parameters
    registry.emplace<ViewportComponent>(e, ViewportComponent{ width, height, 5.0f, 0.0025f, 2.0f });

    // If no active camera set, set this one
    if (m_activeRenderCamera == entt::null)
        m_activeRenderCamera = e;

    return e;
}