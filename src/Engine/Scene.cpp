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