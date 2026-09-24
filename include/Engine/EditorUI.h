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
		bool m_scenePanelFocused = false;   // Tracks if the Scene panel is currently focused for input handling

        entt::entity m_selectedEntity = entt::null;

		// Used to determine which transformation gizmo to display in the Scene view when an entity is selected.
        int m_gizmoType = 0; // 0 = Translate, 1 = Rotate, 2 = Scale

        EditorState m_state = EditorState::Edit;
        // Cached editor camera (so Play mode can always revert cleanly)
        entt::entity m_editorCamera = entt::null;

		// Asset path and current working directory for file dialogs
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
        std::string m_exportErrorMsg = "";

        // Create Scene Modal State
        bool m_openCreateScenePopup = false;
        char m_newSceneNameBuf[256] = "";
        bool m_showCreateSceneWarning = false;
        std::string m_createSceneWarningMsg = "";

		// Recursive function to draw an entity node in the Scene Hierarchy panel, including its children and handling selection and deletion.
        void DrawEntityNode(Engine::Scene& scene, entt::entity entity, entt::entity& entityToDestroy);

		// Draws all modal popups for saving, loading, exporting, and creating scenes, handling user input and displaying warnings or errors as needed.
        void DrawModals(Engine::Scene& scene, Engine::PhysicsManager& physicsManager);
        
		// Sets up the ImGui Dockspace for the editor, allowing for flexible panel arrangement and docking of various UI elements.
        void SetupDockspace(const std::string& sceneWindowTitle);

		// Draws the toolbar at the top of the editor, including Play/Stop buttons and other controls, and handles their interactions with the scene and physics manager.
        void DrawToolbar(Engine::Scene& scene, Engine::PhysicsManager& physicsManager);
        
		// Draws the Content Browser panel, allowing users to navigate the asset directory, create new scenes, and load existing scenes, while handling user interactions and displaying relevant warnings or errors.
        void DrawContentBrowser(Engine::Scene& scene, Engine::Renderer& renderer, Engine::MeshManager& meshManager, Engine::TextureManager& textureManager, Engine::PhysicsManager& physicsManager);

		// Draws the Hierarchy panel, displaying the scene's entity hierarchy and allowing users to select, manipulate, and delete entities, while also handling user interactions and updating the selected entity state.
        void DrawHierarchy(Engine::Scene& scene, Engine::Renderer& renderer, Engine::MeshManager& meshManager, Engine::PhysicsManager& physicsManager);
        
		// Draws the Inspector panel, displaying the properties of the selected entity and allowing users to modify its components, while also handling user interactions and updating the scene accordingly.
        void DrawInspector(Engine::Scene& scene, Engine::Renderer& renderer, Engine::MeshManager& meshManager, Engine::TextureManager& textureManager, Engine::PhysicsManager& physicsManager);

        // Draws the primary 3D viewport, handles framebuffer rendering, gizmo manipulation, and scene picking.
        void DrawSceneView(Engine::Scene& scene, Engine::Renderer& renderer, Engine::InputManager& input, Engine::PhysicsManager& physicsManager, SDL_Window* window, const std::string& sceneWindowTitle);
    };
}