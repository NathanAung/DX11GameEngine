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

        // get TransformComponent
        auto& tc = scene.registry.get<TransformComponent>(cubeEntity);

        static float s_angle = 0.0f;
        s_angle += dt * XM_PIDIV4; // 45 deg/sec

        // Rotation around X
        XMVECTOR qx = XMQuaternionRotationAxis(XMVectorSet(1.f, 0.f, 0.f, 0.f), s_angle);

        // Rotation around Y (slower)
        XMVECTOR qy = XMQuaternionRotationAxis(XMVectorSet(0.f, 1.f, 0.f, 0.f), s_angle * 0.7f);

        // Combine
        XMVECTOR q = XMQuaternionMultiply(qx, qy);
        XMStoreFloat4(&tc.rotation, q);
    }


    void CameraInputSystem(Engine::Scene& scene, const Engine::InputManager& input, float dt)
    {
        // Iterate all entities with Transform + FlyCamControl
        auto view = scene.registry.view<TransformComponent, EditorCamControlComponent>();
        for (auto ent : view)
        {
            auto& tf = view.get<TransformComponent>(ent);
            auto& fc = view.get<EditorCamControlComponent>(ent);

            if (fc.mode != CameraControlMode::EditorCam)
                continue;

            // Mouse look: accumulate yaw/pitch
            const auto md = input.GetMouseDelta();
            fc.yaw   += static_cast<float>(md.dx) * fc.lookSensitivity;
            fc.pitch += static_cast<float>(md.dy) * fc.lookSensitivity * -1.0f; // default invertY

            // Clamp and wrap
            const float kPitchLimit = XMConvertToRadians(89.0f);
            if (fc.pitch >  kPitchLimit) fc.pitch =  kPitchLimit;
            if (fc.pitch < -kPitchLimit) fc.pitch = -kPitchLimit;
            if (fc.yaw > XM_PI)  fc.yaw -= XM_2PI;
            if (fc.yaw < -XM_PI) fc.yaw += XM_2PI;

            // Recompute basis from yaw/pitch (LH, yaw=0 looks +Z)
            const float cy = cosf(fc.yaw);
            const float sy = sinf(fc.yaw);
            const float cp = cosf(fc.pitch);
            const float sp = sinf(fc.pitch);

            XMVECTOR forward = XMVector3Normalize(XMVectorSet(sy * cp, sp, cy * cp, 0.0f));
            XMVECTOR worldUp = XMVectorSet(0, 1, 0, 0);
            XMVECTOR right = XMVector3Normalize(XMVector3Cross(worldUp, forward));
            XMVECTOR up = XMVector3Normalize(XMVector3Cross(forward, right));

            // Movement: W/A/S/D (+ Shift sprint), Space up
            float speed = fc.moveSpeed * dt;
            if (input.IsKeyDown(Key::LShift)) speed *= fc.sprintMultiplier;

            XMVECTOR move = XMVectorZero();
            if (input.IsKeyDown(Key::W)) move = XMVectorAdd(move, forward);
            if (input.IsKeyDown(Key::S)) move = XMVectorSubtract(move, forward);
            if (input.IsKeyDown(Key::D)) move = XMVectorAdd(move, right);
            if (input.IsKeyDown(Key::A)) move = XMVectorSubtract(move, right);
            if (input.IsKeyDown(Key::Space)) move = XMVectorAdd(move, XMVectorSet(0, 1, 0, 0));

            if (!XMVector3Equal(move, XMVectorZero()))
            {
                move = XMVector3Normalize(move);
                move = XMVectorScale(move, speed);
                XMVECTOR pos = XMVectorAdd(XMLoadFloat3(&tf.position), move);
                XMStoreFloat3(&tf.position, pos);
            }

            // Store rotation in TransformComponent as quaternion from basis
            // Build quaternion from forward (look) and up: derive rotation matrix then quaternion
            XMMATRIX basis;
            basis.r[0] = XMVectorSet(XMVectorGetX(right), XMVectorGetY(right), XMVectorGetZ(right), 0.0f);
            basis.r[1] = XMVectorSet(XMVectorGetX(up), XMVectorGetY(up), XMVectorGetZ(up), 0.0f);
            basis.r[2] = XMVectorSet(XMVectorGetX(forward), XMVectorGetY(forward), XMVectorGetZ(forward), 0.0f);
            basis.r[3] = XMVectorSet(0, 0, 0, 1);

            XMVECTOR q = XMQuaternionRotationMatrix(basis);
            q = XMQuaternionNormalize(q);
            XMStoreFloat4(&tf.rotation, q);
        }
    }


    void CameraMatrixSystem(Engine::Scene& scene,
                            ID3D11DeviceContext* context,
                            ID3D11Buffer* cbView,
                            ID3D11Buffer* cbProj)
    {
        // Get active camera entity
        const entt::entity cam = scene.m_activeRenderCamera;
        if (cam == entt::null || !scene.registry.valid(cam)) return;
        if (!scene.registry.all_of<TransformComponent, CameraComponent, ViewportComponent>(cam)) return;

        const auto& tf = scene.registry.get<TransformComponent>(cam);   // camera transfor
        const auto& camc = scene.registry.get<CameraComponent>(cam);    // camera component
        const auto& vp = scene.registry.get<ViewportComponent>(cam);    // viewport component

        // Build camera world matrix from TransformComponent
        const XMMATRIX S = XMMatrixScaling(tf.scale.x, tf.scale.y, tf.scale.z);
        XMVECTOR qn = XMLoadFloat4(&tf.rotation);
        qn = XMQuaternionNormalize(qn);
        const XMMATRIX R = XMMatrixRotationQuaternion(qn);
        const XMMATRIX T = XMMatrixTranslation(tf.position.x, tf.position.y, tf.position.z);
        const XMMATRIX world = S * R * T;

        // View matrix (LH): look-to using basis and position
        const XMMATRIX view = XMMatrixInverse(nullptr, world);

        // Projection matrix (LH)
        const float aspect = static_cast<float>(vp.width) / static_cast<float>(vp.height ? vp.height : 1u);
        const XMMATRIX proj = XMMatrixPerspectiveFovLH(camc.FOV, aspect, camc.nearClip, camc.farClip);

        // Upload to GPU
        if (cbView) UpdateMatrixCB(context, cbView, view);
        if (cbProj) UpdateMatrixCB(context, cbProj, proj);
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