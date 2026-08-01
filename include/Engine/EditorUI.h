#pragma once
#include <filesystem>
#include <entt/entt.hpp>

struct SDL_Window;

// EditorUI class manages the ImGui-based editor interface, including the Scene view and other panels.

namespace Engine
{
    class PhysicsManager;
	class InputManager;
	class Scene;
	class Renderer;
    class MeshManager;
    class TextureManager;

    // Editor state machine
    enum class EditorState { Edit, Play };

    class EditorUI
    {
    public:
        void Render(Engine::Scene& scene, Engine::Renderer& renderer, Engine::MeshManager& meshManager, Engine::TextureManager& textureManager, Engine::InputManager& input, Engine::PhysicsManager& physicsManager, SDL_Window* window);

        bool IsSceneFocused() const { return m_scenePanelFocused; }

        EditorState GetState() const { return m_state; }

        entt::entity GetSelectedEntity() const { return m_selectedEntity; }

    private:
        void DrawEntityNode(Engine::Scene& scene, entt::entity entity, entt::entity& entityToDestroy);

        bool m_scenePanelFocused = false;

        entt::entity m_selectedEntity = entt::null;

		// Used to determine which transformation gizmo to display in the Scene view when an entity is selected.
        int m_gizmoType = 0; // 0 = Translate, 1 = Rotate, 2 = Scale

        EditorState m_state = EditorState::Edit;
        // Cached editor camera (so Play mode can always revert cleanly)
        entt::entity m_editorCamera = entt::null;

        std::filesystem::path m_assetPath = "assets";
        std::filesystem::path m_currentDirectory = "assets";

        // Save Scene Modal State
        char m_saveFilenameBuf[256] = "";
        bool m_showSaveWarning = false;
        std::string m_saveWarningMsg = "";

		// Load Scene Error State for displaying error messages when loading fails
        bool m_showLoadError = false;
        std::string m_loadErrorMsg = "";

		// Export Scene Error State for displaying error messages when exporting fails
        bool m_showExportError = false;

        // Create Scene Modal State
        bool m_openCreateScenePopup = false;
        char m_newSceneNameBuf[256] = "";
        bool m_showCreateSceneWarning = false;
        std::string m_createSceneWarningMsg = "";
    };
}