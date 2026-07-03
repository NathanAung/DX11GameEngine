#pragma once
#include <string>
#include <entt/entt.hpp>
#include <sol/sol.hpp>
#include "Engine/Components.h"
#include "Engine/UUID.h"

// Scene class manages entities and components using EnTT ECS

namespace Engine
{
    class PhysicsManager;
    class InputManager;
    class AudioManager;
	class AssetManager;

    class Scene
    {
    public:
        // Exposed by design to allow direct registry access as requested
        entt::registry registry;

        // Backup snapshot registry (used for Play->Edit restore)
        entt::registry m_backupRegistry;

        // Active camera entity used for rendering
        entt::entity m_activeRenderCamera = entt::null;

		// Lua state for scripting
        sol::state m_lua;

        Engine::PhysicsManager* GetPhysicsManager() const { return m_physicsManager; }

        // Audio Manager reference for lifecycle cleanup
        void SetAudioManager(Engine::AudioManager* am) { m_audioManager = am; }
        Engine::AudioManager* GetAudioManager() const { return m_audioManager; }

		// Asset Manager reference for asset lookups and spawning
        Engine::AssetManager* m_assetManager = nullptr;
        void SetAssetManager(Engine::AssetManager* am) { m_assetManager = am; }
        Engine::AssetManager* GetAssetManager() const { return m_assetManager; }

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
        void SetDefaultAssets(int shaderID, int debugShaderID, UUID cubeID, UUID sphereID, UUID capsuleID);
        entt::entity CreateCube(const std::string& name);
        entt::entity CreateSphere(const std::string& name);
        entt::entity CreateCapsule(const std::string& name);

        // Expose default primitive mesh IDs for editor dropdowns
        UUID GetCubeMeshID() const { return m_cubeMeshID; }
        UUID GetSphereMeshID() const { return m_sphereMeshID; }
        UUID GetCapsuleMeshID() const { return m_capsuleMeshID; }
		int GetDefaultShaderID() const { return m_defaultShaderID; }
        int GetDebugShaderID() const { return m_debugShaderID; }

        // Intrusive hierarchy helpers
        void ParentEntity(entt::entity child, entt::entity parent);
        void UnparentEntity(entt::entity child);

        // Safely destroy an entity and unregister any physics bodies (Jolt) first
        void DestroyEntity(entt::entity entity, Engine::PhysicsManager& physicsManager);

        // Backup/restore to support Edit <-> Play state machine
        void CopyToBackup();
        void RestoreFromBackup(Engine::PhysicsManager& physicsManager);

		// Initialize Lua bindings for scene manipulation and scripting
        void InitializeLuaBindings(Engine::InputManager* inputManager, Engine::PhysicsManager* physicsManager);

        // Deferred Destruction Queue
        std::vector<entt::entity> m_entitiesToDestroy;
        void SubmitForDestruction(entt::entity entity);
        void ProcessDestructionQueue(Engine::PhysicsManager& physicsManager);

    private:
		// Cache default asset IDs for editor-spawned primitives
        int m_defaultShaderID = 0;
        int m_debugShaderID = 0;
        UUID m_cubeMeshID = 0;
        UUID m_sphereMeshID = 0;
        UUID m_capsuleMeshID = 0;

        Engine::PhysicsManager* m_physicsManager = nullptr;

        Engine::AudioManager* m_audioManager = nullptr;
    };
}