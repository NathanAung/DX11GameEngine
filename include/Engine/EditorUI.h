#pragma once
#include <filesystem>
#include <entt/entt.hpp>
#include "Engine/Scene.h"
#include "Engine/Renderer.h"
#include "Engine/InputManager.h"

struct SDL_Window;

// EditorUI class manages the ImGui-based editor interface, including the Scene view and other panels.

namespace Engine
{
    class PhysicsManager;

    // Editor state machine
    enum class EditorState { Edit, Play };

    class EditorUI
    {
    public:
        void Render(Engine::Scene& scene, Engine::Renderer& renderer, Engine::InputManager& input, Engine::PhysicsManager& physicsManager, SDL_Window* window);

        bool IsSceneFocused() const { return m_scenePanelFocused; }

        EditorState GetState() const { return m_state; }

    private:
        bool m_scenePanelFocused = false;

        entt::entity m_selectedEntity = entt::null;

		// Used to determine which transformation gizmo to display in the Scene view when an entity is selected.
        int m_gizmoType = 0; // 0 = Translate, 1 = Rotate, 2 = Scale

        EditorState m_state = EditorState::Edit;
        // Cached editor camera (so Play mode can always revert cleanly)
        entt::entity m_editorCamera = entt::null;

        std::filesystem::path m_assetPath = "assets";
        std::filesystem::path m_currentDirectory = "assets";
    };
}