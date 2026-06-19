#pragma once
#include <entt/entt.hpp>
#include <d3d11.h>
#include "Engine/Scene.h"
#include "Engine/MeshManager.h"
#include "Engine/ShaderManager.h"
#include "Engine/InputManager.h"
#include "Engine/Renderer.h"
#include "Engine/PhysicsManager.h"
#include "Engine/TextureManager.h"
#include "Engine/AudioManager.h"

// Systems for the engine, including various update and rendering systems

namespace Engine
{
    namespace RenderSystem
    {
        // pass Renderer to access context and sampler
        void DrawEntities(Engine::Scene& scene, MeshManager& meshManager, ShaderManager& shaderManager, Engine::Renderer& renderer, Engine::TextureManager& textureManager);

		// Debug draw system for physics colliders. Draws wireframe meshes for entities with ColliderComponent, using the debug shader. Highlights the selected entity's collider if applicable.
        void DrawDebugColliders(Engine::Scene& scene, Engine::Renderer& renderer, Engine::MeshManager& meshManager, Engine::ShaderManager& shaderManager, entt::entity selectedEntity);
    }

    // demo rotation logic
    void DemoRotationSystem(Engine::Scene& scene, entt::entity sampleEntity, float dt);

    // input-driven camera movement and look
    void EditorCameraInputSystem(Engine::Scene& scene, const Engine::InputManager& input, float dt, bool isSceneFocused);

    // build view/projection matrices for active camera and upload via renderer
    void CameraMatrixSystem(Engine::Scene& scene, Engine::Renderer& renderer);

    // physics update system: initialize bodies, step simulation, sync back transforms
    void PhysicsSystem(Engine::Scene& scene, Engine::PhysicsManager& physicsManager, const Engine::MeshManager& meshManager, float dt, bool isPlaying);

    // transform update system: cascades local->world matrices down the relationship tree
    void TransformSystem(Engine::Scene& scene);

    // script lifecycle systems
    void ScriptSystemInit(Engine::Scene& scene);
    void ScriptSystemUpdate(Engine::Scene& scene, float dt);
    void ScriptSystemShutdown(Engine::Scene& scene);

    // audio update system: syncs listener and 3D sound emitters
    void AudioSystem(Engine::Scene& scene, Engine::AudioManager& audioManager);
}