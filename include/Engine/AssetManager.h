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
        Shader,
        ModelFile,
		LuaScript,
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
		// Every asset the engine has ever seen is logged in a ledger (JSON file) for future reference.
		// This gurantees that UUIDs remain consistent across engine sessions and prevents accidental duplication of assets.
        
        // Saves the current state of the ledger to a JSON file
        bool SaveRegistry(const std::string& filepath);
        // Loads a saved ledger from a JSON file, restoring all UUIDs
        bool LoadRegistry(const std::string& filepath);

        // Table of Contents Entry for the VFS
        struct TOCEntry {
            uint64_t uuid;
            uint32_t type;
            uint64_t offset;
            uint64_t size;
        };

        // Compiles all physical files in the registry into a single binary archive
        bool PackAssets(const std::string& pakFilepath) const;

        // Mounts the binary archive and loads the Table of Contents into memory
        bool MountVFS(const std::string& pakFilepath);

        // Checks if the engine is currently running in VFS mode
        bool IsVFSActive() const { return m_useVFS; }

        // Seeks to the exact byte offset in the archive and extracts the raw asset data
        std::vector<char> ReadAssetFromVFS(UUID handle) const;

        // Checks if an asset currently exists inside the mounted VFS
        bool IsInVFS(UUID handle) const;

    private:
		// Internal registry mapping UUIDs to their corresponding asset metadata
        std::unordered_map<UUID, AssetMetadata> m_assetRegistry;

        // VFS State
        std::unordered_map<uint64_t, TOCEntry> m_vfsTable;
        std::string m_vfsPakPath;
        bool m_useVFS = false;
    };
}