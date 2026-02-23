#pragma once
#include <DirectXMath.h>

namespace Engine::Math
{
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

        // Pitch (X)
        pitch = asinf(-XMVectorGetZ(m.r[1]));
        // Yaw (Y)
        yaw = atan2f(XMVectorGetX(m.r[2]), XMVectorGetX(m.r[0]));
        // Roll (Z)
        roll = atan2f(XMVectorGetZ(m.r[1]), XMVectorGetY(m.r[1]));

        const float degPitch = XMConvertToDegrees(pitch);
        const float degYaw   = XMConvertToDegrees(yaw);
        const float degRoll  = XMConvertToDegrees(roll);

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
}