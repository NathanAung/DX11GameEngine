#include "Engine/EditorUI.h"
#include "Engine/Components.h"
#include "Engine/MathUtils.h"
#include "Engine/AssetManager.h"
#include "Engine/PhysicsManager.h"
#include "Engine/AudioManager.h"
#include "Engine/InputManager.h"
#include "Engine/Scene.h"
#include "Engine/Renderer.h"
#include "Engine/Systems.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <ImGuizmo.h>
#include <SDL.h>
#include <DirectXCollision.h>

namespace Engine
{
    void EditorUI::DrawEntityNode(Engine::Scene& scene, entt::entity entity, entt::entity& entityToDestroy)
    {
        auto& nameComp = scene.registry.get<NameComponent>(entity);
        auto* relComp = scene.registry.try_get<RelationshipComponent>(entity);

        ImGuiTreeNodeFlags flags = ((m_selectedEntity == entity) ? ImGuiTreeNodeFlags_Selected : 0) | ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;

        bool isLeaf = !relComp || relComp->firstChild == entt::null;
        if (isLeaf) flags |= ImGuiTreeNodeFlags_Leaf;

        bool opened = ImGui::TreeNodeEx((void*)(uint64_t)(uint32_t)entity, flags, "%s", nameComp.name.c_str());

        // Selection logic
        if (ImGui::IsItemClicked()) m_selectedEntity = entity;

        // Right-click menu for deletion
        if (ImGui::BeginPopupContextItem()) {
            if (ImGui::MenuItem("Delete Entity")) {
                if (m_selectedEntity == entity) m_selectedEntity = entt::null;
                entityToDestroy = entity;
            }
            ImGui::EndPopup();
        }

        // DRAG SOURCE (Picking this entity up)
        if (ImGui::BeginDragDropSource()) {
            ImGui::SetDragDropPayload("ENTITY_PAYLOAD", &entity, sizeof(entt::entity));
            ImGui::Text("Move %s", nameComp.name.c_str());
            ImGui::EndDragDropSource();
        }

        // DROP TARGET (Dropping another entity ONTO this one to make it a child)
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENTITY_PAYLOAD")) {
                entt::entity droppedEntity = *(const entt::entity*)payload->Data;
                scene.ParentEntity(droppedEntity, entity);
            }
            ImGui::EndDragDropTarget();
        }

        // Recursively draw children
        if (opened) {
            if (!isLeaf) {
                entt::entity currentChild = relComp->firstChild;
                while (currentChild != entt::null) {
                    entt::entity next = scene.registry.get<RelationshipComponent>(currentChild).nextSibling;
                    DrawEntityNode(scene, currentChild, entityToDestroy);
                    currentChild = next;
                }
            }
            ImGui::TreePop();
        }
    }

    void EditorUI::Render(Engine::Scene& scene, Engine::Renderer& renderer, Engine::InputManager& input, Engine::PhysicsManager& physicsManager, SDL_Window* window)
    {
        // Cache the Editor Camera so we can always revert to it
        if (m_editorCamera == entt::null)
        {
            auto view = scene.registry.view<Engine::EditorCamControlComponent>();
            if (view.begin() != view.end()) {
                m_editorCamera = *view.begin();
            }
        }

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

            // Create a top strip for the toolbar
            ImGuiID dock_id_top = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Up, 0.07f, nullptr, &dock_main_id);

            // Assign panels to the newly created splits
            ImGui::DockBuilderDockWindow("Hierarchy", dock_id_left);
            ImGui::DockBuilderDockWindow("Inspector", dock_id_right);
            ImGui::DockBuilderDockWindow("Content Browser", dock_id_bottom);
            ImGui::DockBuilderDockWindow("Toolbar", dock_id_top);
            ImGui::DockBuilderDockWindow("Scene", dock_main_id); // Scene takes whatever is left in the center

            ImGui::DockBuilderFinish(dockspace_id);
        }

        // End the DockSpace Window host
        ImGui::End();

        // TOOLBAR WINDOW (Play / Stop)

		// Set the window class to prevent docking and resizing, and to remove the tab bar
        ImGuiWindowClass window_class;
        window_class.DockNodeFlagsOverrideSet = ImGuiDockNodeFlags_NoTabBar | ImGuiDockNodeFlags_NoResize;
        ImGui::SetNextWindowClass(&window_class);

		// Style the toolbar to be more compact and visually distinct
        //ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 2));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, ImVec2(0, 0));
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
        auto& colors = ImGui::GetStyle().Colors;
        const auto& buttonHovered = colors[ImGuiCol_ButtonHovered];
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(buttonHovered.x, buttonHovered.y, buttonHovered.z, 0.5f));
        const auto& buttonActive = colors[ImGuiCol_ButtonActive];
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(buttonActive.x, buttonActive.y, buttonActive.z, 0.5f));

        ImGui::Begin("Toolbar", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

        // Center the buttons
        float size = ImGui::GetWindowHeight() / 2;
        ImGui::SetCursorPosX((ImGui::GetWindowContentRegionMax().x * 0.5f) - (size * 0.5f));

        // Play / Stop Logic
        bool isPlaying = m_state == EditorState::Play;
        const char* buttonLabel = isPlaying ? "Stop" : "Play";

		// When the button is clicked, toggle between Play and Edit modes
        if (ImGui::Button(buttonLabel, ImVec2(size * 2.5f, size)))
        {
            if (m_state == EditorState::Edit)
            {
                m_state = EditorState::Play;

                // Snapshot the scene before simulation starts
                scene.CopyToBackup();
                Engine::ScriptSystemInit(scene);

                // Switch to Game Camera
                auto camView = scene.registry.view<Engine::CameraComponent>();
                for (auto entity : camView)
                {
                    // Find the first active camera that is NOT the editor camera
                    if (!scene.registry.all_of<Engine::EditorCamControlComponent>(entity) && scene.registry.get<NameComponent>(entity).isActive)
                    {
                        scene.m_activeRenderCamera = entity;
                        break;
                    }
                }
            }
            else if (m_state == EditorState::Play)
            {
                m_state = EditorState::Edit;

                Engine::ScriptSystemShutdown(scene);
                // Restore the original scene state and reset all physics bodies
                scene.RestoreFromBackup(physicsManager);

                // Revert to Editor Camera
                scene.m_activeRenderCamera = m_editorCamera;
            }
        }

        ImGui::PopStyleVar(1);
        ImGui::PopStyleColor(3);
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

		// Handle mouse click in the Scene panel to generate a Screen-to-World ray for potential object picking (Edit mode only)
        if (m_state == EditorState::Edit && ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGuizmo::IsOver())
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

                    // Skip inactive entities (master toggle)
                    if (scene.registry.all_of<Engine::NameComponent>(entity))
                    {
                        if (!scene.registry.get<Engine::NameComponent>(entity).isActive)
                            continue;
                    }

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

        // Draw Gizmo if an entity is selected (Edit mode only)
        if (m_state == EditorState::Edit &&
            m_selectedEntity != entt::null && scene.registry.valid(m_selectedEntity) && scene.registry.all_of<Engine::TransformComponent>(m_selectedEntity))
        {
            auto& tc = scene.registry.get<Engine::TransformComponent>(m_selectedEntity);

            // Map m_gizmoType to ImGuizmo Operation
            ImGuizmo::OPERATION op = ImGuizmo::TRANSLATE;
            if (m_gizmoType == 1) op = ImGuizmo::ROTATE;
            if (m_gizmoType == 2) op = ImGuizmo::SCALE;

            // 1. Get the current World Matrix from the TransformComponent
            DirectX::XMMATRIX worldMat = DirectX::XMLoadFloat4x4(&tc.worldMatrix);
            DirectX::XMFLOAT4X4 worldMatFloat;
            DirectX::XMStoreFloat4x4(&worldMatFloat, worldMat);

            // 2. Pass the World Matrix to ImGuizmo
            ImGuizmo::Manipulate(
                &view4x4.m[0][0],
                &proj4x4.m[0][0],
                op,
                ImGuizmo::LOCAL,
                &worldMatFloat.m[0][0]
            );

            // 3. If the user is dragging the Gizmo, convert the new World Matrix back to Local Space
            if (ImGuizmo::IsUsing())
            {
                DirectX::XMMATRIX modifiedWorld = DirectX::XMLoadFloat4x4(&worldMatFloat);
                DirectX::XMMATRIX parentWorld = DirectX::XMMatrixIdentity();

                // Check if the entity has a parent
                if (scene.registry.all_of<RelationshipComponent>(m_selectedEntity)) {
                    entt::entity parent = scene.registry.get<RelationshipComponent>(m_selectedEntity).parent;
                    if (parent != entt::null && scene.registry.all_of<TransformComponent>(parent)) {
                        parentWorld = DirectX::XMLoadFloat4x4(&scene.registry.get<TransformComponent>(parent).worldMatrix);
                    }
                }

                // Convert Modified World -> New Local 
                // Math: Local = World * Inverse(ParentWorld)
                DirectX::XMVECTOR det;
                DirectX::XMMATRIX parentInv = DirectX::XMMatrixInverse(&det, parentWorld);
                DirectX::XMMATRIX newLocal = modifiedWorld * parentInv;

                // 4. Decompose the new Local Matrix back into the TransformComponent
                DirectX::XMVECTOR s, r, t;
                DirectX::XMMatrixDecompose(&s, &r, &t, newLocal);

                // Check for gimbal lock / Euler conversion if Euler angles are being cached for the UI here

                DirectX::XMStoreFloat3(&tc.scale, s);
                DirectX::XMStoreFloat4(&tc.rotation, r);
                DirectX::XMStoreFloat3(&tc.position, t);

                tc.isDirty = true; // Force the TransformSystem to recalculate matrices next frame
            }
        }

        ImGui::End();

		// Only show these windows in Edit mode
        if (m_state == EditorState::Edit) {

            // HIERARCHY WINDOW
            ImGui::Begin("Hierarchy");
            {
                // Right-click empty space in the Hierarchy to create entities
                if (ImGui::BeginPopupContextWindow("HierarchyContextMenu", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
                {
                    if (ImGui::BeginMenu("Create New Entity"))
                    {
                        // EMPTY ENTITY
                        if (ImGui::MenuItem("Empty Entity")) {
                            scene.CreateEntity("New Entity");
                        }

						// CAMERA ENTITY
                        if (ImGui::MenuItem("Camera")) {
                            scene.CreateGameCamera("Camera", 1280, 720);
                        }

						// PRIMITIVE SHAPES
                        if (ImGui::BeginMenu("Shapes"))
                        {
                            if (ImGui::MenuItem("Cube")) scene.CreateCube("Cube");
                            if (ImGui::MenuItem("Sphere")) scene.CreateSphere("Sphere");
                            if (ImGui::MenuItem("Capsule")) scene.CreateCapsule("Capsule");
                            ImGui::EndMenu();
                        }

						// LIGHT ENTITIES
                        if (ImGui::BeginMenu("Lights"))
                        {
                            if (ImGui::MenuItem("Directional Light")) scene.CreateDirectionalLight("Directional Light");
                            if (ImGui::MenuItem("Point Light")) scene.CreatePointLight("Point Light", {0, 0, 0}, {1, 1, 1}, 1.0f, 10.0f);
                            if (ImGui::MenuItem("Spot Light")) scene.CreateSpotLight("Spot Light", {0, 0, 0}, {0, 0, 1}, {1, 1, 1}, 1.0f, 10.0f, 0.785f);
                            ImGui::EndMenu();
                        }
                        ImGui::EndMenu();
                    }

                    ImGui::EndPopup();
                }

				// We cannot destroy entities while iterating, so we defer destruction until after the loop
                entt::entity entityToDestroy = entt::null;

                // Find all roots (every entity has a NameComponent)
                auto view = scene.registry.view<NameComponent>();
                for (auto entity : view)
                {
					auto& nameComp = view.get<NameComponent>(entity);

                    // Prevent editor camera from showing in the hierarchy
                    if (scene.registry.all_of<Engine::EditorCamControlComponent>(entity)) continue;

                    // Grey-out inactive entities in the list so state is obvious
                    if (!nameComp.isActive) {
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
                    }

                    auto* rel = scene.registry.try_get<RelationshipComponent>(entity);
                    // Only draw root nodes (no relationship comp, or parent is null)
                    if (!rel || rel->parent == entt::null) {
                        DrawEntityNode(scene, entity, entityToDestroy);
                    }

                    if (!nameComp.isActive) {
                        ImGui::PopStyleColor();
                    }
                }

                // Fill the remaining vertical space with a dummy item to act as a drop zone for unparenting
                ImVec2 availSpace = ImGui::GetContentRegionAvail();
                if (availSpace.y > 0.0f) {
                    ImGui::Dummy(availSpace);
                    if (ImGui::BeginDragDropTarget()) {
                        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENTITY_PAYLOAD")) {
                            entt::entity droppedEntity = *(const entt::entity*)payload->Data;
                            scene.UnparentEntity(droppedEntity);
                        }
                        ImGui::EndDragDropTarget();
                    }
                }

                // Safe deferred destruction
                if (entityToDestroy != entt::null) {
                    scene.DestroyEntity(entityToDestroy, physicsManager);
                }

                // Deselection: click empty space in the window to clear selection
                if (ImGui::IsMouseDown(0) && ImGui::IsWindowHovered())
                {
                    m_selectedEntity = entt::null;
                }

                if (m_selectedEntity != entt::null && scene.registry.valid(m_selectedEntity))
                {
                    if (ImGui::IsWindowFocused() && ImGui::IsKeyPressed(ImGuiKey_Delete))
                    {
                        scene.DestroyEntity(m_selectedEntity, physicsManager);
                        m_selectedEntity = entt::null;
                    }
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
                        auto& nameComp = scene.registry.get<Engine::NameComponent>(m_selectedEntity);

                        // Master active toggle next to name (entity-wide active state)
                        ImGui::Checkbox("##EntityActive", &nameComp.isActive);
                        ImGui::SameLine();

                        static char buffer[256] = {};
#ifdef _MSC_VER
                        strncpy_s(buffer, nameComp.name.c_str(), sizeof(buffer) - 1);
#else
                        std::strncpy(buffer, nameComp.name.c_str(), sizeof(buffer) - 1);
#endif

                        if (ImGui::InputText("##Name", buffer, sizeof(buffer)))
                        {
                            nameComp.name = buffer;
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
								tc.isDirty = true;
                            }

                            // 4. Draw the UI using the stable cached values
                            if (ImGui::DragFloat3("Rotation", &s_cachedEuler.x, 1.0f))
                            {
                                // 5. If the user drags the slider, push the new rotation to the component
                                tc.rotation = Engine::Math::EulerDegreesToQuaternion(s_cachedEuler);
								tc.isDirty = true;
                            }
                        }
                    }

                    // LightComponent UI
                    if (scene.registry.all_of<Engine::LightComponent>(m_selectedEntity))
                    {
                        auto& lc = scene.registry.get<Engine::LightComponent>(m_selectedEntity);

                        ImGui::PushID("Light");
                        ImGui::Checkbox("##Active", &lc.isActive);
                        ImGui::SameLine();
                        bool treeOpen = ImGui::TreeNodeEx("Light", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed);

                        bool removeComponent = false;
                        if (ImGui::BeginPopupContextItem("RemoveMenu")) {
                            if (ImGui::MenuItem("Remove Component")) removeComponent = true;
                            ImGui::EndPopup();
                        }

                        if (treeOpen)
                        {
                            // Fundamental Light type picker
                            const char* lightTypes[] = { "Directional", "Point", "Spot" };
                            // Assuming LightType enum maps 0=Directional, 1=Point, 2=Spot
                            int currentLightIdx = static_cast<int>(lc.type);

                            if (ImGui::Combo("Light Type", &currentLightIdx, lightTypes, IM_ARRAYSIZE(lightTypes)))
                            {
                                lc.type = static_cast<Engine::LightType>(currentLightIdx);
                            }

                            ImGui::ColorEdit3("Color", &lc.color.x);
                            ImGui::DragFloat("Intensity", &lc.intensity, 0.1f, 0.0f, 1000.0f);
                            ImGui::DragFloat("Range", &lc.range, 0.5f, 0.0f, 1000.0f);

                            ImGui::TreePop();
                        }
                        ImGui::PopID();

                        if (removeComponent)
                        {
                            scene.registry.remove<Engine::LightComponent>(m_selectedEntity);
                        }
                    }

                    // RigidbodyComponent UI
                    if (scene.registry.all_of<Engine::RigidBodyComponent>(m_selectedEntity))
                    {
                        auto& rb = scene.registry.get<Engine::RigidBodyComponent>(m_selectedEntity);

                        ImGui::PushID("Rigidbody");
                        ImGui::Checkbox("##Active", &rb.isActive);
                        ImGui::SameLine();
                        bool treeOpen = ImGui::TreeNodeEx("Rigidbody", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed);

                        bool removeComponent = false;
                        if (ImGui::BeginPopupContextItem("RemoveMenu")) {
                            if (ImGui::MenuItem("Remove Component")) removeComponent = true;
                            ImGui::EndPopup();
                        }

                        if (treeOpen)
                        {
                            // Fundamental rigid body settings: Shape + Motion
                            // CRUCIAL: on change, destroy the Jolt body so PhysicsSystem rebuilds it next frame.
                            const char* rbShapes[] = { "Box", "Sphere", "Capsule", "Mesh" };
                            int currentShapeIdx = static_cast<int>(rb.shape);
                            if (ImGui::Combo("Shape", &currentShapeIdx, rbShapes, IM_ARRAYSIZE(rbShapes)))
                            {
                                rb.shape = static_cast<Engine::RBShape>(currentShapeIdx);
                                // Invalidate body to force a rebuild with the new shape
                                if (!rb.bodyID.IsInvalid()) {
                                    physicsManager.RemoveRigidBody(rb.bodyID);
                                    rb.bodyID = JPH::BodyID();
                                    rb.bodyCreated = false;
                                }
                            }

                            const char* rbMotions[] = { "Static", "Dynamic" };
                            int currentMotionIdx = static_cast<int>(rb.motionType);
                            if (ImGui::Combo("Motion Type", &currentMotionIdx, rbMotions, IM_ARRAYSIZE(rbMotions)))
                            {
                                rb.motionType = static_cast<Engine::RBMotion>(currentMotionIdx);
                                // Invalidate body to force a rebuild with the new mass properties
                                if (!rb.bodyID.IsInvalid()) {
                                    physicsManager.RemoveRigidBody(rb.bodyID);
                                    rb.bodyID = JPH::BodyID();
                                    rb.bodyCreated = false;
                                }
                            }

                            // Physical dimensions (force Jolt rebuild on change)
							// NOTE* min/max values are hardcoded for now
                            bool shapeChanged = false;
                            if (rb.shape == Engine::RBShape::Box) {
                                if (ImGui::DragFloat3("Half Extents", &rb.halfExtent.x, 0.05f, 0.01f, 100.0f)) shapeChanged = true;
                            }
                            else if (rb.shape == Engine::RBShape::Sphere) {
                                if (ImGui::DragFloat("Radius", &rb.radius, 0.05f, 0.01f, 100.0f)) shapeChanged = true;
                            }
                            else if (rb.shape == Engine::RBShape::Capsule) {
                                if (ImGui::DragFloat("Radius", &rb.radius, 0.05f, 0.01f, 100.0f)) shapeChanged = true;
                                if (ImGui::DragFloat("Height", &rb.height, 0.05f, 0.01f, 100.0f)) shapeChanged = true;
                            }
                            else if (rb.shape == Engine::RBShape::Mesh) {
                                if (ImGui::DragFloat3("Collider Scale", &rb.colliderScale.x, 0.05f, 0.01f, 100.0f)) shapeChanged = true;
                            }

                            // Force Jolt to rebuild the body with the new dimensions
                            if (shapeChanged && !rb.bodyID.IsInvalid()) {
                                physicsManager.RemoveRigidBody(rb.bodyID);
                                rb.bodyID = JPH::BodyID();
                                rb.bodyCreated = false;
                            }

                            // Mass and Linear Damping: These require a core physical structural rebuild
                            if (ImGui::DragFloat("Mass", &rb.mass, 0.1f, 0.01f, 1000.0f) ||
                                ImGui::DragFloat("Linear Damping", &rb.linearDamping, 0.01f, 0.0f, 1.0f))
                            {
                                if (!rb.bodyID.IsInvalid()) {
                                    physicsManager.RemoveRigidBody(rb.bodyID);
                                    rb.bodyID = JPH::BodyID();
                                    rb.bodyCreated = false;
                                }
                            }

                            // Friction and Restitution: These can be mutated directly via Jolt's API instantly
                            if (ImGui::DragFloat("Friction", &rb.friction, 0.01f, 0.0f, 1.0f))
                            {
                                if (!rb.bodyID.IsInvalid()) {
                                    physicsManager.GetBodyInterface().SetFriction(rb.bodyID, rb.friction);
                                }
                            }

                            if (ImGui::DragFloat("Restitution", &rb.restitution, 0.01f, 0.0f, 1.0f))
                            {
                                if (!rb.bodyID.IsInvalid()) {
                                    physicsManager.GetBodyInterface().SetRestitution(rb.bodyID, rb.restitution);
                                }
                            }

                            ImGui::TreePop();
                        }

                        // Wireframe toggle
						ImGui::Checkbox("Show Wireframe", &rb.showWireframe);

                        ImGui::PopID();

                        if (removeComponent)
                        {
                            // CRITICAL: unregister the Jolt body first to prevent leaks
                            if (!rb.bodyID.IsInvalid())
                            {
                                physicsManager.RemoveRigidBody(rb.bodyID);
                            }
                            scene.registry.remove<Engine::RigidBodyComponent>(m_selectedEntity);
                        }
                    }

                    // MeshRendererComponent UI
                    if (scene.registry.all_of<Engine::MeshRendererComponent>(m_selectedEntity))
                    {
                        auto& mr = scene.registry.get<Engine::MeshRendererComponent>(m_selectedEntity);

                        ImGui::PushID("MeshRenderer");
                        ImGui::Checkbox("##Active", &mr.isActive);
                        ImGui::SameLine();
                        bool treeOpen = ImGui::TreeNodeEx("Mesh Renderer", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed);

                        bool removeComponent = false;
                        if (ImGui::BeginPopupContextItem("RemoveMenu")) {
                            if (ImGui::MenuItem("Remove Component")) removeComponent = true;
                            ImGui::EndPopup();
                        }

                        if (treeOpen)
                        {
                            // Fundamental mesh selection: choose from default primitive meshes
                            const char* meshTypes[] = { "Cube", "Sphere", "Capsule", "Custom"};
                            int currentMeshIdx = -1;
                            if (mr.meshID == scene.GetCubeMeshID()) currentMeshIdx = 0;
                            else if (mr.meshID == scene.GetSphereMeshID()) currentMeshIdx = 1;
                            else if (mr.meshID == scene.GetCapsuleMeshID()) currentMeshIdx = 2;

                            if (ImGui::Combo("Mesh Shape", &currentMeshIdx, meshTypes, IM_ARRAYSIZE(meshTypes)))
                            {
                                if (currentMeshIdx == 0) mr.meshID = scene.GetCubeMeshID();
                                else if (currentMeshIdx == 1) mr.meshID = scene.GetSphereMeshID();
                                else if (currentMeshIdx == 2) mr.meshID = scene.GetCapsuleMeshID();

                            }

                            // Material Type selector
                            const char* matTypes[] = { "LitColor", "UnlitColor", "Textured" };
                            int currentMatIdx = static_cast<int>(mr.matType);
                            if (ImGui::Combo("Material Type", &currentMatIdx, matTypes, IM_ARRAYSIZE(matTypes)))
                            {
                                mr.matType = static_cast<Engine::MaterialType>(currentMatIdx);
                            }

                            if (mr.matType == Engine::MaterialType::LitColor || mr.matType == Engine::MaterialType::UnlitColor)
                            {
                                ImGui::ColorEdit4("Base Color", &mr.baseColor.x);
                            }

                            if (mr.matType == Engine::MaterialType::LitColor || mr.matType == Engine::MaterialType::Textured)
                            {
                                ImGui::DragFloat("Roughness", &mr.roughness, 0.01f, 0.0f, 1.0f);
                                ImGui::DragFloat("Metallic", &mr.metallic, 0.01f, 0.0f, 1.0f);
                            }

                            if (mr.matType == Engine::MaterialType::Textured)
                            {
                                ImGui::TextDisabled("Texture assignment via Drag & Drop coming soon...");
                            }

                            ImGui::TreePop();
                        }
                        ImGui::PopID();

                        if (removeComponent)
                        {
                            scene.registry.remove<Engine::MeshRendererComponent>(m_selectedEntity);
                        }
                    }

                    // AudioComponent UI
                    if (scene.registry.all_of<Engine::AudioComponent>(m_selectedEntity))
                    {
                        auto& ac = scene.registry.get<Engine::AudioComponent>(m_selectedEntity);

                        ImGui::PushID("AudioComponent");
                        bool treeOpen = ImGui::TreeNodeEx("Audio Emitter", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed);

                        bool removeComponent = false;
                        if (ImGui::BeginPopupContextItem("RemoveMenu")) {
                            if (ImGui::MenuItem("Remove Component")) removeComponent = true;
                            ImGui::EndPopup();
                        }

                        if (treeOpen)
                        {
                            std::vector<std::string> audioFiles;
                            audioFiles.push_back("None");

                            std::string audioPath = "assets/audio";
                            if (!std::filesystem::exists(audioPath)) std::filesystem::create_directories(audioPath);

                            for (const auto& entry : std::filesystem::directory_iterator(audioPath)) {
                                if (entry.path().extension() == ".wav" || entry.path().extension() == ".mp3") {
                                    audioFiles.push_back(entry.path().filename().string());
                                }
                            }

                            // Read the string filename back out of the UUID metadata so the UI can display it
                            int currentIndex = 0;
                            std::string currentFilename = "";
                            if (ac.audioID != 0 && scene.GetAssetManager()) {
                                const Engine::AssetMetadata* meta = scene.GetAssetManager()->GetMetadata(ac.audioID);
                                if (meta) {
                                    currentFilename = std::filesystem::path(meta->filepath).filename().string();
                                    for (int i = 1; i < audioFiles.size(); ++i) {
                                        if (audioFiles[i] == currentFilename) { currentIndex = i; break; }
                                    }
                                }
                            }

                            if (ImGui::BeginCombo("Audio File", audioFiles[currentIndex].c_str()))
                            {
                                for (int i = 0; i < audioFiles.size(); i++) {
                                    bool isSelected = (currentIndex == i);
                                    if (ImGui::Selectable(audioFiles[i].c_str(), isSelected)) {
                                        if (currentIndex != i) {
                                            // Assign a UUID based on the dropdown selection
                                            if (i == 0) {
                                                ac.audioID = 0;
                                            }
                                            else {
                                                std::string newPath = audioPath + "/" + audioFiles[i];
                                                if (scene.GetAssetManager()) {
                                                    ac.audioID = scene.GetAssetManager()->ImportAsset(newPath, Engine::AssetType::Audio);
                                                }
                                            }
                                            // Safely clear old handle so it reloads on Play
                                            if (scene.GetAudioManager() && ac.soundHandle) {
                                                scene.GetAudioManager()->DestroyAudio(ac.soundHandle);
                                                ac.soundHandle = nullptr;
                                                ac.isPlaying = false;
                                            }
                                        }
                                    }
                                    if (isSelected) ImGui::SetItemDefaultFocus();
                                }
                                ImGui::EndCombo();
                            }

                            if (ImGui::BeginDragDropTarget()) {
                                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("AUDIO_FILE")) {
                                    const char* droppedPath = (const char*)payload->Data;
                                    // Generate a UUID from the dropped file path
                                    if (scene.GetAssetManager()) {
                                        Engine::UUID newID = scene.GetAssetManager()->ImportAsset(droppedPath, Engine::AssetType::Audio);
                                        if (ac.audioID != newID) {
                                            ac.audioID = newID;
                                            if (scene.GetAudioManager() && ac.soundHandle) {
                                                scene.GetAudioManager()->DestroyAudio(ac.soundHandle);
                                                ac.soundHandle = nullptr;
                                                ac.isPlaying = false;
                                            }
                                        }
                                    }
                                }
                                ImGui::EndDragDropTarget();
                            }

                            bool is3dChanged = ImGui::Checkbox("Is 3D", &ac.is3D);
                            bool loopChanged = ImGui::Checkbox("Looping", &ac.loop);
                            ImGui::Checkbox("Play On Create", &ac.playOnCreate);

                            // Slider clamps value between 0.0f (Mute) and 1.0f (Max)
                            ImGui::SliderFloat("Volume", &ac.volume, 0.0f, 1.0f);

                            // Rebuild handle if physical parameters change
                            if (is3dChanged || loopChanged) {
                                if (scene.GetAudioManager() && ac.soundHandle) {
                                    scene.GetAudioManager()->DestroyAudio(ac.soundHandle);
                                    ac.soundHandle = nullptr;
                                    ac.isPlaying = false;
                                }
                            }

                            ImGui::TreePop();
                        }
                        ImGui::PopID();

                        if (removeComponent) {
                            if (scene.GetAudioManager() && ac.soundHandle) {
                                scene.GetAudioManager()->DestroyAudio(ac.soundHandle);
                            }
                            scene.registry.remove<Engine::AudioComponent>(m_selectedEntity);
                        }
                    }

                    // LuaScriptComponent UI
                    if (scene.registry.all_of<Engine::LuaScriptComponent>(m_selectedEntity))
                    {
                        auto& scriptComp = scene.registry.get<Engine::LuaScriptComponent>(m_selectedEntity);

                        ImGui::PushID("LuaScript");
                        bool treeOpen = ImGui::TreeNodeEx("Lua Scripts", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed);

                        bool removeComponent = false;
                        if (ImGui::BeginPopupContextItem("RemoveMenu")) {
                            if (ImGui::MenuItem("Remove Component")) removeComponent = true;
                            ImGui::EndPopup();
                        }

                        if (treeOpen)
                        {
                            // Scan assets/scripts for .lua files
                            std::vector<std::string> scriptFiles;
                            scriptFiles.push_back("None");  // Option to clear script

                            std::string scriptsPath = "assets/scripts";

                            // Ensure directory exists to prevent crashes
                            if (!std::filesystem::exists(scriptsPath)) {
                                std::filesystem::create_directories(scriptsPath);
                            }

                            for (const auto& entry : std::filesystem::directory_iterator(scriptsPath)) {
                                if (entry.path().extension() == ".lua") {
                                    scriptFiles.push_back(entry.path().filename().string());
                                }
                            }

							// Display each script slot with a dropdown and remove button
                            int scriptToRemove = -1;

							// Iterate through each script slot in the LuaScriptComponent
                            for (size_t s = 0; s < scriptComp.scripts.size(); ++s)
                            {
                                ImGui::PushID(static_cast<int>(s));
                                auto& script = scriptComp.scripts[s];

                                // Find current active script index in the dropdown

                                int currentIndex = 0;
                                std::string currentFilename = "";
                                if (!script.filepath.empty()) {
                                    currentFilename = std::filesystem::path(script.filepath).filename().string();
                                    for (int i = 1; i < scriptFiles.size(); ++i) {
                                        if (scriptFiles[i] == currentFilename) {
                                            currentIndex = i;
                                            break;
                                        }
                                    }
                                }

                                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 30.0f);

								// Dropdown for selecting a script file
                                if (ImGui::BeginCombo("##ScriptFile", scriptFiles[currentIndex].c_str()))
                                {
                                    for (int i = 0; i < scriptFiles.size(); i++)
                                    {
                                        bool isSelected = (currentIndex == i);
                                        if (ImGui::Selectable(scriptFiles[i].c_str(), isSelected))
                                        {
                                            if (i == 0) script.filepath = "";
                                            else script.filepath = scriptsPath + "/" + scriptFiles[i];
                                        }
                                        if (isSelected) ImGui::SetItemDefaultFocus();
                                    }
                                    ImGui::EndCombo();
                                }

								// Drag-and-drop support for assigning scripts directly from the Content Browser
                                if (ImGui::BeginDragDropTarget())
                                {
                                    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("SCRIPT_FILE"))
                                    {
                                        const char* droppedPath = (const char*)payload->Data;
                                        script.filepath = droppedPath;
                                    }
                                    ImGui::EndDragDropTarget();
                                }

                                ImGui::SameLine();

								// Remove button for the script slot
                                if (ImGui::Button("X")) {
                                    scriptToRemove = static_cast<int>(s);
                                }
                                ImGui::PopID();
                            }

							// Remove the script slot if the "X" button was clicked
                            if (scriptToRemove >= 0) {
                                scriptComp.scripts.erase(scriptComp.scripts.begin() + scriptToRemove);
                            }

							// Button to add a new script slot
                            if (ImGui::Button("Add Script Slot", ImVec2(-1, 0))) {
                                scriptComp.scripts.push_back(Engine::ScriptInstance{});
                            }
                            // Allow dropping directly onto the "Add" button to quickly append
                            if (ImGui::BeginDragDropTarget())
                            {
                                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("SCRIPT_FILE"))
                                {
                                    const char* droppedPath = (const char*)payload->Data;
                                    Engine::ScriptInstance newScript;
                                    newScript.filepath = droppedPath;
                                    scriptComp.scripts.push_back(newScript);
                                }
                                ImGui::EndDragDropTarget();
                            }

                            ImGui::TreePop();
                        }
                        ImGui::PopID();

                        if (removeComponent)
                        {
                            scene.registry.remove<Engine::LuaScriptComponent>(m_selectedEntity);
                        }
                    }

					// Add Component (only show what the entity doesn't currently have)
                    ImGui::Separator();
                    ImGui::Spacing();
                    if (ImGui::Button("Add Component", ImVec2(-1, 30))) {
                        ImGui::OpenPopup("AddComponentPopup");
                    }

                    if (ImGui::BeginPopup("AddComponentPopup"))
                    {
                        if (!scene.registry.all_of<Engine::MeshRendererComponent>(m_selectedEntity)) {
                            if (ImGui::MenuItem("Mesh Renderer")) scene.registry.emplace<Engine::MeshRendererComponent>(m_selectedEntity);
                        }
                        if (!scene.registry.all_of<Engine::LightComponent>(m_selectedEntity)) {
                            if (ImGui::MenuItem("Light")) scene.registry.emplace<Engine::LightComponent>(m_selectedEntity);
                        }
                        if (!scene.registry.all_of<Engine::RigidBodyComponent>(m_selectedEntity)) {
                            if (ImGui::MenuItem("Rigidbody")) scene.registry.emplace<Engine::RigidBodyComponent>(m_selectedEntity);
                        }
                        if (!scene.registry.all_of<Engine::AudioComponent>(m_selectedEntity)) {
                            if (ImGui::MenuItem("Audio Component")) scene.registry.emplace<Engine::AudioComponent>(m_selectedEntity);
                        }
                        if (!scene.registry.all_of<Engine::LuaScriptComponent>(m_selectedEntity)) {
                            if (ImGui::MenuItem("Lua Script")) scene.registry.emplace<Engine::LuaScriptComponent>(m_selectedEntity);
                        }
                        // Add other components like CameraComponent, etc.
                        ImGui::EndPopup();
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
                        // Files: display as selectable so it highlights on hover
                        ImGui::Selectable(("[FILE] " + filenameString).c_str());

                        // If the file is a Lua script, make it a Drag Source
                        if (path.extension() == ".lua")
                        {
                            if (ImGui::BeginDragDropSource())
                            {
                                // Get path and normalize Windows backslashes to forward slashes
                                std::string relativePath = path.string();
                                std::replace(relativePath.begin(), relativePath.end(), '\\', '/');

                                // Attach the string path as the payload
                                ImGui::SetDragDropPayload("SCRIPT_FILE", relativePath.c_str(), relativePath.size() + 1);

                                // Tooltip next to the cursor while dragging
                                ImGui::Text("Assign %s", filenameString.c_str());

                                ImGui::EndDragDropSource();
                            }
                        }
						// If the file is an audio file, make it a Drag Source
                        else if (path.extension() == ".wav" || path.extension() == ".mp3")
                        {
                            if (ImGui::BeginDragDropSource()) {
                                std::string relativePath = path.string();
                                std::replace(relativePath.begin(), relativePath.end(), '\\', '/');
                                ImGui::SetDragDropPayload("AUDIO_FILE", relativePath.c_str(), relativePath.size() + 1);
                                ImGui::Text("Assign %s", filenameString.c_str());
                                ImGui::EndDragDropSource();
                            }
                        }
                    }
                }
            }
            ImGui::End();
        }
    }
}