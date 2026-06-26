#include "Engine/Systems.h"
#include "Engine/Components.h"
#include "Engine/Renderer.h"
#include "Engine/MeshManager.h"
#include "Engine/PhysicsManager.h"
#include "Engine/ShaderManager.h"
#include "Engine/InputManager.h"
#include "Engine/AudioManager.h"
#include "Engine/TextureManager.h"
#include "Engine/ScriptEntity.h"
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
        tc.isDirty = true; // Local transform changed; force world matrix recompute
    }


    void EditorCameraInputSystem(Engine::Scene& scene, const Engine::InputManager& input, float dt, bool isSceneFocused)
    {
        // Iterate all entities with Transform + FlyCamControl
        auto view = scene.registry.view<TransformComponent, EditorCamControlComponent>();
        for (auto ent : view)
        {
            auto& tf = view.get<TransformComponent>(ent);           // transform
			auto& fc = view.get<EditorCamControlComponent>(ent);    // flycam control

            if (fc.mode != CameraControlMode::EditorCam)
                continue;

            if (input.IsMouseCaptured())
            {
                // Mouse look: accumulate yaw/pitch
                const auto md = input.GetMouseDelta();
                fc.yaw   += static_cast<float>(md.dx) * fc.lookSensitivity;
                fc.pitch += static_cast<float>(md.dy) * fc.lookSensitivity * -1.0f; // default invertY

                // Clamp and wrap
                constexpr float kPitchLimit = XMConvertToRadians(89.0f);
                if (fc.pitch >  kPitchLimit) fc.pitch =  kPitchLimit;
                if (fc.pitch < -kPitchLimit) fc.pitch = -kPitchLimit;
                if (fc.yaw > XM_PI)  fc.yaw -= XM_2PI;
                if (fc.yaw < -XM_PI) fc.yaw += XM_2PI;
            }

            // Recompute basis from yaw/pitch (LH, yaw=0 looks +Z)
            const float cy = cosf(fc.yaw);
            const float sy = sinf(fc.yaw);
            const float cp = cosf(fc.pitch);
            const float sp = sinf(fc.pitch);

            XMVECTOR forward = XMVector3Normalize(XMVectorSet(sy * cp, sp, cy * cp, 0.0f));
            XMVECTOR worldUp = XMVectorSet(0, 1, 0, 0);
            XMVECTOR right = XMVector3Normalize(XMVector3Cross(worldUp, forward));
            XMVECTOR up = XMVector3Normalize(XMVector3Cross(forward, right));

            float scroll = static_cast<float>(input.GetMouseDelta().wheelY);
            if (isSceneFocused && scroll != 0.0f)
            {
                // Multiply by a factor (e.g., 5.0f) so the scroll movement is noticeable
                float scrollSpeed = fc.moveSpeed * dt * 5.0f; 
                XMVECTOR move = XMVectorScale(forward, scroll * scrollSpeed);
                XMVECTOR pos = XMVectorAdd(XMLoadFloat3(&tf.position), move);
                XMStoreFloat3(&tf.position, pos);
                tf.isDirty = true; // moved
            }

            if (input.IsMouseCaptured() || (isSceneFocused && input.IsKeyDown(Key::LShift)))
            {
                // Movement: W/A/S/D + Shift, Space up
                float speed = fc.moveSpeed * dt;
                if (input.IsKeyDown(Key::LShift)) speed *= fc.sprintMultiplier;

                XMVECTOR move = XMVectorZero();
                if (input.IsKeyDown(Key::W)) move = XMVectorAdd(move, forward);
                if (input.IsKeyDown(Key::S)) move = XMVectorSubtract(move, forward);
                if (input.IsKeyDown(Key::D)) move = XMVectorAdd(move, right);
                if (input.IsKeyDown(Key::A)) move = XMVectorSubtract(move, right);
                //if (input.IsKeyDown(Key::Space)) move = XMVectorAdd(move, XMVectorSet(0, 1, 0, 0));

                if (!XMVector3Equal(move, XMVectorZero()))
                {
                    move = XMVector3Normalize(move);
                    move = XMVectorScale(move, speed);
                    XMVECTOR pos = XMVectorAdd(XMLoadFloat3(&tf.position), move);
                    XMStoreFloat3(&tf.position, pos);
                    tf.isDirty = true; // moved
                }
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
            tf.isDirty = true; // rotated
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


    void TransformSystem(Engine::Scene& scene)
    {
        auto& registry = scene.registry;

        const XMMATRIX identity = XMMatrixIdentity();

        // recursive helper; parentChanged cascades down even if child isn't dirty
        const auto Recurse = [&](auto&& self, entt::entity e, const XMMATRIX& parentWorld, bool parentChanged) -> void
        {
            if (!registry.valid(e) || !registry.all_of<TransformComponent>(e))
                return;

            auto& tc = registry.get<TransformComponent>(e);

            const bool dirty = tc.isDirty || parentChanged;

            // Local = Scale * Rotation * Translation.
            const XMMATRIX S = XMMatrixScaling(tc.scale.x, tc.scale.y, tc.scale.z);
            XMVECTOR qn = XMLoadFloat4(&tc.rotation);
            qn = XMQuaternionNormalize(qn);
            const XMMATRIX R = XMMatrixRotationQuaternion(qn);
            const XMMATRIX T = XMMatrixTranslation(tc.position.x, tc.position.y, tc.position.z);
            const XMMATRIX local = S * R * T;

            // If the entity has a parent, World = Local * ParentWorld. Otherwise, World = Local.
            XMMATRIX world = local;
            if (registry.all_of<RelationshipComponent>(e))
            {
                const auto& rel = registry.get<RelationshipComponent>(e);
                if (rel.parent != entt::null)
                {
                    world = local * parentWorld;
                }
            }

            XMStoreFloat4x4(&tc.worldMatrix, world);
            tc.isDirty = false;

            if (registry.all_of<RelationshipComponent>(e))
            {
                const auto& rel = registry.get<RelationshipComponent>(e);
                entt::entity child = rel.firstChild;

                while (child != entt::null)
                {
                    self(self, child, world, dirty);

                    if (!registry.valid(child) || !registry.all_of<RelationshipComponent>(child))
                        break;

                    child = registry.get<RelationshipComponent>(child).nextSibling;
                }
            }
        };

        // Iterate through all entities that have a TransformComponent.
        // Filter for Roots: Only process entities that either DO NOT have a RelationshipComponent,
        // OR have a RelationshipComponent where parent == entt::null.
        auto view = registry.view<TransformComponent>();
        for (auto e : view)
        {
            if (registry.all_of<RelationshipComponent>(e))
            {
                const auto& rel = registry.get<RelationshipComponent>(e);
                if (rel.parent != entt::null)
                    continue; // not a root
            }

            Recurse(Recurse, e, identity, false);
        }
    }


    namespace RenderSystem
    {
        void DrawEntities(Engine::Scene& scene, MeshManager& meshManager, ShaderManager& shaderManager, Engine::Renderer& renderer, Engine::TextureManager& textureManager)
        {
            auto* context = renderer.GetContext();

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

                    // Respect master entity toggle (skip inactive entities entirely)
                    if (scene.registry.all_of<NameComponent>(lightEnt)) {
                        if (!scene.registry.get<NameComponent>(lightEnt).isActive) continue;
                    }

                    const auto& ltTf = lightView.get<TransformComponent>(lightEnt);
                    const auto& lt = lightView.get<LightComponent>(lightEnt);

                    // Respect component active toggle
                    if (!lt.isActive) continue;

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
                // Respect master entity toggle (skip inactive entities entirely)
                if (scene.registry.all_of<NameComponent>(entity)) {
                    if (!scene.registry.get<NameComponent>(entity).isActive) continue;
                }

                auto& mr = view.get<MeshRendererComponent>(entity);
                auto& tc = view.get<TransformComponent>(entity);

                // Respect component active toggle
                if (!mr.isActive) continue;

                // World matrix from cached TransformComponent
                DirectX::XMMATRIX world = DirectX::XMLoadFloat4x4(&tc.worldMatrix);
                renderer.UpdateWorldMatrix(world);

                // Fetch mesh buffers
                MeshBuffers buffers{};
                if (!meshManager.GetMesh(mr.meshID, buffers))
                    continue;

                if (mr.matType == Engine::MaterialType::UnlitColor)
                {
                    // Bind the Unlit Shader
                    renderer.BindShader(shaderManager, scene.GetDebugShaderID());

                    // Update the unlit color buffer (b4)
                    renderer.UpdateColorConstants(mr.baseColor);

                    // Submit and draw
                    ID3D11InputLayout* layout = shaderManager.GetInputLayout(scene.GetDebugShaderID());
                    renderer.SubmitMesh(buffers, layout);
                    renderer.DrawIndexed(buffers.indexCount);
                }
                else if (mr.matType == Engine::MaterialType::LitColor)
                {
                    // Bind the PBR Shader
                    renderer.BindShader(shaderManager, scene.GetDefaultShaderID());

                    // Bind a default 1x1 white texture so the color isn't tinted black
                    ID3D11ShaderResourceView* defaultTex = textureManager.GetDefaultTexture();
                    context->PSSetShaderResources(0, 1, &defaultTex);

                    // Update PBR material constants (PS b4)
                    {
                        Engine::MaterialConstants mat{};
                        mat.baseColor = mr.baseColor;
                        mat.roughness = mr.roughness;
                        mat.metallic  = mr.metallic;
                        renderer.UpdateMaterialConstants(mat);
                    }

                    // Submit and draw
                    ID3D11InputLayout* layout = shaderManager.GetInputLayout(scene.GetDefaultShaderID());
                    renderer.SubmitMesh(buffers, layout);
                    renderer.DrawIndexed(buffers.indexCount);
                }
                else if (mr.matType == Engine::MaterialType::Textured)
                {
                    // Bind the PBR Shader
                    renderer.BindShader(shaderManager, scene.GetDefaultShaderID());

                    // Bind mesh.texture (if valid, else bind default white texture).
                    if (mr.texture)
                    {
                        context->PSSetShaderResources(0, 1, &mr.texture);
                    }
                    else
                    {
                        ID3D11ShaderResourceView* defaultTex = textureManager.GetDefaultTexture();
                        context->PSSetShaderResources(0, 1, &defaultTex);
                    }

                    // Update the material buffer with baseColor white, so texture shows accurately
                    {
                        Engine::MaterialConstants mat{};
                        mat.baseColor = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
                        mat.roughness = mr.roughness;
                        mat.metallic  = mr.metallic;
                        renderer.UpdateMaterialConstants(mat);
                    }

                    // Submit and draw
                    ID3D11InputLayout* layout = shaderManager.GetInputLayout(scene.GetDefaultShaderID());
                    renderer.SubmitMesh(buffers, layout);
                    renderer.DrawIndexed(buffers.indexCount);
                }
            }
        }

        void DrawDebugColliders(Engine::Scene& scene, Engine::Renderer& renderer, MeshManager& meshManager, ShaderManager& shaderManager, entt::entity selectedEntity)
        {
            if (selectedEntity == entt::null || !scene.registry.valid(selectedEntity)) return;
            if (!scene.registry.all_of<TransformComponent, RigidBodyComponent>(selectedEntity)) return;

            auto& tc = scene.registry.get<TransformComponent>(selectedEntity);
            auto& rb = scene.registry.get<RigidBodyComponent>(selectedEntity);
            if (!rb.isActive) return;

			if (!rb.showWireframe) return;

            // Enable Wireframe
            renderer.SetWireframeMode(true);

            // Bind the debug Unlit shader
            renderer.BindShader(shaderManager, scene.GetDebugShaderID());

            // Set bright green
            renderer.UpdateColorConstants(DirectX::XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f));

            // Calculate Scale based purely on Physics dimensions
            DirectX::XMFLOAT3 debugScale = {1.0f, 1.0f, 1.0f};
            int debugMeshID = 0;

            if (rb.shape == RBShape::Box) {
                // Assuming base cube mesh is 1x1x1, scale by halfExtent * 2
                debugScale = { rb.halfExtent.x * 2.0f, rb.halfExtent.y * 2.0f, rb.halfExtent.z * 2.0f };
                debugMeshID = scene.GetCubeMeshID();
            }
            else if (rb.shape == RBShape::Sphere) {
                // Assuming base sphere has diameter 1 (radius 0.5), scale by radius * 2
                debugScale = { rb.radius * 2.0f, rb.radius * 2.0f, rb.radius * 2.0f };
                debugMeshID = scene.GetSphereMeshID();
            }
            else if (rb.shape == RBShape::Capsule) {
                // Map capsule radius and height to the capsule mesh bounds
                debugScale = { rb.radius * 2.0f, rb.height, rb.radius * 2.0f };
                debugMeshID = scene.GetCapsuleMeshID();
            }
            else if (rb.shape == Engine::RBShape::Mesh) {
				// For mesh colliders, we can use the entity's transform scale multiplied by the collider scale to visualize the collider size. This assumes the base mesh is also 1x1x1.
                debugScale = { tc.scale.x * rb.colliderScale.x,
                   tc.scale.y * rb.colliderScale.y,
                   tc.scale.z * rb.colliderScale.z };
                debugMeshID = rb.meshID;
            }

            // Update World Matrix and Draw
            if (debugMeshID != 0) {
                DirectX::XMMATRIX local =
                    DirectX::XMMatrixScaling(debugScale.x, debugScale.y, debugScale.z) *
                    DirectX::XMMatrixRotationQuaternion(DirectX::XMLoadFloat4(&tc.rotation)) *
                    DirectX::XMMatrixTranslation(tc.position.x, tc.position.y, tc.position.z);

                DirectX::XMMATRIX parentWorld = DirectX::XMMatrixIdentity();
                if (scene.registry.all_of<RelationshipComponent>(selectedEntity)) {
                    entt::entity parent = scene.registry.get<RelationshipComponent>(selectedEntity).parent;
                    if (parent != entt::null && scene.registry.all_of<TransformComponent>(parent)) {
                        parentWorld = DirectX::XMLoadFloat4x4(&scene.registry.get<TransformComponent>(parent).worldMatrix);
                    }
                }

                renderer.UpdateWorldMatrix(local * parentWorld);

                MeshBuffers buffers{};
                if (meshManager.GetMesh(debugMeshID, buffers)) {
                    ID3D11InputLayout* layout = shaderManager.GetInputLayout(scene.GetDebugShaderID());
                    renderer.SubmitMesh(buffers, layout);
                    renderer.DrawIndexed(buffers.indexCount);
                }
            }

            // 5. Restore Solid Mode
            renderer.SetWireframeMode(false);
        }
    }


	// --- PHYSICS SYSTEM ---

    // Helpers: convert Jolt types to DirectX
    static inline XMFLOAT3 FromJolt(const JPH::Vec3& v) {
        return XMFLOAT3(v.GetX(), v.GetY(), v.GetZ());
    }

    static inline XMFLOAT4 FromJolt(const JPH::Quat& q) {
        return XMFLOAT4(q.GetX(), q.GetY(), q.GetZ(), q.GetW());
    }

    void PhysicsSystem(Engine::Scene& scene, Engine::PhysicsManager& physicsManager, const Engine::MeshManager& meshManager, float dt, bool isPlaying)
    {
        // Phase 1: Initialization (Create Bodies) + Maintenance (Destroy bodies for inactive entities/components)
        auto physView = scene.registry.view<TransformComponent, RigidBodyComponent>();
        for (auto ent : physView)
        {
            auto& tc = physView.get<TransformComponent>(ent);
            auto& rb = physView.get<RigidBodyComponent>(ent);

            bool isEntityActive = scene.registry.all_of<NameComponent>(ent) ? scene.registry.get<NameComponent>(ent).isActive : true;

            // If either the entity is off, or the component is off, destroy the Jolt body
            if (!rb.isActive || !isEntityActive) {
                if (!rb.bodyID.IsInvalid()) {
                    physicsManager.RemoveRigidBody(rb.bodyID);
                    rb.bodyID = JPH::BodyID();
                    rb.bodyCreated = false;
                }
                continue;
            }

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

		// Only update physics and sync transforms if we're in Play mode. 
        // In Edit mode, we allow free Transform editing without physics interference, and we push those changes to Jolt so colliders stay in sync with gizmo movements.
        if (isPlaying)
        {
            // Phase 2: Simulation
            physicsManager.Update(dt);

            // Phase 3: Synchronization (Jolt -> ECS)
            JPH::BodyInterface& bi = physicsManager.GetBodyInterface();
            for (auto ent : physView)
            {
                auto& tc = physView.get<TransformComponent>(ent);
                auto& rb = physView.get<RigidBodyComponent>(ent);

                bool isEntityActive = scene.registry.all_of<NameComponent>(ent) ? scene.registry.get<NameComponent>(ent).isActive : true;
                if (!rb.isActive || !isEntityActive) continue;

                // Skip invalid bodies
                if (rb.bodyID.IsInvalid()) continue;

                const JPH::Vec3 pos = bi.GetPosition(rb.bodyID);
                const JPH::Quat rot = bi.GetRotation(rb.bodyID);

                // Jolt returns World Space position and rotation.
                // Convert it back into Local Space if the entity has a parent.

                // Extract correct world scale from the current matrix to avoid shrinking
                DirectX::XMVECTOR currentS, currentR, currentT;
                DirectX::XMMatrixDecompose(&currentS, &currentR, &currentT, DirectX::XMLoadFloat4x4(&tc.worldMatrix));
                DirectX::XMFLOAT3 worldScale;
                DirectX::XMStoreFloat3(&worldScale, currentS);

                DirectX::XMMATRIX joltWorld = DirectX::XMMatrixScaling(worldScale.x, worldScale.y, worldScale.z) *
                    DirectX::XMMatrixRotationQuaternion(DirectX::XMVectorSet(rot.GetX(), rot.GetY(), rot.GetZ(), rot.GetW())) *
                    DirectX::XMMatrixTranslation(pos.GetX(), pos.GetY(), pos.GetZ());

                DirectX::XMMATRIX local = joltWorld;

                // Check if it has a parent
                if (scene.registry.all_of<RelationshipComponent>(ent)) {
                    entt::entity parent = scene.registry.get<RelationshipComponent>(ent).parent;
                    if (parent != entt::null && scene.registry.all_of<TransformComponent>(parent)) {

                        DirectX::XMMATRIX parentWorld = DirectX::XMLoadFloat4x4(&scene.registry.get<TransformComponent>(parent).worldMatrix);

                        // Prevent 1-frame ECS lag: If parent is also a physics body, query Jolt for its exact current-frame transform
                        if (scene.registry.all_of<RigidBodyComponent>(parent)) {
                            auto& pRb = scene.registry.get<RigidBodyComponent>(parent);
                            if (!pRb.bodyID.IsInvalid()) {
                                JPH::Vec3 pPos = bi.GetPosition(pRb.bodyID);
                                JPH::Quat pRot = bi.GetRotation(pRb.bodyID);

                                DirectX::XMVECTOR pS, pR, pT;
                                DirectX::XMMatrixDecompose(&pS, &pR, &pT, DirectX::XMLoadFloat4x4(&scene.registry.get<TransformComponent>(parent).worldMatrix));
                                DirectX::XMFLOAT3 pScale;
                                DirectX::XMStoreFloat3(&pScale, pS);

                                parentWorld = DirectX::XMMatrixScaling(pScale.x, pScale.y, pScale.z) *
                                    DirectX::XMMatrixRotationQuaternion(DirectX::XMVectorSet(pRot.GetX(), pRot.GetY(), pRot.GetZ(), pRot.GetW())) *
                                    DirectX::XMMatrixTranslation(pPos.GetX(), pPos.GetY(), pPos.GetZ());
                            }
                        }

                        DirectX::XMVECTOR det;
                        DirectX::XMMATRIX parentInv = DirectX::XMMatrixInverse(&det, parentWorld);
                        local = joltWorld * parentInv;
                    }
                }

                DirectX::XMVECTOR s, r, t;
                DirectX::XMMatrixDecompose(&s, &r, &t, local);

                DirectX::XMStoreFloat3(&tc.position, t);
                DirectX::XMStoreFloat4(&tc.rotation, r);
                tc.isDirty = true; // Ensure TransformSystem cascades the update
            }
        }
        else
        {
            // Edit Mode: Sync ECS -> Jolt (Push Gizmo movements to physics colliders)
            for (auto ent : physView)
            {
                auto& tc = physView.get<TransformComponent>(ent);
                auto& rb = physView.get<RigidBodyComponent>(ent);

                bool isEntityActive = scene.registry.all_of<NameComponent>(ent) ? scene.registry.get<NameComponent>(ent).isActive : true;
                if (!rb.isActive || !isEntityActive) continue;

                if (!rb.bodyID.IsInvalid()) {
                    // Push World Space to Jolt (do NOT use local tc.position / tc.rotation)
                    DirectX::XMVECTOR worldS, worldR, worldT;
                    DirectX::XMMatrixDecompose(&worldS, &worldR, &worldT, DirectX::XMLoadFloat4x4(&tc.worldMatrix));

                    DirectX::XMFLOAT3 worldPos{};
                    DirectX::XMFLOAT4 worldRot{};
                    DirectX::XMStoreFloat3(&worldPos, worldT);
                    DirectX::XMStoreFloat4(&worldRot, worldR);

                    physicsManager.GetBodyInterface().SetPositionAndRotation(
                        rb.bodyID,
                        JPH::Vec3(worldPos.x, worldPos.y, worldPos.z),
                        JPH::Quat(worldRot.x, worldRot.y, worldRot.z, worldRot.w),
                        JPH::EActivation::Activate
                    );

                    // match old behavior: killing momentum happens inside ResetBodyTransform()
                    physicsManager.ResetBodyTransform(tc, rb, meshManager);
                }
            }
        }
    }


	// --- SCRIPT SYSTEM ---
    // Initialize, Update, Shutdown

    void ScriptSystemInit(Engine::Scene& scene) {
        auto view = scene.registry.view<LuaScriptComponent>();

        // Cache entities to a vector to prevent iterator invalidation if scripts spawn entities during OnCreate
        std::vector<entt::entity> activeEntities(view.begin(), view.end());

        for (auto entity : activeEntities) {
            if (!scene.registry.valid(entity)) continue;

			// Iterate through each script for this entity
            size_t count = scene.registry.get<LuaScriptComponent>(entity).scripts.size();
            for (size_t i = 0; i < count; ++i) {

                // Block scope to limit reference lifespan
                {
					// Re-fetch the script reference to ensure we have the latest data in case of memory reallocations
                    auto& script = scene.registry.get<LuaScriptComponent>(entity).scripts[i];
                    if (script.filepath.empty() || script.isInitialized) continue;

                    try {
						// Create an isolated Lua environment for this specific entity
                        script.env = sol::environment(scene.m_lua, sol::create, scene.m_lua.globals());

						// Inject the 'self' wrapper so the script knows which entity it belongs to
                        script.env["self"] = ScriptEntity(entity, &scene);

						// Compile and run the file into this environment
                        scene.m_lua.script_file(script.filepath, script.env);

						// Cache lifecycle functions to avoid string lookups per frame
                        script.OnCreate = script.env["OnCreate"];
                        script.OnUpdate = script.env["OnUpdate"];
                        script.OnDestroy = script.env["OnDestroy"];

                        if (script.OnCreate.valid()) {
							// Copy to stack before execution (in case OnCreate spawns new entities or scripts and causes reallocations)
                            sol::function createFunc = script.OnCreate;
                            createFunc();
                        }
                    }
                    catch (const sol::error& e) {
                        std::fprintf(stderr, "Lua Init Error in %s: %s\n", scene.registry.get<LuaScriptComponent>(entity).scripts[i].filepath.c_str(), e.what());
                    }
                }

                // Safely update initialization state after potential memory reallocations
                scene.registry.get<LuaScriptComponent>(entity).scripts[i].isInitialized = true;
            }
        }
    }

    void ScriptSystemUpdate(Engine::Scene& scene, float dt) {
        auto view = scene.registry.view<LuaScriptComponent>();

		// Cache entities to prevent iterator invalidation if scripts spawn new entities during OnUpdate
        std::vector<entt::entity> activeEntities(view.begin(), view.end());

        for (auto entity : activeEntities) {
            if (!scene.registry.valid(entity)) continue;

			// Use an index loop and re-fetch the component reference each iteration.
			// This ensures we survive EnTT pool memory reallocations caused by runtime instantiation.
            size_t scriptCount = scene.registry.get<LuaScriptComponent>(entity).scripts.size();
            for (size_t i = 0; i < scriptCount; ++i) {

                // ON-THE-FLY INITIALIZATION
				// allows scripts to be attached at runtime and still have their OnCreate called before the first OnUpdate
                {
                    auto& script = scene.registry.get<LuaScriptComponent>(entity).scripts[i];
                    if (!script.isInitialized && !script.filepath.empty()) {
                        try {
                            script.env = sol::environment(scene.m_lua, sol::create, scene.m_lua.globals());
                            script.env["self"] = ScriptEntity(entity, &scene);
                            scene.m_lua.script_file(script.filepath, script.env);

                            script.OnCreate = script.env["OnCreate"];
                            script.OnUpdate = script.env["OnUpdate"];
                            script.OnDestroy = script.env["OnDestroy"];

                            if (script.OnCreate.valid()) {
                                sol::function createFunc = script.OnCreate;
                                createFunc();
                            }
                        }
                        catch (const sol::error& e) {
                            std::fprintf(stderr, "Lua Init Error in %s: %s\n", scene.registry.get<LuaScriptComponent>(entity).scripts[i].filepath.c_str(), e.what());
							scene.registry.get<LuaScriptComponent>(entity).scripts[i].filepath = "";    // Clear to prevent infinite error spam
                            continue;
                        }
                        scene.registry.get<LuaScriptComponent>(entity).scripts[i].isInitialized = true;
                    }
                }

                // UPDATE LOOP
				// Re-fetch reference in case OnCreate spawned something and reallocated memory
                {
                    auto& safeScript = scene.registry.get<LuaScriptComponent>(entity).scripts[i];
                    if (safeScript.isInitialized && safeScript.OnUpdate.valid()) {

                        // Copy the Lua function and filepath to the C++ stack before executing
                        // This guarantees safety even if the EnTT pool is deleted/moved mid-execution.
                        sol::function updateFunc = safeScript.OnUpdate;
                        std::string filepath = safeScript.filepath;

                        try {
                            updateFunc(dt);
                        }
                        catch (const sol::error& e) {
                            std::fprintf(stderr, "Lua Update Error in %s: %s\n", filepath.c_str(), e.what());
                        }
                    }
                }
            }
        }
    }

    void ScriptSystemShutdown(Engine::Scene& scene) {
        auto view = scene.registry.view<LuaScriptComponent>();
        std::vector<entt::entity> activeEntities(view.begin(), view.end());

        for (auto entity : activeEntities) {
            if (!scene.registry.valid(entity)) continue;

            size_t count = scene.registry.get<LuaScriptComponent>(entity).scripts.size();
            for (size_t i = 0; i < count; ++i) {

				// Block scope to limit reference lifespan
                {
                    auto& script = scene.registry.get<LuaScriptComponent>(entity).scripts[i];
                    if (script.isInitialized) {
                        if (script.OnDestroy.valid()) {
                            sol::function destroyFunc = script.OnDestroy;
                            std::string filepath = script.filepath;
                            try {
                                destroyFunc();
                            }
                            catch (const sol::error& e) {
                                std::fprintf(stderr, "Lua Destroy Error in %s: %s\n", filepath.c_str(), e.what());
                            }
                        }
                    }
                }

				// Reset state so it cleanly reloads next time Play is pressed
                auto& resetScript = scene.registry.get<LuaScriptComponent>(entity).scripts[i];
                resetScript.isInitialized = false;
                resetScript.env = sol::environment();
                resetScript.OnCreate = sol::function();
                resetScript.OnUpdate = sol::function();
                resetScript.OnDestroy = sol::function();
            }
        }
    }


	// --- AUDIO SYSTEM ---

    void AudioSystem(Engine::Scene& scene, Engine::AudioManager& audioManager)
    {
        // Update Listener (Ears) using Active Camera
        if (scene.m_activeRenderCamera != entt::null && scene.registry.valid(scene.m_activeRenderCamera))
        {
            if (scene.registry.all_of<TransformComponent>(scene.m_activeRenderCamera))
            {
                auto& camTc = scene.registry.get<TransformComponent>(scene.m_activeRenderCamera);

                // Calculate Forward Vector from camera's rotation quaternion
                DirectX::XMVECTOR q = DirectX::XMLoadFloat4(&camTc.rotation);
                DirectX::XMVECTOR forward = DirectX::XMVector3Rotate(DirectX::XMVectorSet(0, 0, 1, 0), q);

                audioManager.SetListenerPosition(
                    camTc.position.x, camTc.position.y, camTc.position.z,
                    DirectX::XMVectorGetX(forward), DirectX::XMVectorGetY(forward), DirectX::XMVectorGetZ(forward)
                );
            }
        }

        // Update Audio Emitters
        auto view = scene.registry.view<AudioComponent, TransformComponent>();
        for (auto entity : view)
        {
            auto& ac = view.get<AudioComponent>(entity);
            auto& tc = view.get<TransformComponent>(entity);

            bool isEntityActive = scene.registry.all_of<NameComponent>(entity) ? scene.registry.get<NameComponent>(entity).isActive : true;

            // Stop playback if entity is disabled
            if (!isEntityActive) {
                if (ac.soundHandle && ac.isPlaying) {
                    audioManager.StopAudio(ac.soundHandle);
                    ac.isPlaying = false;
                }
                continue;
            }

            // Initialize sound handle dynamically
            if (ac.soundHandle == nullptr && !ac.filepath.empty())
            {
                ac.soundHandle = audioManager.LoadSound(ac.filepath, ac.is3D, ac.loop);
            }

            // Play if requested
            if (ac.soundHandle && ac.playOnCreate && !ac.isPlaying)
            {
                audioManager.PlayAudio(ac.soundHandle);
                ac.isPlaying = true;
            }

            // Sync 3D Position
            if (ac.soundHandle && ac.is3D)
            {
                // Decompose world matrix for true absolute world position
                DirectX::XMVECTOR s, r, t;
                DirectX::XMMatrixDecompose(&s, &r, &t, DirectX::XMLoadFloat4x4(&tc.worldMatrix));
                audioManager.SetAudioPosition(ac.soundHandle, DirectX::XMVectorGetX(t), DirectX::XMVectorGetY(t), DirectX::XMVectorGetZ(t));
            }

            // Sync Volume
            if (ac.soundHandle)
            {
                audioManager.SetAudioVolume(ac.soundHandle, ac.volume);
            }
        }
    }
}