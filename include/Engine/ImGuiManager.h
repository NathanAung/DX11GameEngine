#pragma once

#include <imgui.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_dx11.h>

#include <SDL.h>
#include <d3d11.h>

// The ImGuiManager class encapsulates the initialization, frame management, and event processing for Dear ImGui in the context of an SDL2 + DirectX 11 application.

namespace Engine
{
    class ImGuiManager
    {
    public:
		// Initializes ImGui with SDL2 and DirectX 11 contexts. Returns true on success.
        bool Initialize(SDL_Window* window, ID3D11Device* device, ID3D11DeviceContext* context);
		// Shuts down ImGui and cleans up resources.
        void Shutdown();

		// Call at the start of each frame to set up ImGui for new frame rendering.
        void BeginFrame();
		// Call at the end of each frame to render ImGui draw data.
        void EndFrame();

		// Processes an SDL event and updates ImGui's internal state. Returns true if ImGui wants to capture this event (mouse or keyboard).
        bool ProcessEvent(const SDL_Event& e);
    };
}