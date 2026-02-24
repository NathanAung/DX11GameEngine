#include "Engine/EditorUI.h"
#include "Engine/Components.h"
#include "Engine/MathUtils.h"
#include <imgui.h>
#include <SDL.h>

namespace Engine
{
    void EditorUI::Render(Engine::Scene& scene, Engine::Renderer& renderer, Engine::InputManager& input, SDL_Window* window)
    {
        // Root editor layout: invisible grid that panels dock into
        ImGui::DockSpaceOverViewport(ImGui::GetMainViewport()->ID);

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

        // Render the framebuffer texture (from off-screen rendering) as an ImGui image in the Scene panel
        ImGui::Image((ImTextureID)(intptr_t)renderer.GetFramebufferSRV(), viewportSize);

        // Smart Input Routing: Right-Click to Fly (Scene panel only)
        {
            // Check if the Scene panel is hovered for input routing
            bool isHovered = ImGui::IsWindowHovered();
            m_scenePanelFocused = ImGui::IsWindowFocused();

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
                        ImGui::DragFloat3("Scale", &tc.scale.x, 0.1f);

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