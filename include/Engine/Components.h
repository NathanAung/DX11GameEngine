#pragma once
#include <cstdint>
#include <string>
#include <DirectXMath.h>
#include <d3d11.h> // Added for ID3D11ShaderResourceView*
#include <Jolt/Physics/Body/BodyID.h> // Jolt BodyID
#include <entt/entt.hpp>
#include <sol/sol.hpp>

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
        bool isActive = true; // Master entity toggle
    };

    struct RelationshipComponent
    {
        entt::entity parent = entt::null;
        entt::entity firstChild = entt::null;
        entt::entity prevSibling = entt::null;
        entt::entity nextSibling = entt::null;
    };

    // Local transform (position, rotation as quaternion, scale)
    struct TransformComponent
    {
        DirectX::XMFLOAT3 position{ 0.0f, 0.0f, 0.0f };
        DirectX::XMFLOAT4 rotation{ 0.0f, 0.0f, 0.0f, 1.0f }; // identity quaternion
        DirectX::XMFLOAT3 scale{ 1.0f, 1.0f, 1.0f };

        // Initialize to Identity Matrix
        DirectX::XMFLOAT4X4 worldMatrix = {
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f
        };
        bool isDirty = true; // Flag to force recalculation
    };

	enum class MeshType { Cube, Sphere, Capsule, Custom };
    enum class MaterialType { LitColor, UnlitColor, Textured };

    // Placeholder renderer bindings
    struct MeshRendererComponent
    {
        bool isActive = true;

        int meshID = 0;
        ID3D11ShaderResourceView* texture = nullptr; // texture SRV bound to PS t0

        // Material switching
        MaterialType matType = MaterialType::LitColor;
        DirectX::XMFLOAT4 baseColor{ 1.0f, 1.0f, 1.0f, 1.0f }; // Tint or solid color

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
        bool isActive = true;

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
        bool isActive = true;

        // Config
        RBShape  shape      = RBShape::Box;
        RBMotion motionType = RBMotion::Static;
        float    mass       = 1.0f;
        float    friction   = 0.5f;
        float    restitution= 0.0f; // bounciness
		float	 linearDamping = 0.0f;

        // Shape dimensions
        DirectX::XMFLOAT3 halfExtent{ 0.5f, 0.5f, 0.5f };   // Box
        float radius = 0.5f;                                // Sphere/Capsule
        float height = 1.0f;                                // Capsule total height
        DirectX::XMFLOAT3 colliderScale{ 1.0f, 1.0f, 1.0f }; // Mesh collider scale multiplier

        // Mesh collider binding (used when shape == Mesh)
        int meshID = 0;

        // Runtime (managed by physics system)
        JPH::BodyID bodyID;         // default invalid BodyID
        bool bodyCreated = false;   // whether registered in Jolt world

		bool showWireframe = true; // Debug visualization toggle
    };

	// Lua scripting
	// Stores Lua environment and function references for entity scripting
    struct ScriptInstance
    {
        std::string filepath = "";
        sol::environment env;

        sol::function OnCreate;
        sol::function OnUpdate;
        sol::function OnDestroy;

        bool isInitialized = false;
    };

	// Component to hold multiple Lua scripts for an entity
    struct LuaScriptComponent
    {
        std::vector<ScriptInstance> scripts;
    };

	// Audio component to manage sound effects and music
    struct AudioComponent
    {
        std::string filepath = "";
        bool is3D = true;
        bool loop = false;
        bool playOnCreate = true;

        // Runtime state (Managed by AudioSystem)
        void* soundHandle = nullptr;
        bool isPlaying = false;
    };
}