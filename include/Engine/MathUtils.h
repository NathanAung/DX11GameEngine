#pragma once
#include <DirectXMath.h>
#include <DirectXCollision.h>

namespace Engine::Math
{
	// Simple struct to represent a ray in world space, with an origin and a direction.
    struct Ray
    {
        DirectX::XMFLOAT3 origin;
        DirectX::XMFLOAT3 direction;
    };


	// Convert a quaternion to Euler angles in degrees (pitch, yaw, roll).
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


	// Convert Euler angles in degrees to a quaternion representation.
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


	// Check if a ray intersects with an Oriented Bounding Box (OBB) defined by its position, scale, and rotation.
    inline bool RayIntersectsOBB(const Ray& ray, const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT3& scale, const DirectX::XMFLOAT4& rotation, float& outDistance)
    {
        DirectX::BoundingOrientedBox obb;
        obb.Center = position;
        // Assuming a standard 1x1x1 unit volume, half-extents are 0.5 * scale
        obb.Extents = DirectX::XMFLOAT3(scale.x * 0.5f, scale.y * 0.5f, scale.z * 0.5f);
        obb.Orientation = rotation;

        return obb.Intersects(DirectX::XMLoadFloat3(&ray.origin), DirectX::XMLoadFloat3(&ray.direction), outDistance);
    }


    // Calculates the 8 corners of a frustum in world space from a view-projection matrix
    inline std::vector<DirectX::XMVECTOR> GetFrustumCornersWorldSpace(const DirectX::XMMATRIX& viewProj)
    {
		DirectX::XMVECTOR det;  // Determinant for the inverse matrix
		DirectX::XMMATRIX invViewProj = DirectX::XMMatrixInverse(&det, viewProj);   // Inverse of the view-projection matrix

		// Reserve space for 8 corners of the frustum
        std::vector<DirectX::XMVECTOR> corners;
        corners.reserve(8);

        // 8 corners of the NDC cube (DirectX uses Z: 0 to 1)
        for (unsigned int x = 0; x < 2; ++x)
        {
            for (unsigned int y = 0; y < 2; ++y)
            {
                for (unsigned int z = 0; z < 2; ++z)
                {
                    DirectX::XMFLOAT4 pt(
                        2.0f * x - 1.0f,
                        2.0f * y - 1.0f,
                        1.0f * z, // Z: 0 for near, 1 for far
                        1.0f);

                    DirectX::XMVECTOR ptV = DirectX::XMLoadFloat4(&pt);
                    DirectX::XMVECTOR transformed = DirectX::XMVector4Transform(ptV, invViewProj);

                    // Perspective divide
                    transformed = DirectX::XMVectorScale(transformed, 1.0f / DirectX::XMVectorGetW(transformed));
                    corners.push_back(transformed);
                }
            }
        }
        return corners;
    }
}