#pragma once
#include <string>
#include <entt/entt.hpp>
#include "Engine/Components.h"

// Scene class manages entities and components using EnTT ECS

namespace Engine
{
    class PhysicsManager;

    class Scene
    {
    public:
        // Exposed by design to allow direct registry access as requested
        entt::registry registry;

        // Backup snapshot registry (used for Play->Edit restore)
        entt::registry m_backupRegistry;

        // Active camera entity used for rendering
        entt::entity m_activeRenderCamera = entt::null;

        // Create a generic entity with ID, Name and default Transform
        entt::entity CreateEntity(const std::string& name);

        // Create a 3d sample entity with MeshRenderer and visible transform
        entt::entity CreateSampleEntity(const std::string& name);

        // Create camera entity and set as active if none set
        entt::entity CreateEditorCamera(const std::string& name, unsigned width, unsigned height);

		// Create in-game camera entity (not controlled by editor, used for gameplay or scripted cameras)
		entt::entity CreateGameCamera(const std::string& name, unsigned width, unsigned height);

        // Create a directional light entity
        entt::entity CreateDirectionalLight(const char* name);

        // Create a point light entity (position/range set on Transform/LightComponent)
        entt::entity CreatePointLight(const char* name,
                                      const DirectX::XMFLOAT3& position,
                                      const DirectX::XMFLOAT3& color,
                                      float intensity,
                                      float range);

        // Create a spot light entity (position/direction via Transform rotation, cone via spotAngle)
        entt::entity CreateSpotLight(const char* name,
                                     const DirectX::XMFLOAT3& position,
                                     const DirectX::XMFLOAT3& direction, // world-space forward
                                     const DirectX::XMFLOAT3& color,
                                     float intensity,
                                     float range,
                                     float spotAngleRadians);

        // Cached default assets so the editor can autonomously spawn primitives
        void SetDefaultAssets(int shaderID, int cubeID, int sphereID, int capsuleID);
        entt::entity CreateCube(const std::string& name);
        entt::entity CreateSphere(const std::string& name);
        entt::entity CreateCapsule(const std::string& name);

        // Safely destroy an entity and unregister any physics bodies (Jolt) first
        void DestroyEntity(entt::entity entity, Engine::PhysicsManager& physicsManager);

        // Backup/restore to support Edit <-> Play state machine
        void CopyToBackup();
        void RestoreFromBackup(Engine::PhysicsManager& physicsManager);

    private:
		// Cache default asset IDs for editor-spawned primitives
        int m_defaultShaderID = 0;
        int m_cubeMeshID = 0;
        int m_sphereMeshID = 0;
        int m_capsuleMeshID = 0;
    };
}