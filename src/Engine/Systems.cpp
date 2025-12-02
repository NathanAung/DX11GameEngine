#include "Engine/Systems.h"
#include <DirectXMath.h>

using namespace DirectX;

namespace
{
    // Local CBs provided by main at startup
    ID3D11Buffer* g_cbProjPtr = nullptr;
    ID3D11Buffer* g_cbViewPtr = nullptr;
    ID3D11Buffer* g_cbWorldPtr = nullptr;

    inline void UpdateMatrixCB(ID3D11DeviceContext* ctx, ID3D11Buffer* cb, const XMMATRIX& m)
    {
        XMFLOAT4X4 rm;
        XMStoreFloat4x4(&rm, m);
        ctx->UpdateSubresource(cb, 0, nullptr, &rm, 0, 0);
    }
}

namespace Engine
{
    void DemoRotationSystem(Engine::Scene& scene, entt::entity cubeEntity, float dt)
    {
        if (cubeEntity == entt::null) return;
        if (!scene.registry.valid(cubeEntity)) return;

        auto& tc = scene.registry.get<TransformComponent>(cubeEntity);

        static float s_angle = 0.0f;
        s_angle += dt * XM_PIDIV4; // 45 deg/sec

        // Rotation around X
        XMVECTOR qx = XMQuaternionRotationAxis(
            XMVectorSet(1.f, 0.f, 0.f, 0.f),
            s_angle
        );

        // Rotation around Y (slower)
        XMVECTOR qy = XMQuaternionRotationAxis(
            XMVectorSet(0.f, 1.f, 0.f, 0.f),
            s_angle * 0.7f
        );

        // Combine
        XMVECTOR q = XMQuaternionMultiply(qx, qy);
        XMStoreFloat4(&tc.rotation, q);
    }

    void RenderSystem::SetConstantBuffers(ID3D11Buffer* cbProj, ID3D11Buffer* cbView, ID3D11Buffer* cbWorld)
    {
        g_cbProjPtr  = cbProj;
        g_cbViewPtr  = cbView;
        g_cbWorldPtr = cbWorld;
    }

    void RenderSystem::SetupFrame(
        ID3D11DeviceContext* context,
        ID3D11RenderTargetView* rtv,
        ID3D11DepthStencilView* dsv,
        ID3D11RasterizerState* rasterState,
        ID3D11DepthStencilState* depthStencilState,
        UINT width, UINT height)
    {
        // OM: bind RTV/DSV and clear them
        context->OMSetRenderTargets(1, &rtv, dsv);

        const float clearColor[4] = { 0.10f, 0.18f, 0.28f, 1.0f };
        context->ClearRenderTargetView(rtv, clearColor);
        context->ClearDepthStencilView(dsv, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

        // viewport
        D3D11_VIEWPORT vp{};
        vp.TopLeftX = 0.0f;
        vp.TopLeftY = 0.0f;
        vp.Width    = static_cast<float>(width);
        vp.Height   = static_cast<float>(height);
        vp.MinDepth = 0.0f;
        vp.MaxDepth = 1.0f;
        context->RSSetViewports(1, &vp);

        // basic states
        if (rasterState)       context->RSSetState(rasterState);
        if (depthStencilState) context->OMSetDepthStencilState(depthStencilState, 0);
    }

    void RenderSystem::DrawEntities(Engine::Scene& scene, Engine::MeshManager& meshMan, Engine::ShaderManager& shaderMan, ID3D11DeviceContext* context)
    {
        // Bind per-frame CBs (b0=Proj, b1=View, b2=World)
        ID3D11Buffer* vscbs[] = { g_cbProjPtr, g_cbViewPtr, g_cbWorldPtr };
        context->VSSetConstantBuffers(0, 3, vscbs);

        // Render all mesh entities
        auto view = scene.registry.view<TransformComponent, MeshRendererComponent>();
        for (auto ent : view)
        {
            const auto& tc = view.get<TransformComponent>(ent);
            const auto& mr = view.get<MeshRendererComponent>(ent);

            // Bind shader by "materialID" (mapped to shaderID for this demo)
            shaderMan.Bind(mr.materialID, context);

            // Bind mesh
            Engine::MeshBuffers mb{};
            if (!meshMan.GetMesh(mr.meshID, mb)) continue;

            UINT stride = mb.stride;
            UINT offset = 0;
            context->IASetInputLayout(shaderMan.GetInputLayout(mr.materialID));
            context->IASetVertexBuffers(0, 1, &mb.vertexBuffer, &stride, &offset);
            context->IASetIndexBuffer(mb.indexBuffer, mb.indexFormat, 0);
            context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

            // Build world from transform
            const XMMATRIX S = XMMatrixScaling(tc.scale.x, tc.scale.y, tc.scale.z);
            XMVECTOR qn = XMLoadFloat4(&tc.rotation);
            qn = XMQuaternionNormalize(qn);
            const XMMATRIX R = XMMatrixRotationQuaternion(qn);
            const XMMATRIX T = XMMatrixTranslation(tc.position.x, tc.position.y, tc.position.z);
            const XMMATRIX world = S * R * T;

            // Update world CB
            if (g_cbWorldPtr) UpdateMatrixCB(context, g_cbWorldPtr, world);

            // Draw
            context->DrawIndexed(mb.indexCount, 0, 0);
        }
    }
}