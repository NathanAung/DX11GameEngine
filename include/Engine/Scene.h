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

        // Backup/restore to support Edit <-> Play state machine
        void CopyToBackup();
        void RestoreFromBackup(Engine::PhysicsManager& physicsManager);
    };
}