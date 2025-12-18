#include "Engine/Systems.h"
#include "Engine/Components.h"
#include "Engine/Renderer.h"
#include "Engine/MeshManager.h"
#include "Engine/PhysicsManager.h"
#include <DirectXMath.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <random> // jitter for ball spawn

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
            fc.yaw += static_cast<float>(md.dx) * fc.lookSensitivity;
            fc.pitch += static_cast<float>(md.dy) * fc.lookSensitivity * -1.0f; // default invertY

            // Clamp and wrap
            const float kPitchLimit = XMConvertToRadians(89.0f);
            if (fc.pitch > kPitchLimit) fc.pitch = kPitchLimit;
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
                    ld.position = ltTf.position;  // used by point/spot
                    ld.range = lt.range;       // attenuation range for point/spot
                    ld.direction = fwd;            // used by directional/spot
                    ld.spotAngle = lt.spotAngle;
                    ld.color = lt.color;
                    ld.intensity = lt.intensity;
                    ld.type = static_cast<unsigned int>(lt.type);
                    ld.padding = XMFLOAT3(0.0f, 0.0f, 0.0f);

                    lc.lights[lc.lightCount] = ld;
                    lc.lightCount++;
                }

                // If no light present, push a default directional light
                if (lc.lightCount == 0)
                {
                    Engine::LightData ld{};
                    ld.position = XMFLOAT3(0, 0, 0);
                    ld.range = 10.0f;
                    ld.direction = XMFLOAT3(0.0f, -1.0f, 0.0f);
                    ld.spotAngle = XM_PIDIV4;
                    ld.color = XMFLOAT3(1.0f, 1.0f, 1.0f);
                    ld.intensity = 1.0f;
                    ld.type = static_cast<unsigned int>(Engine::LightType::Directional);
                    ld.padding = XMFLOAT3(0.0f, 0.0f, 0.0f);
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

            // Auto-wire meshID if missing
            if (rb.shape == RBShape::Mesh && rb.meshID == 0 && scene.registry.all_of<MeshRendererComponent>(ent)) {
                rb.meshID = scene.registry.get<MeshRendererComponent>(ent).meshID;
            }

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


    // Galton board helpers and system


    namespace
    {
        constexpr float kBallRadius = 0.07f;
        constexpr float kPegRadius = 0.12f;
        constexpr float kPegSpacingX = 0.5f;
        constexpr float kPegSpacingY = 0.5f;

        // Build a grid of static pegs (capsules), glass walls (thin boxes) and bin separators (boxes),
        // plus a top funnel to guide balls.
        static void SetupBoard(Engine::Scene& scene, Engine::PhysicsManager& physMan, Engine::MeshManager& meshMan, Engine::Renderer& renderer)
        {
            // Material defaults
            const int materialID = 1; // basic shader

            // Peg field
            {
                const int rows = 20;
                const int cols = 16;
                const float halfWidth = (cols * kPegSpacingX) / 2.0f;

                for (int r = 0; r < rows; ++r)
                {
                    for (int c = 0; c < cols; ++c)
                    {
                        const float x = (c + (r % 2) * 0.5f) * kPegSpacingX - halfWidth;
                        const float y = 10.0f - r * kPegSpacingY;

                        entt::entity peg = scene.CreateEntity("Peg");
                        auto& tc = scene.registry.get<Engine::TransformComponent>(peg);
                        tc.position = XMFLOAT3(x, y, 0.0f);
                        // Rotate capsule (Y-up) 90 degrees around Z to lie horizontally (pointing along X)
                        XMVECTOR q = XMQuaternionRotationAxis(XMVectorSet(1, 0, 0, 0), XM_PIDIV2);
                        XMStoreFloat4(&tc.rotation, q);
                        tc.scale = XMFLOAT3(1.0f, 1.0f, 1.0f);

                        Engine::MeshRendererComponent rend{};
                        rend.meshID = meshMan.CreateCapsule(renderer.GetDevice(), kPegRadius, 1.0f, 32, 32);
                        rend.materialID = 1;
                        rend.roughness = 0.1f;
                        rend.metallic = 0.2f;
                        scene.registry.emplace<Engine::MeshRendererComponent>(peg, rend);

                        // Physics: Capsule peg (static)
                        Engine::RigidBodyComponent rb{};
                        rb.shape = Engine::RBShape::Capsule;
                        rb.motionType = Engine::RBMotion::Static;
                        rb.radius = kPegRadius;
                        rb.height = 0.2f; // cylinder segment length
                        auto& rbRef = scene.registry.emplace<Engine::RigidBodyComponent>(peg, rb);

                        // Create body now to avoid one-frame latency
                        JPH::BodyID id = physMan.CreateRigidBody(tc, rbRef, meshMan);
                        rbRef.bodyID = id;
                        rbRef.bodyCreated = !id.IsInvalid();
                    }
                }
            }

            // Glass walls to confine motion in a 2D plane (z = +-0.3)
            {
                const float wallZs[2] = { +0.12f, -0.12f };
                for (float wz : wallZs)
                {
                    entt::entity wall = scene.CreateEntity("Glass Wall");
                    auto& tc = scene.registry.get<Engine::TransformComponent>(wall);
                    tc.position = XMFLOAT3(0.0f, 5.0f, wz);
                    tc.scale = XMFLOAT3(40.0f, 40.0f, 0.05f); // wide and tall, very thin
                    // Visual
                    /*Engine::MeshRendererComponent mr{};
                    mr.meshID = 101;
                    mr.materialID = 1;
                    scene.registry.emplace<Engine::MeshRendererComponent>(wall, mr);*/
                    // Physics
                    Engine::RigidBodyComponent rb{};
                    rb.shape = Engine::RBShape::Box;
                    rb.motionType = Engine::RBMotion::Static;
                    auto& rbRef = scene.registry.emplace<Engine::RigidBodyComponent>(wall, rb);
                    rbRef.bodyID = physMan.CreateRigidBody(tc, rbRef, meshMan);
                    rbRef.bodyCreated = !rbRef.bodyID.IsInvalid();
                }
            }

            // Bins at bottom (vertical separators)
            {
                const int cols = 16; // separators count (a little more than peg columns)
                const float width = (15 * kPegSpacingX);
                const float startX = -width * 0.5f;
                for (int i = 0; i <= cols; ++i)
                {
                    const float x = startX + i * (width / cols);
                    entt::entity sep = scene.CreateEntity("Bin Separator");
                    auto& tc = scene.registry.get<Engine::TransformComponent>(sep);
                    tc.position = XMFLOAT3(x, -2.0f, 0.0f);
                    tc.scale = XMFLOAT3(0.1f, 4.0f, 0.2f); // thin, tall
                    // Visual
                    Engine::MeshRendererComponent mr{};
                    mr.meshID = 101;
                    mr.materialID = 1;
                    scene.registry.emplace<Engine::MeshRendererComponent>(sep, mr);
                    // Physics
                    Engine::RigidBodyComponent rb{};
                    rb.shape = Engine::RBShape::Box;
                    rb.motionType = Engine::RBMotion::Static;
                    auto& rbRef = scene.registry.emplace<Engine::RigidBodyComponent>(sep, rb);
                    rbRef.bodyID = physMan.CreateRigidBody(tc, rbRef, meshMan);
                    rbRef.bodyCreated = !rbRef.bodyID.IsInvalid();
                }
            }

            // Funnel at top (two angled static boxes guiding balls to center)
            {
                // Left
                {
                    entt::entity f = scene.CreateEntity("Funnel Left");
                    auto& tc = scene.registry.get<Engine::TransformComponent>(f);
                    tc.position = XMFLOAT3(-3.05f, 11.8f, 0.0f);
                    tc.scale = XMFLOAT3(6.0f, 0.5f, 0.1f);
                    XMVECTOR q = XMQuaternionRotationAxis(XMVectorSet(0, 0, 1, 0), XMConvertToRadians(-25.0f));
                    XMStoreFloat4(&tc.rotation, q);

                    Engine::MeshRendererComponent mr{};
                    mr.meshID = 101;
                    mr.materialID = 1;
                    scene.registry.emplace<Engine::MeshRendererComponent>(f, mr);

                    Engine::RigidBodyComponent rb{};
                    rb.shape = Engine::RBShape::Box;
                    rb.motionType = Engine::RBMotion::Static;
                    auto& rbRef = scene.registry.emplace<Engine::RigidBodyComponent>(f, rb);
                    rbRef.bodyID = physMan.CreateRigidBody(tc, rbRef, meshMan);
                    rbRef.bodyCreated = !rbRef.bodyID.IsInvalid();
                }
                // Right
                {
                    entt::entity f = scene.CreateEntity("Funnel Right");
                    auto& tc = scene.registry.get<Engine::TransformComponent>(f);
                    tc.position = XMFLOAT3(3.05f, 11.8f, 0.0f);
                    tc.scale = XMFLOAT3(6.0f, 0.5f, 0.1f);
                    XMVECTOR q = XMQuaternionRotationAxis(XMVectorSet(0, 0, 1, 0), XMConvertToRadians(+25.0f));
                    XMStoreFloat4(&tc.rotation, q);

                    Engine::MeshRendererComponent mr{};
                    mr.meshID = 101;
                    mr.materialID = 1;
                    scene.registry.emplace<Engine::MeshRendererComponent>(f, mr);

                    Engine::RigidBodyComponent rb{};
                    rb.shape = Engine::RBShape::Box;
                    rb.motionType = Engine::RBMotion::Static;
                    auto& rbRef = scene.registry.emplace<Engine::RigidBodyComponent>(f, rb);
                    rbRef.bodyID = physMan.CreateRigidBody(tc, rbRef, meshMan);
                    rbRef.bodyCreated = !rbRef.bodyID.IsInvalid();
                }

                // Spout (vertical guides) below the angled ramps to create a narrow opening centered at X=0
                //{
                //    // Left side of spout
                //    entt::entity sL = scene.CreateEntity("Funnel Spout Left");
                //    auto& tcL = scene.registry.get<Engine::TransformComponent>(sL);
                //    tcL.position = XMFLOAT3(-0.35f, 10.0f, 0.0f);
                //    tcL.scale    = XMFLOAT3(0.2f, 2.0f, 0.1f);

                //    Engine::MeshRendererComponent mr{};
                //    mr.meshID = 101;
                //    mr.materialID = 1;
                //    scene.registry.emplace<Engine::MeshRendererComponent>(sL, mr);

                //    Engine::RigidBodyComponent rb{};
                //    rb.shape = Engine::RBShape::Box;
                //    rb.motionType = Engine::RBMotion::Static;
                //    auto& rbRefL = scene.registry.emplace<Engine::RigidBodyComponent>(sL, rb);
                //    rbRefL.bodyID = physMan.CreateRigidBody(tcL, rbRefL, meshMan);
                //    rbRefL.bodyCreated = !rbRefL.bodyID.IsInvalid();

                //    // Right side of spout
                //    entt::entity sR = scene.CreateEntity("Funnel Spout Right");
                //    auto& tcR = scene.registry.get<Engine::TransformComponent>(sR);
                //    tcR.position = XMFLOAT3(+0.35f, 12.0f, 0.0f);
                //    tcR.scale    = XMFLOAT3(0.2f, 3.0f, 0.1f);

                //    scene.registry.emplace<Engine::MeshRendererComponent>(sR, mr);

                //    auto& rbRefR = scene.registry.emplace<Engine::RigidBodyComponent>(sR, rb);
                //    rbRefR.bodyID = physMan.CreateRigidBody(tcR, rbRefR, meshMan);
                //    rbRefR.bodyCreated = !rbRefR.bodyID.IsInvalid();
                //}

                // lower floor
                {
                    entt::entity f = scene.CreateEntity("Funnel bottom");
                    auto& tc = scene.registry.get<Engine::TransformComponent>(f);
                    tc.position = XMFLOAT3(0.0f, -3.0f, 0.0f);
                    tc.scale = XMFLOAT3(10.0f, 0.5f, 1.0f);
                    /*XMVECTOR q = XMQuaternionRotationAxis(XMVectorSet(0, 0, 1, 0), XMConvertToRadians(-25.0f));
                    XMStoreFloat4(&tc.rotation, q);*/

                    Engine::MeshRendererComponent mr{};
                    mr.meshID = 101;
                    mr.materialID = 1;
                    scene.registry.emplace<Engine::MeshRendererComponent>(f, mr);

                    Engine::RigidBodyComponent rb{};
                    rb.shape = Engine::RBShape::Box;
                    rb.motionType = Engine::RBMotion::Static;
                    auto& rbRef = scene.registry.emplace<Engine::RigidBodyComponent>(f, rb);
                    rbRef.bodyID = physMan.CreateRigidBody(tc, rbRef, meshMan);
                    rbRef.bodyCreated = !rbRef.bodyID.IsInvalid();
                }

                // wall left
                {
                    entt::entity f = scene.CreateEntity("wall left");
                    auto& tc = scene.registry.get<Engine::TransformComponent>(f);
                    tc.position = XMFLOAT3(-4.f, 5.0f, 0.0f);
                    tc.scale = XMFLOAT3(0.5f, 17.0f, 1.0f);
                    /*XMVECTOR q = XMQuaternionRotationAxis(XMVectorSet(0, 0, 1, 0), XMConvertToRadians(-25.0f));
                    XMStoreFloat4(&tc.rotation, q);*/

                    Engine::MeshRendererComponent mr{};
                    mr.meshID = 101;
                    mr.materialID = 1;
                    scene.registry.emplace<Engine::MeshRendererComponent>(f, mr);

                    Engine::RigidBodyComponent rb{};
                    rb.shape = Engine::RBShape::Box;
                    rb.motionType = Engine::RBMotion::Static;
                    auto& rbRef = scene.registry.emplace<Engine::RigidBodyComponent>(f, rb);
                    rbRef.bodyID = physMan.CreateRigidBody(tc, rbRef, meshMan);
                    rbRef.bodyCreated = !rbRef.bodyID.IsInvalid();
                }

                // wall right
                {
                    entt::entity f = scene.CreateEntity("wall right");
                    auto& tc = scene.registry.get<Engine::TransformComponent>(f);
                    tc.position = XMFLOAT3(4.f, 5.0f, 0.0f);
                    tc.scale = XMFLOAT3(0.5f, 17.0f, 1.0f);
                    /*XMVECTOR q = XMQuaternionRotationAxis(XMVectorSet(0, 0, 1, 0), XMConvertToRadians(-25.0f));
                    XMStoreFloat4(&tc.rotation, q);*/

                    Engine::MeshRendererComponent mr{};
                    mr.meshID = 101;
                    mr.materialID = 1;
                    scene.registry.emplace<Engine::MeshRendererComponent>(f, mr);

                    Engine::RigidBodyComponent rb{};
                    rb.shape = Engine::RBShape::Box;
                    rb.motionType = Engine::RBMotion::Static;
                    auto& rbRef = scene.registry.emplace<Engine::RigidBodyComponent>(f, rb);
                    rbRef.bodyID = physMan.CreateRigidBody(tc, rbRef, meshMan);
                    rbRef.bodyCreated = !rbRef.bodyID.IsInvalid();
                }
            }
        }

        // Grid-spawn balls to avoid interpenetration and reduce explosions
        static void SpawnBalls(int count, Engine::Scene& scene, Engine::PhysicsManager& physMan, Engine::MeshManager& meshMan, Engine::Renderer& renderer)
        {
            static std::mt19937 rng{ std::random_device{}() };
            // Small jitter so they don't stack perfectly on the same contact points
            std::uniform_real_distribution<float> jitter(-0.5f, 0.5f);

            const int materialID = 1;
            const int BallsPerRow = 20;
            const float cell = 0.25f;   // spacing per cell (>= diameter + margin)
            const float halfRowWidth = (BallsPerRow * cell) * 0.5f;

            // Cache ball mesh once (avoid uploading N identical meshes to the GPU)
            static int ballMeshID = -1;
            if (ballMeshID == -1)
            {
                // 16 slices/stacks is enough for small balls; keeps vertex count low
                ballMeshID = meshMan.CreateSphere(renderer.GetDevice(), kBallRadius, 16, 16);
            }

            for (int i = 0; i < count; ++i)
            {
                const int row = i / BallsPerRow;
                const int col = i % BallsPerRow;

                const float x = (col * cell) - halfRowWidth + jitter(rng);
                const float y = 12.0f + (row * cell);
                const float z = 0.0f;

                entt::entity e = scene.CreateEntity("Ball");
                auto& tc = scene.registry.get<Engine::TransformComponent>(e);
                tc.position = XMFLOAT3(x, y, z);
                tc.scale    = XMFLOAT3(1.0f, 1.0f, 1.0f);   // no post-create transform edits

                // Visual mesh matches physics radius for consistency (single cached mesh)
                Engine::MeshRendererComponent rend{};
                rend.meshID = ballMeshID;
                rend.materialID = materialID;
                scene.registry.emplace<Engine::MeshRendererComponent>(e, rend);

                // Physics: dynamic sphere
                Engine::RigidBodyComponent rb{};
                rb.shape = Engine::RBShape::Sphere;
                rb.motionType = Engine::RBMotion::Dynamic;
                rb.radius = kBallRadius;
                rb.mass = 1.0f;
				rb.friction = 0.0f;
				rb.restitution = 0.5f;
				rb.linearDamping = 0.5f;
                auto& rbRef = scene.registry.emplace<Engine::RigidBodyComponent>(e, rb);

                // Create body immediately
                rbRef.bodyID = physMan.CreateRigidBody(tc, rbRef, meshMan);
                rbRef.bodyCreated = !rbRef.bodyID.IsInvalid();
            }
        }
    }

    void GaltonBoardSystem(Engine::Scene& scene, Engine::PhysicsManager& physMan, Engine::MeshManager& meshMan, const Engine::InputManager& input, Engine::Renderer& renderer, float /*dt*/)
    {
        static bool initialized = false;
        if (!initialized)
        {
            SetupBoard(scene, physMan, meshMan, renderer);
            initialized = true;
        }

        // Edge-triggered key presses using IsKeyDown with local state
        static bool prevA = false, prevS = false, prevD = false, prevR = false;

        const bool aNow = input.IsKeyDown(Key::A);
        const bool sNow = input.IsKeyDown(Key::S);
        const bool dNow = input.IsKeyDown(Key::D);
        const bool rNow = input.IsKeyDown(Key::R);

        if (aNow && !prevA) SpawnBalls(100, scene, physMan, meshMan, renderer);
        if (sNow && !prevS) SpawnBalls(500, scene, physMan, meshMan, renderer);
        if (dNow && !prevD) SpawnBalls(1000, scene, physMan, meshMan, renderer);

        if (rNow && !prevR)
        {
            // Clear only dynamic bodies
            std::vector<entt::entity> toDestroy;
            auto view = scene.registry.view<Engine::RigidBodyComponent>();
            for (auto e : view)
            {
                auto& rb = view.get<Engine::RigidBodyComponent>(e);
                if (rb.motionType == Engine::RBMotion::Dynamic)
                {
                    if (!rb.bodyID.IsInvalid())
                    {
                        physMan.RemoveRigidBody(rb.bodyID);
                        rb.bodyID = JPH::BodyID();
                        rb.bodyCreated = false;
                    }
                    toDestroy.push_back(e);
                }
            }
            for (auto e : toDestroy)
            {
                if (scene.registry.valid(e))
                    scene.registry.destroy(e);
            }
        }

        prevA = aNow; prevS = sNow; prevD = dNow; prevR = rNow;
    }
}