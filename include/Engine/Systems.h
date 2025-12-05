#pragma once
#include <entt/entt.hpp>
#include <d3d11.h>
#include "Engine/Scene.h"
#include "Engine/MeshManager.h"
#include "Engine/ShaderManager.h"
#include "Engine/InputManager.h"
#include "Engine/Renderer.h"

// Systems for the engine, including various update and rendering systems

namespace Engine
{
    namespace RenderSystem
    {
        // pass Renderer to access context and sampler
        void DrawEntities(Engine::Scene& scene, MeshManager& meshManager, ShaderManager& shaderManager, Engine::Renderer& renderer);
    }

    // demo cube rotation logic
    void DemoRotationSystem(Engine::Scene& scene, entt::entity cubeEntity, float dt);

    // input-driven camera movement and look
    void CameraInputSystem(Engine::Scene& scene, const Engine::InputManager& input, float dt);

    // build view/projection matrices for active camera and upload via renderer
    void CameraMatrixSystem(Engine::Scene& scene, Engine::Renderer& renderer);
}