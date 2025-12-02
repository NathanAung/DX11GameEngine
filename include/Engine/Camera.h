#pragma once
#include <DirectXMath.h>
#include "Engine/InputManager.h"

// Camera class for 3D navigation

namespace Engine
{
    class Camera
    {
    public:
        Camera();

        void SetViewport(unsigned width, unsigned height);
        void SetLens(float fovYRadians, float nearZ, float farZ);
        void SetPosition(const DirectX::XMFLOAT3& pos);

        // tweak speeds at runtime
        void SetSpeeds(float moveSpeed, float sprintMultiplier, float lookSensitivity);

        void UpdateFromInput(const InputManager& input, float dt);

        DirectX::XMMATRIX GetViewMatrix() const;
        DirectX::XMMATRIX GetProjectionMatrix() const;

        float GetYaw() const { return m_yaw; }
        float GetPitch() const { return m_pitch; }

    private:
        void RecomputeBasis();

        // State
        DirectX::XMFLOAT3 m_position{ 0.0f, 0.0f, -5.0f };
        DirectX::XMFLOAT3 m_forward{ 0.0f, 0.0f, 1.0f };
        DirectX::XMFLOAT3 m_right{ 1.0f, 0.0f, 0.0f };
        DirectX::XMFLOAT3 m_up{ 0.0f, 1.0f, 0.0f };
        float m_yaw = 0.0f;    // radians
        float m_pitch = 0.0f;  // radians

        // Lens / viewport
        unsigned m_viewW = 1, m_viewH = 1;
		float m_fovY = DirectX::XM_PIDIV4;  // 45 degrees
        float m_nearZ = 0.1f;
        float m_farZ = 100.0f;

        // Movement / look
        float m_moveSpeed = 4.0f;         // units per second
        float m_sprintMultiplier = 2.0f;  // while holding Shift
        float m_lookSensitivity = 0.0025f; // radians per pixel

		bool m_invertY = true; // invert Y look
    };
}