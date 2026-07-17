#include "Engine/AssetManager.h"

// Standard library includes
#include <fstream>
#include <sstream>
#include <iostream>

// RapidJSON includes
#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>

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


    bool AssetManager::SaveRegistry(const std::string& filepath)
    {
		// Create a JSON document to hold the asset registry
        rapidjson::StringBuffer sb;
		// Use PrettyWriter for human-readable formatting
        rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(sb);

		// Start the JSON object
        writer.StartObject();
		writer.Key("Assets");   // Key for the array of assets
		writer.StartArray();    // Start the array of assets

        // Write every known asset to the ledger
        // NOTE: We intentionally save "virtual paths" (like primitive://cube).
        // This ensures procedural primitives get the exact same UUID next time we boot.
        for (const auto& [uuid, meta] : m_assetRegistry)
        {
            writer.StartObject();

			// Write the UUID, filepath, and type of each asset
            writer.Key("UUID");
            writer.Uint64(uuid);

            writer.Key("Filepath");
            writer.String(meta.filepath.c_str());

            writer.Key("Type");
            writer.Int(static_cast<int>(meta.type));

            writer.EndObject();
        }

        writer.EndArray();
        writer.EndObject();

        // Write to disk
		// NOTE: This will overwrite any existing file, which is the intended behavior.

		// Open the file for writing
        std::ofstream file(filepath);
		// Check if the file was successfully opened
        if (!file.is_open())
        {
            std::cerr << "Failed to open Asset Registry for writing: " << filepath << std::endl;
            return false;
        }

		// Write the JSON string to the file
        file << sb.GetString();
        file.close();

        std::cout << "Successfully saved Asset Registry to " << filepath << std::endl;
        return true;
    }


    bool AssetManager::LoadRegistry(const std::string& filepath)
    {
        std::ifstream file(filepath);
        if (!file.is_open())
        {
            // It's completely normal for this to fail on the very first boot of a new project
            std::cout << "No existing Asset Registry found at " << filepath << ". Starting fresh." << std::endl;
            return false;
        }

		// Read the entire file into a string
        std::stringstream buffer;
        buffer << file.rdbuf();
        file.close();

		// Parse the JSON string into a RapidJSON document
        rapidjson::Document doc;
        doc.Parse(buffer.str().c_str());

		// Validate the JSON structure
        if (doc.HasParseError() || !doc.HasMember("Assets") || !doc["Assets"].IsArray())
        {
            std::cerr << "Failed to parse Asset Registry JSON!" << std::endl;
            return false;
        }

        // Clear existing just in case
        m_assetRegistry.clear();

		// Read every asset from the ledger and restore it to the registry
        const auto& assets = doc["Assets"];
        for (rapidjson::SizeType i = 0; i < assets.Size(); i++)
        {
            const auto& item = assets[i];

            UUID uuid = item["UUID"].GetUint64();
            std::string path = item["Filepath"].GetString();
            AssetType type = static_cast<AssetType>(item["Type"].GetInt());

            AssetMetadata meta;
            meta.handle = uuid;
            meta.filepath = path;
            meta.type = type;

            // CRITICAL: We mark them as NOT loaded. 
            // This tells the engine the file exists in the ledger, but isn't taking up GPU memory yet.
            meta.isLoaded = false;

            m_assetRegistry[uuid] = meta;
        }

        std::cout << "Successfully loaded Asset Registry from " << filepath << " (" << m_assetRegistry.size() << " assets known)." << std::endl;
        return true;
    }
}