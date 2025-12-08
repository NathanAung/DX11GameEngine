#pragma once
#include <cstdint>
#include <string>
#include <DirectXMath.h>
#include <d3d11.h> // Added for ID3D11ShaderResourceView*

// Components class is used to define various components for ECS architecture

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
        ID3D11ShaderResourceView* texture = nullptr; // texture SRV bound to PS t0

        // Simple PBR material parameters
        float roughness = 0.5f; // [0..1]
        float metallic  = 0.0f; // [0..1]
    };

    // Camera data
    struct CameraComponent
    {
        float FOV = DirectX::XM_PIDIV4;
        float nearClip = 0.1f;
        float farClip = 5000.0f;
        bool invertY = true;
    };

    // control mode enum for camera
    enum class CameraControlMode
    {
        EditorCam = 0,
        Scripted
    };

    // viewport only stores dimensions
    struct ViewportComponent
    {
        unsigned width = 1280;
        unsigned height = 720;
    };

    // editor camera control parameters moved here
    struct EditorCamControlComponent
    {
        CameraControlMode mode = CameraControlMode::EditorCam;
        float moveSpeed = 50.0f;
        float lookSensitivity = 0.0025f;
        float sprintMultiplier = 2.0f;
        float yaw = 0.0f;
        float pitch = 0.0f;
    };

    // Light component (direction comes from TransformComponent's rotation)
    struct LightComponent
    {
        DirectX::XMFLOAT3 color{ 1.0f, 1.0f, 1.0f };
        float intensity = 1.0f; // multiplier
        // For Phase 3 Step 7, this will be used as directional light. Point/spot will be added later.
    };
}