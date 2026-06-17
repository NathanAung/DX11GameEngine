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
    enum class Key : uint16_t
    {
        Unknown = 0,
        A, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
        Num0, Num1, Num2, Num3, Num4, Num5, Num6, Num7, Num8, Num9,
        Escape, LControl, LShift, LAlt, RControl, RShift, RAlt,
        Space, Enter, Backspace, Tab,
        Up, Down, Left, Right,
        Count
    };

	// Structure to hold mouse movement delta
    struct MouseDelta
    {
        int dx = 0;
        int dy = 0;
        int wheelY = 0;
    };

    class InputManager
    {
    public:
		// Call at the start of each frame to reset per-frame state
        void BeginFrame()
        {
            // Copy current state to previous state before processing this frame's events
            m_keysPrevious = m_keys;

            m_mouseDelta.dx = 0;
            m_mouseDelta.dy = 0;
            m_mouseDelta.wheelY = 0;
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
            case SDL_MOUSEWHEEL:
            {
                m_mouseDelta.wheelY += e.wheel.y;
                break;
            }
            default:
                break;
            }
            return false;
        }

		// Query if a specific key is currently held down
        // True as long as the key is held down
        bool IsKeyDown(Key k) const { return m_keys[static_cast<size_t>(k)]; }

        // True ONLY on the exact frame the key is pressed
        bool IsKeyPressed(Key k) const
        {
            return m_keys[static_cast<size_t>(k)] && !m_keysPrevious[static_cast<size_t>(k)];
        }

        // True ONLY on the exact frame the key is released
        bool IsKeyReleased(Key k) const
        {
            return !m_keys[static_cast<size_t>(k)] && m_keysPrevious[static_cast<size_t>(k)];
        }

		// Get the accumulated mouse movement delta for the current frame
        MouseDelta GetMouseDelta() const { return m_mouseDelta; }

        bool IsMouseCaptured() const { return m_isCaptured; }

		// Enable or disable mouse capture (relative mode)
        void SetMouseCaptured(bool enabled)
        {
            m_isCaptured = enabled;
            SDL_SetRelativeMouseMode(enabled ? SDL_TRUE : SDL_FALSE);
            SDL_ShowCursor(enabled ? SDL_DISABLE : SDL_ENABLE);
        }

    private:
		// Map SDL_Scancode to Key enum and set its state
        void MapAndSetKey(SDL_Scancode sc, bool down)
        {
            Key k = Key::Unknown;
            switch (sc)
            {
                // Alphabet
                case SDL_SCANCODE_A: k = Key::A; break;
                case SDL_SCANCODE_B: k = Key::B; break;
                case SDL_SCANCODE_C: k = Key::C; break;
                case SDL_SCANCODE_D: k = Key::D; break;
                case SDL_SCANCODE_E: k = Key::E; break;
                case SDL_SCANCODE_F: k = Key::F; break;
                case SDL_SCANCODE_G: k = Key::G; break;
                case SDL_SCANCODE_H: k = Key::H; break;
                case SDL_SCANCODE_I: k = Key::I; break;
                case SDL_SCANCODE_J: k = Key::J; break;
                case SDL_SCANCODE_K: k = Key::K; break;
                case SDL_SCANCODE_L: k = Key::L; break;
                case SDL_SCANCODE_M: k = Key::M; break;
                case SDL_SCANCODE_N: k = Key::N; break;
                case SDL_SCANCODE_O: k = Key::O; break;
                case SDL_SCANCODE_P: k = Key::P; break;
                case SDL_SCANCODE_Q: k = Key::Q; break;
                case SDL_SCANCODE_R: k = Key::R; break;
                case SDL_SCANCODE_S: k = Key::S; break;
                case SDL_SCANCODE_T: k = Key::T; break;
                case SDL_SCANCODE_U: k = Key::U; break;
                case SDL_SCANCODE_V: k = Key::V; break;
                case SDL_SCANCODE_W: k = Key::W; break;
                case SDL_SCANCODE_X: k = Key::X; break;
                case SDL_SCANCODE_Y: k = Key::Y; break;
                case SDL_SCANCODE_Z: k = Key::Z; break;

                // Numbers
                case SDL_SCANCODE_0: k = Key::Num0; break;
                case SDL_SCANCODE_1: k = Key::Num1; break;
                case SDL_SCANCODE_2: k = Key::Num2; break;
                case SDL_SCANCODE_3: k = Key::Num3; break;
                case SDL_SCANCODE_4: k = Key::Num4; break;
                case SDL_SCANCODE_5: k = Key::Num5; break;
                case SDL_SCANCODE_6: k = Key::Num6; break;
                case SDL_SCANCODE_7: k = Key::Num7; break;
                case SDL_SCANCODE_8: k = Key::Num8; break;
                case SDL_SCANCODE_9: k = Key::Num9; break;

                // Arrows
                case SDL_SCANCODE_UP:    k = Key::Up; break;
                case SDL_SCANCODE_DOWN:  k = Key::Down; break;
                case SDL_SCANCODE_LEFT:  k = Key::Left; break;
                case SDL_SCANCODE_RIGHT: k = Key::Right; break;

                // Modifiers & Actions
                case SDL_SCANCODE_ESCAPE:    k = Key::Escape; break;
                case SDL_SCANCODE_LCTRL:     k = Key::LControl; break;
                case SDL_SCANCODE_LSHIFT:    k = Key::LShift; break;
                case SDL_SCANCODE_LALT:      k = Key::LAlt; break;
                case SDL_SCANCODE_RCTRL:     k = Key::RControl; break;
                case SDL_SCANCODE_RSHIFT:    k = Key::RShift; break;
                case SDL_SCANCODE_RALT:      k = Key::RAlt; break;
                case SDL_SCANCODE_SPACE:     k = Key::Space; break;
                case SDL_SCANCODE_RETURN:    k = Key::Enter; break;
                case SDL_SCANCODE_BACKSPACE: k = Key::Backspace; break;
                case SDL_SCANCODE_TAB:       k = Key::Tab; break;

                default: break;
            }

			// Update the key state if it's a recognized key
            if (k != Key::Unknown)
            {
                m_keys[static_cast<size_t>(k)] = down;
            }
        }

		std::array<bool, static_cast<size_t>(Key::Count)> m_keys{};         // Current state of tracked keys
		std::array<bool, static_cast<size_t>(Key::Count)> m_keysPrevious{}; // Previous frame's state of tracked keys

		// Accumulated mouse movement delta for the current frame
        MouseDelta m_mouseDelta{};

        bool m_isCaptured = false;
    };
}