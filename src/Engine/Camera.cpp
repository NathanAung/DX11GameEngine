#include "Engine/Camera.h"
using namespace DirectX;
using namespace Engine;

// Helpers to load/store XMFLOAT3 <-> XMVECTOR
static inline XMVECTOR LoadF3(const XMFLOAT3& f) { return XMLoadFloat3(&f); }
static inline void StoreF3(XMFLOAT3& dst, FXMVECTOR v) { XMStoreFloat3(&dst, v); }

Camera::Camera() { RecomputeBasis(); }

// Set the viewport dimensions (used for aspect ratio)
void Camera::SetViewport(unsigned width, unsigned height)
{
    m_viewW = (width == 0 ? 1u : width);
    m_viewH = (height == 0 ? 1u : height);
}

// Set the lens parameters
void Camera::SetLens(float fovYRadians, float nearZ, float farZ)
{
    m_fovY = fovYRadians;
    m_nearZ = nearZ;
    m_farZ = farZ;
}

// Set the camera position in world space
void Camera::SetPosition(const XMFLOAT3& pos)
{
    m_position = pos;
}

// Set movement and look speeds
void Camera::SetSpeeds(float moveSpeed, float sprintMultiplier, float lookSensitivity)
{
    m_moveSpeed = moveSpeed;
    m_sprintMultiplier = sprintMultiplier;
    m_lookSensitivity = lookSensitivity;
}

// Recompute the camera basis vectors from yaw/pitch angles
void Camera::RecomputeBasis()
{
    // Forward from yaw/pitch (LH, yaw=0 looks along +Z)
    const float cy = cosf(m_yaw);
    const float sy = sinf(m_yaw);
    const float cp = cosf(m_pitch);
    const float sp = sinf(m_pitch);

    XMVECTOR forward = XMVector3Normalize(XMVectorSet(sy * cp, sp, cy * cp, 0.0f));
    XMVECTOR worldUp = XMVectorSet(0, 1, 0, 0);
    XMVECTOR right = XMVector3Normalize(XMVector3Cross(worldUp, forward));
    XMVECTOR up = XMVector3Normalize(XMVector3Cross(forward, right));

    StoreF3(m_forward, forward);
    StoreF3(m_right, right);
    StoreF3(m_up, up);
}

// Update camera based on input and delta time
void Camera::UpdateFromInput(const InputManager& input, float dt)
{
    // Look: accumulate relative mouse deltas
    const auto md = input.GetMouseDelta();
    m_yaw   += static_cast<float>(md.dx) * m_lookSensitivity;
	m_pitch += static_cast<float>(md.dy) * m_lookSensitivity * (m_invertY ? -1.0f : 1.0f);

    // Clamp pitch to avoid gimbal flip
    const float kPitchLimit = XMConvertToRadians(89.0f);
    if (m_pitch >  kPitchLimit) m_pitch =  kPitchLimit;
    if (m_pitch < -kPitchLimit) m_pitch = -kPitchLimit;

    // Wrap yaw to keep values bounded
    if (m_yaw > XM_PI)  m_yaw -= XM_2PI;
    if (m_yaw < -XM_PI) m_yaw += XM_2PI;

    RecomputeBasis();

    // Move: W/A/S/D (+ Shift sprint)
    float speed = m_moveSpeed * dt;
    if (input.IsKeyDown(Key::LShift)) speed *= m_sprintMultiplier;

    XMVECTOR move = XMVectorZero();
    if (input.IsKeyDown(Key::W)) move = XMVectorAdd(move, LoadF3(m_forward));
    if (input.IsKeyDown(Key::S)) move = XMVectorSubtract(move, LoadF3(m_forward));
    if (input.IsKeyDown(Key::D)) move = XMVectorAdd(move, LoadF3(m_right));
    if (input.IsKeyDown(Key::A)) move = XMVectorSubtract(move, LoadF3(m_right));
    // Space to move up/down (flycam)
    if (input.IsKeyDown(Key::Space)) move = XMVectorAdd(move, XMVectorSet(0,1,0,0));

	// Normalize and scale movement
    if (!XMVector3Equal(move, XMVectorZero()))
    {
        move = XMVector3Normalize(move);
        move = XMVectorScale(move, speed);
        XMVECTOR pos = XMVectorAdd(LoadF3(m_position), move);
        StoreF3(m_position, pos);
    }
}

// Get the view matrix
XMMATRIX Camera::GetViewMatrix() const
{
    return XMMatrixLookToLH(LoadF3(m_position), LoadF3(m_forward), LoadF3(m_up));
}

// Get the projection matrix
XMMATRIX Camera::GetProjectionMatrix() const
{
    const float aspect = static_cast<float>(m_viewW) / static_cast<float>(m_viewH ? m_viewH : 1u);
    return XMMatrixPerspectiveFovLH(m_fovY, aspect, m_nearZ, m_farZ);
}