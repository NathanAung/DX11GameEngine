#include "Engine/Systems.h"
#include "Engine/Components.h"
#include "Engine/Renderer.h"
#include "Engine/MeshManager.h"
#include "Engine/PhysicsManager.h"
#include <DirectXMath.h>
#include <Jolt/Physics/Body/BodyInterface.h>

using namespace DirectX;

namespace Engine
{
    void DemoRotationSystem(Engine::Scene& scene, entt::entity sampleEntity, float dt)
    {
        if (sampleEntity == entt::null) return;
        if (!scene.registry.valid(sampleEntity)) return;

        // get TransformComponent
        auto& tc = scene.registry.get<TransformComponent>(sampleEntity);

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
            auto& tf = view.get<TransformComponent>(ent);           // transform
			auto& fc = view.get<EditorCamControlComponent>(ent);    // flycam control

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

			// Convert to quaternion
            XMVECTOR q = XMQuaternionRotationMatrix(basis);
            q = XMQuaternionNormalize(q);
            XMStoreFloat4(&tf.rotation, q);
        }
    }


    void CameraMatrixSystem(Engine::Scene& scene, Engine::Renderer& renderer)
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

		// Upload to renderer
        renderer.UpdateViewMatrix(view);
        renderer.UpdateProjectionMatrix(proj);
    }


    namespace RenderSystem
    {
        void DrawEntities(Engine::Scene& scene, MeshManager& meshManager, ShaderManager& shaderManager, Engine::Renderer& renderer)
        {
            auto* context = renderer.GetContext();

            // Bind basic shaders (temporary ID 1)
            renderer.BindShader(shaderManager, 1);

            // Bind sampler to PS s0 once per frame
            ID3D11SamplerState* sampler = renderer.GetSamplerState();
            if (sampler)
            {
                context->PSSetSamplers(0, 1, &sampler);
            }

            // Global lights update: collect up to MAX_LIGHTS
            {
                Engine::LightConstants lc{};
                lc.lightCount = 0;

                // Camera position for specular calculations
                if (scene.m_activeRenderCamera != entt::null &&
                    scene.registry.valid(scene.m_activeRenderCamera) &&
                    scene.registry.all_of<TransformComponent>(scene.m_activeRenderCamera))
                {
                    const auto& camTf = scene.registry.get<TransformComponent>(scene.m_activeRenderCamera);
                    lc.cameraPos = camTf.position;
                }
                else
                {
                    lc.cameraPos = XMFLOAT3(0.0f, 0.0f, -100.0f);
                }

                // Search for light entities and extract info
                auto lightView = scene.registry.view<TransformComponent, LightComponent>();
                for (auto lightEnt : lightView)
                {
                    if (lc.lightCount >= MAX_LIGHTS) break;

                    const auto& ltTf = lightView.get<TransformComponent>(lightEnt);
                    const auto& lt = lightView.get<LightComponent>(lightEnt);

                    // Direction: forward vector from quaternion rotated +Z (LH)
					// forward is used because directional light shines along its forward axis
                    XMVECTOR q = XMLoadFloat4(&ltTf.rotation);
                    q = XMQuaternionNormalize(q);
                    XMVECTOR forward = XMVector3Rotate(XMVectorSet(0, 0, 1, 0), q);
                    XMFLOAT3 fwd{};
                    XMStoreFloat3(&fwd, XMVector3Normalize(forward));

                    // Fill per-light data
                    Engine::LightData ld{};
                    ld.position  = ltTf.position;  // used by point/spot
                    ld.range     = lt.range;       // attenuation range for point/spot
                    ld.direction = fwd;            // used by directional/spot
                    ld.spotAngle = lt.spotAngle;
                    ld.color     = lt.color;
                    ld.intensity = lt.intensity;
                    ld.type      = static_cast<unsigned int>(lt.type);
                    ld.padding   = XMFLOAT3(0.0f, 0.0f, 0.0f);

                    lc.lights[lc.lightCount] = ld;
                    lc.lightCount++;
                }

                // If no light present, push a default directional light
                if (lc.lightCount == 0)
                {
                    Engine::LightData ld{};
                    ld.position  = XMFLOAT3(0,0,0);
                    ld.range     = 10.0f;
                    ld.direction = XMFLOAT3(0.0f, -1.0f, 0.0f);
                    ld.spotAngle = XM_PIDIV4;
                    ld.color     = XMFLOAT3(1.0f, 1.0f, 1.0f);
                    ld.intensity = 1.0f;
                    ld.type      = static_cast<unsigned int>(Engine::LightType::Directional);
                    ld.padding   = XMFLOAT3(0.0f, 0.0f, 0.0f);
                    lc.lights[0] = ld;
                    lc.lightCount = 1;
                }

                // Upload & bind PS b3
                renderer.UpdateLightConstants(lc);
            }

            // Iterate renderable entities (assuming MeshRendererComponent and TransformComponent exist)
            auto view = scene.registry.view<MeshRendererComponent, TransformComponent>();
            for (auto entity : view)
            {
                auto& mr = view.get<MeshRendererComponent>(entity);
                auto& tr = view.get<TransformComponent>(entity);

                // Per-entity material constants (PS b4)
                {
                    Engine::MaterialConstants mat{};
                    mat.roughness = mr.roughness;
                    mat.metallic = mr.metallic;
                    renderer.UpdateMaterialConstants(mat);
                }

                // World matrix from transform (position, rotation, scale)
                XMMATRIX world =
                    XMMatrixScaling(tr.scale.x, tr.scale.y, tr.scale.z) *
                    XMMatrixRotationQuaternion(XMLoadFloat4(&tr.rotation)) *
                    XMMatrixTranslation(tr.position.x, tr.position.y, tr.position.z);
                renderer.UpdateWorldMatrix(world);

                // Bind texture if present to PS t0
                if (mr.texture)
                {
                    context->PSSetShaderResources(0, 1, &mr.texture);
                }

                // Fetch mesh buffers
                MeshBuffers buffers{};
                if (!meshManager.GetMesh(mr.meshID, buffers))
                    continue;

                // Submit and draw
                ID3D11InputLayout* layout = shaderManager.GetInputLayout(mr.materialID);
                renderer.SubmitMesh(buffers, layout);
                renderer.DrawIndexed(buffers.indexCount);

                // unbind texture to avoid hazards with subsequent draws (optional here)
                // ID3D11ShaderResourceView* nullSRV[1] = { nullptr };
                // context->PSSetShaderResources(0, 1, nullSRV);
            }
        }
    }

    // Helpers: convert Jolt types to DirectX
    static inline XMFLOAT3 FromJolt(const JPH::Vec3& v) {
        return XMFLOAT3(v.GetX(), v.GetY(), v.GetZ());
    }

    static inline XMFLOAT4 FromJolt(const JPH::Quat& q) {
        return XMFLOAT4(q.GetX(), q.GetY(), q.GetZ(), q.GetW());
    }

    void PhysicsSystem(Engine::Scene& scene, Engine::PhysicsManager& physicsManager, const Engine::MeshManager& meshManager, float dt)
    {
        // Phase 1: Initialization (Create Bodies)
        auto physView = scene.registry.view<TransformComponent, RigidBodyComponent>();
        for (auto ent : physView)
        {
            auto& tc = physView.get<TransformComponent>(ent);
            auto& rb = physView.get<RigidBodyComponent>(ent);

            if (rb.bodyID.IsInvalid()) {
                JPH::BodyID id = physicsManager.CreateRigidBody(tc, rb, meshManager);
                rb.bodyID = id;
                rb.bodyCreated = !id.IsInvalid();
            }
        }

        // Phase 2: Simulation
        physicsManager.Update(dt);

        // Phase 3: Synchronization (Jolt -> ECS)
        JPH::BodyInterface& bi = physicsManager.GetBodyInterface();
        for (auto ent : physView)
        {
            auto& tc = physView.get<TransformComponent>(ent);
            auto& rb = physView.get<RigidBodyComponent>(ent);

            // Skip statics and invalid bodies
            if (rb.motionType == RBMotion::Static) continue;
            if (rb.bodyID.IsInvalid()) continue;

            const JPH::Vec3 pos = bi.GetPosition(rb.bodyID);
            const JPH::Quat rot = bi.GetRotation(rb.bodyID);

            tc.position = FromJolt(pos);
            tc.rotation = FromJolt(rot);
        }
    }
}