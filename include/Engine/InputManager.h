#pragma once
#include <SDL.h>
#include <array>
#include <cstdint>

// Input Manager class manages keyboard and mouse input using SDL2.
// all inline to keep the translation unit lightweight

// namespace for the engine components
namespace Engine
{
	// Enumeration for keys we want to track
    enum class Key : uint8_t
    {
        W = 0,
        A,
        S,
        D,
        LShift,
        Space,
        Esc,
        Count
    };

	// Structure to hold mouse movement delta
    struct MouseDelta
    {
        int dx = 0;
        int dy = 0;
    };

    class InputManager
    {
    public:
		// Call at the start of each frame to reset per-frame state
        void BeginFrame()
        {
            m_mouseDelta.dx = 0;
            m_mouseDelta.dy = 0;
        }

		// Process an SDL event and update input state
        // Returns true if the event was consumed by input handling (informational)
        bool ProcessEvent(const SDL_Event& e)
        {
            switch (e.type)
            {
            case SDL_KEYDOWN:
            case SDL_KEYUP:
            {
				// check if the key is still held down or released
                const bool down = (e.type == SDL_KEYDOWN);

				// continuous key repeat filtering:
                if (e.type == SDL_KEYDOWN)
                {
                    // Set key to down (true) for *any* keydown event (repeat or not)
                    MapAndSetKey(e.key.keysym.scancode, true);
                }
                else if (e.type == SDL_KEYUP)
                {
                    // Set key to up (false) only on explicit key up
                    MapAndSetKey(e.key.keysym.scancode, false);
                }
				// this ignores repeated keydown events (when key is held)
                //const bool down = (e.type == SDL_KEYDOWN) && (e.key.repeat == 0);

                break;
            }
            case SDL_MOUSEMOTION:
            {
                // Prefer relative motion if relative mouse mode is on
                m_mouseDelta.dx += e.motion.xrel;
                m_mouseDelta.dy += e.motion.yrel;
                break;
            }
            default:
                break;
            }
            return false;
        }

		// Query if a specific key is currently held down
        bool IsKeyDown(Key k) const { return m_keys[static_cast<size_t>(k)]; }

		// Get the accumulated mouse movement delta for the current frame
        MouseDelta GetMouseDelta() const { return m_mouseDelta; }

		// Enable or disable mouse capture (relative mode)
        void SetMouseCaptured(bool enabled)
        {
            SDL_SetRelativeMouseMode(enabled ? SDL_TRUE : SDL_FALSE);
            SDL_ShowCursor(enabled ? SDL_DISABLE : SDL_ENABLE);
        }

    private:
		// Map SDL_Scancode to Key enum and set its state
        void MapAndSetKey(SDL_Scancode sc, bool down)
        {
            switch (sc)
            {
            case SDL_SCANCODE_W:      m_keys[static_cast<size_t>(Key::W)] = down; break;
            case SDL_SCANCODE_A:      m_keys[static_cast<size_t>(Key::A)] = down; break;
            case SDL_SCANCODE_S:      m_keys[static_cast<size_t>(Key::S)] = down; break;
            case SDL_SCANCODE_D:      m_keys[static_cast<size_t>(Key::D)] = down; break;
            case SDL_SCANCODE_LSHIFT: m_keys[static_cast<size_t>(Key::LShift)] = down; break;
            case SDL_SCANCODE_SPACE:  m_keys[static_cast<size_t>(Key::Space)] = down; break;
			case SDL_SCANCODE_ESCAPE: m_keys[static_cast<size_t>(Key::Esc)] = down; break;
            default: break;
            }
        }

		// State of tracked keys
        std::array<bool, static_cast<size_t>(Key::Count)> m_keys{ false, false, false, false, false, false, false };
		// Accumulated mouse movement delta for the current frame
        MouseDelta m_mouseDelta{};
    };
}