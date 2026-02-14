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

// Systems for the engine, including various update and rendering systems

namespace Engine
{
    namespace RenderSystem
    {
        // pass Renderer to access context and sampler
        void DrawEntities(Engine::Scene& scene, MeshManager& meshManager, ShaderManager& shaderManager, Engine::Renderer& renderer, Engine::TextureManager& textureManager);
    }

    // demo rotation logic
    void DemoRotationSystem(Engine::Scene& scene, entt::entity sampleEntity, float dt);

    // input-driven camera movement and look
    void CameraInputSystem(Engine::Scene& scene, const Engine::InputManager& input, float dt);

    // build view/projection matrices for active camera and upload via renderer
    void CameraMatrixSystem(Engine::Scene& scene, Engine::Renderer& renderer);

    // physics update system: initialize bodies, step simulation, sync back transforms
    void PhysicsSystem(Engine::Scene& scene, Engine::PhysicsManager& physicsManager, const Engine::MeshManager& meshManager, float dt);
}