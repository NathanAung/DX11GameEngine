#include "Engine/ImGuiManager.h"

namespace Engine
{
    bool ImGuiManager::Initialize(SDL_Window* window, ID3D11Device* device, ID3D11DeviceContext* context)
    {
		IMGUI_CHECKVERSION();       // Verify that the ImGui version is correct
		ImGui::CreateContext();     // Create a new ImGui context

		// Enable docking and keyboard navigation in ImGui
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

		// Set the ImGui style to dark
        ImGui::StyleColorsDark();

		// Initialize ImGui for SDL2 and DirectX 11. Return false if either initialization fails.
        if (!ImGui_ImplSDL2_InitForD3D(window))
            return false;

		// Note: ImGui_ImplDX11_Init must be called after ImGui_ImplSDL2_InitForD3D because the latter sets up the necessary context for DirectX rendering.
        if (!ImGui_ImplDX11_Init(device, context))
            return false;

        return true;
    }


    void ImGuiManager::Shutdown()
    {
		// Shutdown ImGui for DirectX 11 and SDL2, then destroy the ImGui context to clean up all resources.
        ImGui_ImplDX11_Shutdown();
        ImGui_ImplSDL2_Shutdown();
        ImGui::DestroyContext();
    }


    void ImGuiManager::BeginFrame()
    {
		// Start a new ImGui frame by calling the NewFrame functions for both DirectX 11 and SDL2, then call ImGui::NewFrame to set up ImGui for rendering.
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();
    }


    void ImGuiManager::EndFrame()
    {
		// Finalize the ImGui frame and render the draw data using the DirectX 11 backend.
        ImGui::Render();
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    }


    bool ImGuiManager::ProcessEvent(const SDL_Event& e)
    {
		// Pass the SDL event to ImGui's SDL2 backend for processing. This updates ImGui's internal state based on the event (e.g., mouse position, button states, keyboard input).
        ImGui_ImplSDL2_ProcessEvent(&e);

		// After processing the event, check if ImGui wants to capture the mouse or keyboard input. If either is true, it means ImGui is currently interacting with the user and wants to consume the input events, so we return true to indicate that the event should not be processed further by the application.
        const ImGuiIO& io = ImGui::GetIO();
        return io.WantCaptureMouse || io.WantCaptureKeyboard;
    }
}