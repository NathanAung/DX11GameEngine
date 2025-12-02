#pragma once
#include <cstdint>
#include <string>
#include <DirectXMath.h>

namespace Engine
{
    // Unique identifier component
    struct IDComponent
    {
        uint64_t id = 0;
    };

    // Human-readable entity name
    struct NameComponent
    {
        std::string name;
    };

    // Local transform (position, rotation as quaternion, scale)
    struct TransformComponent
    {
        DirectX::XMFLOAT3 position{ 0.0f, 0.0f, 0.0f };
        DirectX::XMFLOAT4 rotation{ 0.0f, 0.0f, 0.0f, 1.0f }; // identity quaternion
        DirectX::XMFLOAT3 scale{ 1.0f, 1.0f, 1.0f };
    };

    // Placeholder renderer bindings
    struct MeshRendererComponent
    {
        int meshID = 0;
        int materialID = 0;
    };

    // Camera data (used for future camera entity)
    struct CameraComponent
    {
        float FOV = DirectX::XM_PIDIV4;
        float NearClip = 0.1f;
        float FarClip = 100.0f;
    };
}