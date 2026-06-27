#include "Engine/AssetManager.h"

namespace Engine
{
    UUID AssetManager::ImportAsset(const std::string& filepath, AssetType type)
    {
        // Check if this exact file is already registered to prevent duplicates
        for (const auto& [uuid, metadata] : m_assetRegistry)
        {
            if (metadata.filepath == filepath)
            {
                return uuid;
            }
        }

        // If it is a new file, generate a UUID and log it in the registry
        UUID newHandle;

        AssetMetadata metadata;
        metadata.handle = newHandle;
        metadata.filepath = filepath;
        metadata.type = type;
        metadata.isLoaded = false;

        m_assetRegistry[newHandle] = metadata;

        return newHandle;
    }


    const AssetMetadata* AssetManager::GetMetadata(UUID handle) const
    {
        auto it = m_assetRegistry.find(handle);
        if (it != m_assetRegistry.end())
        {
            return &it->second;
        }
        return nullptr;
    }


    void AssetManager::SetAssetLoaded(UUID handle, bool loaded)
    {
        auto it = m_assetRegistry.find(handle);
        if (it != m_assetRegistry.end())
        {
            it->second.isLoaded = loaded;
        }
    }
}