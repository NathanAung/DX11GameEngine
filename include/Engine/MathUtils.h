#pragma once
#include <DirectXMath.h>

namespace Engine::Math
{
	// Simple struct to represent a ray in world space, with an origin and a direction.
    struct Ray
    {
        DirectX::XMFLOAT3 origin;
        DirectX::XMFLOAT3 direction;
    };

    inline DirectX::XMFLOAT3 QuaternionToEulerDegrees(const DirectX::XMFLOAT4& q)
    {
        using namespace DirectX;

        // Convert quaternion -> pitch/yaw/roll (radians), then to degrees
        const XMVECTOR quat = XMQuaternionNormalize(XMLoadFloat4(&q));
        float pitch = 0.0f;
        float yaw = 0.0f;
        float roll = 0.0f;

        // Convert quaternion to rotation matrix, then extract Euler angles
        // Using a common Yaw-Pitch-Roll extraction (pitch around X, yaw around Y, roll around Z)
        XMMATRIX m = XMMatrixRotationQuaternion(quat);

        // Extract Pitch (X) from m21, clamped to [-1, 1] to prevent NaN
        float sp = -DirectX::XMVectorGetY(m.r[2]);
        if (sp > 1.0f) sp = 1.0f;
        if (sp < -1.0f) sp = -1.0f;
        pitch = asinf(sp);

        // Check for Gimbal Lock (Pitch is exactly +/- 90 degrees)
        if (fabs(sp) > 0.9999f)
        {
            // Roll is locked, extract Yaw from m00 and m02
            roll = 0.0f;
            yaw = atan2f(-DirectX::XMVectorGetZ(m.r[0]), DirectX::XMVectorGetX(m.r[0]));
        }
        else
        {
            // Normal extraction for Yaw (Y) and Roll (Z)
            yaw = atan2f(DirectX::XMVectorGetX(m.r[2]), DirectX::XMVectorGetZ(m.r[2]));
            roll = atan2f(DirectX::XMVectorGetY(m.r[0]), DirectX::XMVectorGetY(m.r[1]));
        }

        const float degPitch = XMConvertToDegrees(pitch);
        const float degYaw = XMConvertToDegrees(yaw);
        const float degRoll = XMConvertToDegrees(roll);

        return XMFLOAT3{ degPitch, degYaw, degRoll };
    }

    inline DirectX::XMFLOAT4 EulerDegreesToQuaternion(const DirectX::XMFLOAT3& euler)
    {
        using namespace DirectX;

        // Convert degrees -> radians and build quaternion (roll/pitch/yaw)
        const float pitch = XMConvertToRadians(euler.x);
        const float yaw   = XMConvertToRadians(euler.y);
        const float roll  = XMConvertToRadians(euler.z);

        XMVECTOR q = XMQuaternionRotationRollPitchYaw(pitch, yaw, roll);
        q = XMQuaternionNormalize(q);

        XMFLOAT4 out{};
        XMStoreFloat4(&out, q);
        return out;
    }

	// Generate a world ray from screen coordinates (e.g., mouse position) using the camera's view and projection matrices.
    inline Ray ScreenToWorldRay(float mouseX, float mouseY, float screenW, float screenH, const DirectX::XMMATRIX& view, const DirectX::XMMATRIX& proj)
    {
        using namespace DirectX;

		// Unproject the near and far points from screen space to world space
        XMVECTOR nearPoint = XMVector3Unproject(
            XMVectorSet(mouseX, mouseY, 0.0f, 0.0f),
            0.0f, 0.0f, screenW, screenH,
            0.0f, 1.0f,
            proj, view, XMMatrixIdentity()
        );

		// Note: The far point's Z is set to 1.0f to represent the far plane in normalized device coordinates
        XMVECTOR farPoint = XMVector3Unproject(
            XMVectorSet(mouseX, mouseY, 1.0f, 0.0f),
            0.0f, 0.0f, screenW, screenH,
            0.0f, 1.0f,
            proj, view, XMMatrixIdentity()
        );

		// The ray direction is the normalized vector from the near point to the far point
        XMVECTOR dir = XMVector3Normalize(XMVectorSubtract(farPoint, nearPoint));

		// Store the ray origin and direction in the Ray struct
        Ray ray{};
        XMStoreFloat3(&ray.origin, nearPoint);
        XMStoreFloat3(&ray.direction, dir);
        return ray;
    }
}