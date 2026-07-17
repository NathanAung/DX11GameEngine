#pragma once
#include "Engine/UUID.h"
#include <string>
#include <unordered_map>

// AssetManager is responsible for managing assets (meshes, textures, audio) in the engine.
// It maintains a registry of assets, each identified by a unique UUID, along with metadata such as file path, type, and loaded status.

namespace Engine
{
    enum class AssetType
    {
        None = 0,
        Mesh,
        Texture,
        Audio,
        Shader
    };

	// metadata is used for serialization and deserialization of the asset registry, as well as for runtime asset management.
    struct AssetMetadata
    {
        UUID handle = 0;
        std::string filepath = "";
        AssetType type = AssetType::None;
        bool isLoaded = false;
    };

    class AssetManager
    {
    public:
		// Default constructor and destructor
        AssetManager() = default;
        ~AssetManager() = default;

        // Registers a file and assigns it a UUID. 
        // If the file is already registered, it safely returns the existing UUID.
        UUID ImportAsset(const std::string& filepath, AssetType type);

        // Fetches the metadata (filepath, type, loaded status) for a given UUID
        const AssetMetadata* GetMetadata(UUID handle) const;

        // Updates the loaded status of an asset
        void SetAssetLoaded(UUID handle, bool loaded);

        // Exposes the raw registry (useful later for serializing the asset ledger to a file)
        const std::unordered_map<UUID, AssetMetadata>& GetRegistry() const { return m_assetRegistry; }

        // SERIALIZATION
        // Saves the current state of the ledger to a JSON file
        bool SaveRegistry(const std::string& filepath);
        // Loads a saved ledger from a JSON file, restoring all UUIDs
        bool LoadRegistry(const std::string& filepath);

    private:
		// Internal registry mapping UUIDs to their corresponding asset metadata
        std::unordered_map<UUID, AssetMetadata> m_assetRegistry;
    };
}