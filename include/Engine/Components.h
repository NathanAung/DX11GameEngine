#pragma once
#include <cstdint>
#include <string>
#include <DirectXMath.h>
#include <d3d11.h> // Added for ID3D11ShaderResourceView*
#include <Jolt/Physics/Body/BodyID.h> // Jolt BodyID

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
        float FOV = DirectX::XM_PIDIV4; // 45 degrees in radians
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
        float moveSpeed = 10.0f;
        float lookSensitivity = 0.0025f;
        float sprintMultiplier = 2.0f;
        float yaw = 0.0f;
        float pitch = 0.0f;
    };

    // Light types 
    enum class LightType : unsigned int
    {
        Directional = 0,
        Point       = 1,
        Spot        = 2
    };

    // Light component
    // Direction is derived from the entity's Transform rotation.
    struct LightComponent
    {
        DirectX::XMFLOAT3 color{ 1.0f, 1.0f, 1.0f };
        float intensity = 1.0f;                         // multiplier
        LightType type = LightType::Directional;
        float range = 10.0f;                            // attenuation range for Point/Spot
        float spotAngle = DirectX::XM_PIDIV4;           // radians, cone angle for Spot
    };

    // Physics: Rigid Body definitions
    enum class RBShape { Box, Sphere, Capsule, Mesh };
    enum class RBMotion { Static, Dynamic };

    struct RigidBodyComponent
    {
        // Config
        RBShape  shape      = RBShape::Box;
        RBMotion motionType = RBMotion::Static;
        float    mass       = 1.0f;
        float    friction   = 0.5f;
        float    restitution= 0.0f; // bounciness

        // Shape dimensions
        DirectX::XMFLOAT3 halfExtent{ 0.5f, 0.5f, 0.5f };   // Box
        float radius = 0.5f;                                // Sphere/Capsule
        float height = 1.0f;                                // Capsule total height

        // Mesh collider binding (used when shape == Mesh)
        int meshID = 0;

        // Runtime (managed by physics system)
        JPH::BodyID bodyID;         // default invalid BodyID
        bool bodyCreated = false;   // whether registered in Jolt world
    };
}