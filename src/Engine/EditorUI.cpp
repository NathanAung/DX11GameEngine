#include "Engine/EditorUI.h"
#include "Engine/Components.h"
#include "Engine/MathUtils.h"
#include "Engine/PhysicsManager.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <ImGuizmo.h>
#include <SDL.h>
#include <DirectXCollision.h>

namespace Engine
{
    void EditorUI::Render(Engine::Scene& scene, Engine::Renderer& renderer, Engine::InputManager& input, Engine::PhysicsManager& physicsManager, SDL_Window* window)
    {
        ImGuizmo::BeginFrame();

        // 1. Setup variables for the Dockspace
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGuiID dockspace_id = ImGui::GetID("EditorDockspace");

        // 2. Set Dockspace flags and styling to match the viewport
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::SetNextWindowViewport(viewport->ID);

        ImGuiWindowFlags host_window_flags = 0;
        host_window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
        host_window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

        // 3. Create the invisible background window to host the dockspace
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::Begin("DockSpace Window", nullptr, host_window_flags);
        ImGui::PopStyleVar(3);

        // 4. Submit the actual DockSpace
        ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);

        // 5. Build the Default Layout (Only runs once, if the layout is completely empty/new)
        static bool first_time = true;
        if (first_time)
        {
            first_time = false;

            // Clear out existing layout for this dockspace
            ImGui::DockBuilderRemoveNode(dockspace_id);
            ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
            ImGui::DockBuilderSetNodeSize(dockspace_id, viewport->Size);

            // Split the dockspace mathematically
            ImGuiID dock_main_id = dockspace_id;
            ImGuiID dock_id_left = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Left, 0.20f, nullptr, &dock_main_id);
            ImGuiID dock_id_right = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Right, 0.25f, nullptr, &dock_main_id);
            ImGuiID dock_id_bottom = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Down, 0.30f, nullptr, &dock_main_id);

            // Assign panels to the newly created splits
            ImGui::DockBuilderDockWindow("Hierarchy", dock_id_left);
            ImGui::DockBuilderDockWindow("Inspector", dock_id_right);
            ImGui::DockBuilderDockWindow("Content Browser", dock_id_bottom);
            ImGui::DockBuilderDockWindow("Scene", dock_main_id); // Scene takes whatever is left in the center

            ImGui::DockBuilderFinish(dockspace_id);
        }

        // End the DockSpace Window host
        ImGui::End();

        // SCENE WINDOW
        // Scene View (dockable): drives the render-to-texture size
        ImGui::Begin("Scene");
        // Get the available size for the viewport (this is the size of the content region inside the "Scene" window)
        ImVec2 viewportSize = ImGui::GetContentRegionAvail();

        // Update the active camera's viewport size if it doesn't match the current viewport size
        if (scene.m_activeRenderCamera != entt::null &&
            scene.registry.valid(scene.m_activeRenderCamera) &&
            scene.registry.all_of<Engine::ViewportComponent>(scene.m_activeRenderCamera))
        {
            auto& vp = scene.registry.get<Engine::ViewportComponent>(scene.m_activeRenderCamera);

            if (viewportSize.x != vp.width || viewportSize.y != vp.height)
            {
                if (viewportSize.x > 0.0f && viewportSize.y > 0.0f)
                {
                    vp.width = viewportSize.x;
                    vp.height = viewportSize.y;

                    renderer.CreateFramebuffer((UINT)viewportSize.x, (UINT)viewportSize.y);
                }
            }
        }

        // Capture the exact screen position BEFORE drawing the image to avoid title bar offsets
        ImVec2 imagePos = ImGui::GetCursorScreenPos();

        // Render the framebuffer texture
        ImGui::Image((ImTextureID)(intptr_t)renderer.GetFramebufferSRV(), viewportSize);

        // Configure ImGuizmo to perfectly overlay the rendered image
        ImGuizmo::SetOrthographic(false);
        ImGuizmo::SetDrawlist();
        ImGuizmo::AllowAxisFlip(false);
        ImGuizmo::SetRect(imagePos.x, imagePos.y, viewportSize.x, viewportSize.y);

        // Generate Screen-to-World ray based on the active camera using the same math as CameraMatrixSystem
        // NOTE: ImGuizmo needs the camera matrices every single frame to render handles.
        DirectX::XMMATRIX view = DirectX::XMMatrixIdentity();
        DirectX::XMMATRIX proj = DirectX::XMMatrixIdentity();
        DirectX::XMFLOAT4X4 view4x4{}, proj4x4{};

        // Generate Screen-to-World ray based on the active camera using the same math as CameraMatrixSystem
        if (scene.m_activeRenderCamera != entt::null &&
            scene.registry.valid(scene.m_activeRenderCamera) &&
            scene.registry.all_of<Engine::TransformComponent, Engine::CameraComponent>(scene.m_activeRenderCamera))
        {
            const auto& tf = scene.registry.get<Engine::TransformComponent>(scene.m_activeRenderCamera);
            const auto& camc = scene.registry.get<Engine::CameraComponent>(scene.m_activeRenderCamera);

            DirectX::XMVECTOR qn = DirectX::XMQuaternionNormalize(DirectX::XMLoadFloat4(&tf.rotation));
            const DirectX::XMMATRIX R = DirectX::XMMatrixRotationQuaternion(qn);
            const DirectX::XMMATRIX T = DirectX::XMMatrixTranslation(tf.position.x, tf.position.y, tf.position.z);

            // Strip scale from the camera's world matrix to prevent view matrix distortion
            const DirectX::XMMATRIX cameraWorldNoScale = R * T;
            view = DirectX::XMMatrixInverse(nullptr, cameraWorldNoScale);

            // Projection matrix (LH): use current Scene viewport aspect
            const float aspect = (viewportSize.y != 0.0f) ? (viewportSize.x / viewportSize.y) : 1.0f;
            proj = DirectX::XMMatrixPerspectiveFovLH(camc.FOV, aspect, camc.nearClip, camc.farClip);
        }

		// Store the camera matrices in XMFLOAT4X4 format for ImGuizmo
        DirectX::XMStoreFloat4x4(&view4x4, view);
        DirectX::XMStoreFloat4x4(&proj4x4, proj);

		// Handle mouse click in the Scene panel to generate a Screen-to-World ray for potential object picking
        if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGuizmo::IsOver())
        {
            ImVec2 mousePos = ImGui::GetMousePos();
            ImVec2 screenPos = ImGui::GetItemRectMin();

            float localX = mousePos.x - screenPos.x;
            float localY = mousePos.y - screenPos.y;

            auto ray = Engine::Math::ScreenToWorldRay(localX, localY, viewportSize.x, viewportSize.y, view, proj);

            entt::entity hitEntity = physicsManager.CastRay(ray, scene.registry);

            // FALLBACK: If physics didn't hit anything, test mathematical bounding boxes
            if (hitEntity == entt::null)
            {
				// Look through all entities with a TransformComponent and test against their Oriented Bounding Box (OBB)
                float closestDistance = FLT_MAX;
                auto view = scene.registry.view<Engine::TransformComponent>();

                for (auto entity : view)
                {
                    // Skip entities that have a RigidBody (physics already tested them and missed)
                    if (scene.registry.all_of<Engine::RigidBodyComponent>(entity))
                        continue;

                    // Skip the camera we are currently looking through to prevent self-intersection
                    if (entity == scene.m_activeRenderCamera)
                        continue;

                    auto& tc = view.get<Engine::TransformComponent>(entity);

                    // Construct an Oriented Bounding Box (OBB) from the TransformComponent
                    DirectX::BoundingOrientedBox obb;
                    obb.Center = tc.position;
                    // Assuming a standard 1x1x1 unit volume, half-extents are 0.5 * scale
                    obb.Extents = DirectX::XMFLOAT3(tc.scale.x * 0.5f, tc.scale.y * 0.5f, tc.scale.z * 0.5f);
                    obb.Orientation = tc.rotation;

                    // Test for intersection
                    float distance = 0.0f;
                    if (obb.Intersects(DirectX::XMLoadFloat3(&ray.origin), DirectX::XMLoadFloat3(&ray.direction), distance))
                    {
                        // Keep track of the closest intersected entity
                        if (distance < closestDistance)
                        {
                            closestDistance = distance;
                            hitEntity = entity;
                        }
                    }
                }
            }

            // Update selection if we hit something (either via physics or OBB)
            if (hitEntity != entt::null)
            {
                m_selectedEntity = hitEntity;
            }
        }

        // Smart Input Routing: Right-Click to Fly (Scene panel only)
        {
            // Check if the Scene panel is hovered for input routing
            bool isHovered = ImGui::IsWindowHovered();
            m_scenePanelFocused = ImGui::IsWindowFocused();

            if (m_scenePanelFocused && ImGui::IsKeyPressed(ImGuiKey_Space))
            {
                // Cycle between 0 (Translate), 1 (Rotate), and 2 (Scale)
                m_gizmoType = (m_gizmoType + 1) % 3;
            }

            // Store mouse position on RMB down to restore it on release (prevents warping issues if cursor leaves panel while flying)
            static int storedMouseX = 0;
            static int storedMouseY = 0;

            // When right mouse button is clicked while hovering the Scene panel, capture the mouse for camera control
            if (isHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
            {
                SDL_GetMouseState(&storedMouseX, &storedMouseY);
                input.SetMouseCaptured(true);
            }

            // Release capture when RMB is released (even if cursor left the panel while dragging)
            if (!ImGui::IsMouseDown(ImGuiMouseButton_Right) && input.IsMouseCaptured())
            {
                input.SetMouseCaptured(false);
                SDL_WarpMouseInWindow(window, storedMouseX, storedMouseY);
            }
        }

        // Draw Gizmo if an entity is selected
        if (m_selectedEntity != entt::null && scene.registry.valid(m_selectedEntity) && scene.registry.all_of<Engine::TransformComponent>(m_selectedEntity))
        {
            auto& tc = scene.registry.get<Engine::TransformComponent>(m_selectedEntity);

            // Map m_gizmoType to ImGuizmo Operation
            ImGuizmo::OPERATION op = ImGuizmo::TRANSLATE;
            if (m_gizmoType == 1) op = ImGuizmo::ROTATE;
            if (m_gizmoType == 2) op = ImGuizmo::SCALE;

            // Build the selected entity's world matrix
            DirectX::XMMATRIX S = DirectX::XMMatrixScaling(tc.scale.x, tc.scale.y, tc.scale.z);
            DirectX::XMVECTOR qn = DirectX::XMQuaternionNormalize(DirectX::XMLoadFloat4(&tc.rotation));
            DirectX::XMMATRIX R = DirectX::XMMatrixRotationQuaternion(qn);
            DirectX::XMMATRIX T = DirectX::XMMatrixTranslation(tc.position.x, tc.position.y, tc.position.z);
            DirectX::XMMATRIX world = S * R * T;

            DirectX::XMFLOAT4X4 world4x4;
            DirectX::XMStoreFloat4x4(&world4x4, world);

            // Draw the Gizmo and manipulate the matrix
            ImGuizmo::Manipulate(&view4x4.m[0][0], &proj4x4.m[0][0], op, ImGuizmo::LOCAL, &world4x4.m[0][0]);

            if (ImGuizmo::IsUsing())
            {
                // Use DirectX native decomposition to avoid ImGuizmo's Euler angle bugs
                DirectX::XMMATRIX modifiedWorld = DirectX::XMLoadFloat4x4(&world4x4);
                DirectX::XMVECTOR vScale, vRotQuat, vTrans;
                DirectX::XMMatrixDecompose(&vScale, &vRotQuat, &vTrans, modifiedWorld);

                DirectX::XMStoreFloat3(&tc.position, vTrans);
                DirectX::XMStoreFloat3(&tc.scale, vScale);
                DirectX::XMStoreFloat4(&tc.rotation, vRotQuat);
            }
        }

        ImGui::End();

        // HIERARCHY WINDOW
        ImGui::Begin("Hierarchy");
        {
            // List all entities with a NameComponent in the hierarchy
            auto view = scene.registry.view<Engine::NameComponent>();
            for (auto entity : view)
            {
                auto& nameComp = view.get<Engine::NameComponent>(entity);

                // Set tree node flags: leaf because parent-child relationships are not implemented yet, and span width for better clickability
                ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_SpanAvailWidth;
                // Note: Leaf because entities do not have children yet
                if (m_selectedEntity == entity)
                    flags |= ImGuiTreeNodeFlags_Selected;

                // Use the entity ID as the ImGui tree node ID to ensure uniqueness
                bool opened = ImGui::TreeNodeEx((void*)(uint32_t)entity, flags, "%s", nameComp.name.c_str());
                // Handle selection: clicking on the item selects it
                if (ImGui::IsItemClicked()) { m_selectedEntity = entity; }

                // No child nodes for now, but they would go here
                if (opened) { ImGui::TreePop(); }
            }

            // Deselection: click empty space in the window to clear selection
            if (ImGui::IsMouseDown(0) && ImGui::IsWindowHovered())
            {
                m_selectedEntity = entt::null;
            }
        }
        ImGui::End();

        // INSPECTOR WINDOW
        ImGui::Begin("Inspector");
        {
            if (m_selectedEntity != entt::null && scene.registry.valid(m_selectedEntity))
            {
                // NameComponent UI
                if (scene.registry.all_of<Engine::NameComponent>(m_selectedEntity))
                {
                    auto& nc = scene.registry.get<Engine::NameComponent>(m_selectedEntity);

                    static char buffer[256] = {};
#ifdef _MSC_VER
                    strncpy_s(buffer, nc.name.c_str(), sizeof(buffer) - 1);
#else
                    std::strncpy(buffer, nc.name.c_str(), sizeof(buffer) - 1);
#endif

                    if (ImGui::InputText("Name", buffer, sizeof(buffer)))
                    {
                        nc.name = buffer;
                    }
                }

                // TransformComponent UI
                if (scene.registry.all_of<Engine::TransformComponent>(m_selectedEntity))
                {
                    auto& tc = scene.registry.get<Engine::TransformComponent>(m_selectedEntity);

                    if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen))
                    {
                        ImGui::DragFloat3("Position", &tc.position.x, 0.1f);
                        ImGui::DragFloat3("Scale", &tc.scale.x, 0.1f, 0.01f, 10000.0f);

                        // Static cache to hold UI state between frames
                        static entt::entity s_lastEntity = entt::null;
                        static DirectX::XMFLOAT3 s_cachedEuler{ 0.0f, 0.0f, 0.0f };

                        // 1. Check if the selection changed
                        bool selectionChanged = (s_lastEntity != m_selectedEntity);
                        s_lastEntity = m_selectedEntity;

                        // 2. Check if the quaternion was changed externally (e.g., by Physics)
                        // Compare the actual quaternion against the one generated by cached Euler angles.
                        DirectX::XMFLOAT4 expectedQuat = Engine::Math::EulerDegreesToQuaternion(s_cachedEuler);
                        DirectX::XMVECTOR q1 = DirectX::XMLoadFloat4(&expectedQuat);
                        DirectX::XMVECTOR q2 = DirectX::XMLoadFloat4(&tc.rotation);

                        // Use dot product to check if quaternions are virtually identical
                        float dot = fabs(DirectX::XMVectorGetX(DirectX::XMQuaternionDot(q1, q2)));
                        bool externallyChanged = (dot < 0.9999f);

                        // 3. Update the cache ONLY if selection changed or physics moved the object
                        if (selectionChanged || externallyChanged)
                        {
                            s_cachedEuler = Engine::Math::QuaternionToEulerDegrees(tc.rotation);
                        }

                        // 4. Draw the UI using the stable cached values
                        if (ImGui::DragFloat3("Rotation", &s_cachedEuler.x, 1.0f))
                        {
                            // 5. If the user drags the slider, push the new rotation to the component
                            tc.rotation = Engine::Math::EulerDegreesToQuaternion(s_cachedEuler);
                        }
                    }
                }

                // LightComponent UI
                if (scene.registry.all_of<Engine::LightComponent>(m_selectedEntity))
                {
                    auto& lc = scene.registry.get<Engine::LightComponent>(m_selectedEntity);

                    if (ImGui::CollapsingHeader("Light", ImGuiTreeNodeFlags_DefaultOpen))
                    {
                        ImGui::ColorEdit3("Color", &lc.color.x);
                        ImGui::DragFloat("Intensity", &lc.intensity, 0.1f, 0.0f, 1000.0f);
                        ImGui::DragFloat("Range", &lc.range, 0.5f, 0.0f, 1000.0f);
                    }
                }

                // RigidbodyComponent UI
                if (scene.registry.all_of<Engine::RigidBodyComponent>(m_selectedEntity))
                {
                    auto& rb = scene.registry.get<Engine::RigidBodyComponent>(m_selectedEntity);

                    if (ImGui::CollapsingHeader("RigidBody"))
                    {
                        // Note: these values currently dictate the initial state of the Jolt body.
                        // Changing them at runtime won't update the Jolt body yet for now.
                        ImGui::DragFloat("Mass", &rb.mass, 0.1f, 0.0f, 10000.0f);
                        ImGui::DragFloat("Friction", &rb.friction, 0.01f, 0.0f, 10.0f);
                        ImGui::DragFloat("Restitution", &rb.restitution, 0.01f, 0.0f, 1.0f);
                        ImGui::DragFloat("Linear Damping", &rb.linearDamping, 0.01f, 0.0f, 100.0f);
                    }
                }

                // MeshRendererComponent UI
                if (scene.registry.all_of<Engine::MeshRendererComponent>(m_selectedEntity))
                {
                    auto& mr = scene.registry.get<Engine::MeshRendererComponent>(m_selectedEntity);
                    if (ImGui::CollapsingHeader("Mesh Renderer"))
                    {
                        ImGui::DragFloat("Roughness", &mr.roughness, 0.01f, 0.0f, 1.0f);
                        ImGui::DragFloat("Metallic", &mr.metallic, 0.01f, 0.0f, 1.0f);
                    }
                }
            }
            else {
                ImGui::Text("No entity selected.");
            }
        }
        ImGui::End();

        // CONTENT BROWSER WINDOW
        ImGui::Begin("Content Browser");
        {
            // Back button: only when inside a subfolder (never go above assets root)
            if (m_currentDirectory != m_assetPath)
            {
                if (ImGui::Button("<- Back"))
                {
                    m_currentDirectory = m_currentDirectory.parent_path();
                }
            }

            // Iterate and display the current directory
            for (auto& directoryEntry : std::filesystem::directory_iterator(m_currentDirectory))
            {
                const auto& path = directoryEntry.path();
                std::string filenameString = path.filename().string();

                // Directories: clickable to navigate into
                if (directoryEntry.is_directory())
                {
                    if (ImGui::Selectable(("[DIR] " + filenameString).c_str()))
                    {
                        m_currentDirectory /= path.filename();
                    }
                }
                else
                {
                    // Files: display only for now (drag/drop & open actions later)
                    ImGui::Text("[FILE] %s", filenameString.c_str());
                }
            }
        }
        ImGui::End();
    }
}