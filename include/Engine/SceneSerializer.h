#pragma once
#include <string>

// SceneSerializer class is responsible for serializing the scene's entities and components to a JSON file for saving/loading purposes.

namespace Engine
{
    class Scene;
	class AssetManager;

    class SceneSerializer
    {
    public:
        // Serializes all entities and components in the scene to a JSON file
        static bool Serialize(const std::string& filepath, Scene& scene);

        // Parses a JSON scene file. Validates assets first, aborts if files are missing.
        static bool Deserialize(const std::string& filepath, Scene& scene, PhysicsManager& physicsManager, AssetManager& assetManager, std::string& outErrorMsg);
    };
}