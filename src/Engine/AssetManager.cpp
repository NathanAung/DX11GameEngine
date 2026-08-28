#include "Engine/AssetManager.h"

// Standard library includes
#include <fstream>
#include <sstream>
#include <iostream>

#include <filesystem>

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


    bool AssetManager::PackAssets(const std::string& pakFilepath) const
    {
		// Open the output PAK file for writing in binary mode
        std::ofstream pakFile(pakFilepath, std::ios::binary);
        if (!pakFile.is_open()) return false;

		// Prepare the Table of Contents (TOC) and a list of valid file paths
        std::vector<TOCEntry> toc;
        std::vector<std::string> validPaths;

        // Gather valid physical files and calculate file sizes
        for (const auto& [uuid, meta] : m_assetRegistry)
        {
            // Skip virtual sub-meshes (Assimp handles these in memory)
            if (meta.type == Engine::AssetType::Mesh) continue;

            // Skip virtual assets and ensure the file exists on disk
            if (meta.filepath.find("primitive://") == 0 || meta.filepath.find("shader://") == 0 || meta.filepath.find("cubemap://") == 0) continue;

			// If the file exists, we create a TOC entry for it and add it to the list of valid paths to be packed
            if (std::filesystem::exists(meta.filepath))
            {
                TOCEntry entry{};
                entry.uuid = uuid;
                entry.type = static_cast<uint32_t>(meta.type);
                entry.size = std::filesystem::file_size(meta.filepath);

                toc.push_back(entry);
                validPaths.push_back(meta.filepath);
            }
        }

        // Write the Header (Magic Number + Asset Count)
        const char magic[4] = { 'P', 'A', 'K', '1' };
        pakFile.write(magic, 4);

        uint32_t assetCount = static_cast<uint32_t>(toc.size());
        pakFile.write(reinterpret_cast<const char*>(&assetCount), sizeof(uint32_t));

        // Calculate Offsets and Write the TOC
        //uint64_t currentOffset = 4 + sizeof(uint32_t) + (toc.size() * sizeof(TOCEntry));
		// The first byte of raw data starts immediately after the Magic(4) + Count(4) + TOCEntries(28 bytes each)
        uint64_t currentOffset = 4 + sizeof(uint32_t) + (toc.size() * 28);

		// Write each TOC entry to the PAK file
        for (auto& entry : toc)
        {
            entry.offset = currentOffset;
            currentOffset += entry.size;

            pakFile.write(reinterpret_cast<const char*>(&entry.uuid), sizeof(uint64_t));
            pakFile.write(reinterpret_cast<const char*>(&entry.type), sizeof(uint32_t));
            pakFile.write(reinterpret_cast<const char*>(&entry.offset), sizeof(uint64_t));
            pakFile.write(reinterpret_cast<const char*>(&entry.size), sizeof(uint64_t));
        }

        // Append the Raw Binary Data
        for (const std::string& path : validPaths)
        {
            std::ifstream assetFile(path, std::ios::binary);
            if (assetFile.is_open())
            {
                // Rapidly copy the file buffer directly into the archive
                pakFile << assetFile.rdbuf();
                assetFile.close();
            }
        }

        pakFile.close();
        std::cout << "Successfully packed " << assetCount << " assets into " << pakFilepath << std::endl;
        return true;
    }


    bool AssetManager::MountVFS(const std::string& pakFilepath)
    {
        std::ifstream pakFile(pakFilepath, std::ios::binary);
        if (!pakFile.is_open()) {
            return false;
        }

        // Verify the Magic Number
        char magic[5] = { 0 }; // Extra byte for null terminator
        pakFile.read(magic, 4);
        if (std::string(magic) != "PAK1") {
            std::cerr << "VFS Mount Failed: Invalid magic number in " << pakFilepath << std::endl;
            return false;
        }

        // Read the Asset Count
        uint32_t assetCount = 0;
        pakFile.read(reinterpret_cast<char*>(&assetCount), sizeof(uint32_t));

        // Populate the internal VFS Table
        for (uint32_t i = 0; i < assetCount; ++i) {
            TOCEntry entry{};
            pakFile.read(reinterpret_cast<char*>(&entry.uuid), sizeof(uint64_t));
            pakFile.read(reinterpret_cast<char*>(&entry.type), sizeof(uint32_t));
            pakFile.read(reinterpret_cast<char*>(&entry.offset), sizeof(uint64_t));
            pakFile.read(reinterpret_cast<char*>(&entry.size), sizeof(uint64_t));

            m_vfsTable[entry.uuid] = entry;
        }

        m_vfsPakPath = pakFilepath;
        m_useVFS = true;

        std::cout << "Successfully mounted VFS: " << pakFilepath << " (" << assetCount << " assets)" << std::endl;
        return true;
    }


    std::vector<char> AssetManager::ReadAssetFromVFS(UUID handle) const
    {
        std::vector<char> buffer;

        // Check if the asset exists in the VFS Table
        auto it = m_vfsTable.find(handle);
        if (it == m_vfsTable.end()) {
            return buffer; // Return empty buffer if not found
        }

        // Open the archive, seek to the exact offset, and copy the bytes
        std::ifstream pakFile(m_vfsPakPath, std::ios::binary);
        if (pakFile.is_open()) {
            buffer.resize(it->second.size);
            pakFile.seekg(it->second.offset);
            pakFile.read(buffer.data(), it->second.size);
            pakFile.close();
        }

        return buffer;
    }
}