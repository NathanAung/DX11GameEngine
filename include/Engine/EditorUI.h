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
    class EditorUI
    {
    public:
        void Render(Engine::Scene& scene, Engine::Renderer& renderer, Engine::InputManager& input, SDL_Window* window);

        bool IsSceneFocused() const { return m_scenePanelFocused; }

    private:
        bool m_scenePanelFocused = false;

        entt::entity m_selectedEntity = entt::null;

        std::filesystem::path m_assetPath = "assets";
        std::filesystem::path m_currentDirectory = "assets";
    };
}