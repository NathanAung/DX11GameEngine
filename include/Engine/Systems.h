#pragma once
#include <entt/entt.hpp>
#include <d3d11.h>
#include "Engine/Scene.h"
#include "Engine/MeshManager.h"
#include "Engine/ShaderManager.h"

namespace Engine
{
    // cube rotation logic
    void DemoRotationSystem(Engine::Scene& scene, entt::entity cubeEntity, float dt);

    // rendering system
    struct RenderSystem
    {
        // one-time setup to provide constant buffers to the renderer
        static void SetConstantBuffers(ID3D11Buffer* cbProj, ID3D11Buffer* cbView, ID3D11Buffer* cbWorld);

        // Clears RTV/DSV and sets viewport and default states for a frame
        static void SetupFrame(
            ID3D11DeviceContext* context,
            ID3D11RenderTargetView* rtv,
            ID3D11DepthStencilView* dsv,
            ID3D11RasterizerState* rasterState,
            ID3D11DepthStencilState* depthStencilState,
            UINT width, UINT height);

        // Draw all entities that have MeshRendererComponent (+ TransformComponent)
        static void DrawEntities(Engine::Scene& scene, Engine::MeshManager& meshMan, Engine::ShaderManager& shaderMan, ID3D11DeviceContext* context);
    };
}