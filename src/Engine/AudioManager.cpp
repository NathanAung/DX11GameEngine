#define MINIAUDIO_IMPLEMENTATION // This tells miniaudio to generate the implementation in this file. Only do this in one .cpp file.
#include <miniaudio.h>
#include "Engine/AudioManager.h"
#include "Engine/AssetManager.h"
#include <iostream>
#include <vector>

namespace Engine
{
    // Internal wrapper to hold the VFS memory buffer alive while miniaudio plays it
    struct AudioInstance
    {
        ma_sound sound;
        ma_decoder decoder;
        std::vector<char> buffer;
        bool usesDecoder = false;
    };


    bool AudioManager::Initialize()
    {
        m_engine = new ma_engine();

        // Initialize the high-level miniaudio engine with default settings
        ma_result result = ma_engine_init(NULL, m_engine);
        if (result != MA_SUCCESS)
        {
            std::fprintf(stderr, "Failed to initialize miniaudio engine. Error code: %d\n", result);
            delete m_engine;
            m_engine = nullptr;
            return false;
        }

        std::printf("Miniaudio Engine initialized successfully.\n");
        return true;
    }


    void AudioManager::Shutdown()
    {
        if (m_engine)
        {
			// Uninitialize the miniaudio engine and free resources
            ma_engine_uninit(m_engine);
            delete m_engine;
            m_engine = nullptr;
        }
    }


    void AudioManager::PlaySound2D(UUID assetID, Engine::AssetManager& assetManager)
    {
        if (!m_engine) return;

        // Extract the filepath from the central asset ledger
        const AssetMetadata* meta = assetManager.GetMetadata(assetID);
        if (meta && !meta->filepath.empty())
        {
            if (assetManager.IsVFSActive()) {
                // Fire-and-forget streams memory unsafely, so we restrict it to ECS components in Distribution builds
                std::cerr << "PlaySound2D is not supported in VFS mode. Use AudioComponent instead." << std::endl;
            }
            else {
                // ma_engine_play_sound acts as a simple "fire-and-forget" function.
                // It streams or loads the file automatically and cleans up when finished.
                ma_engine_play_sound(m_engine, meta->filepath.c_str(), NULL);
            }
        }
    }


    void* AudioManager::LoadSound(UUID assetID, Engine::AssetManager& assetManager, bool is3D, bool loop)
    {
        if (!m_engine) return nullptr;

        // Extract the filepath from the central asset ledger
        const AssetMetadata* meta = assetManager.GetMetadata(assetID);
        if (!meta || meta->filepath.empty()) return nullptr;

        // Allocate our custom wrapper so the memory buffer persists
        AudioInstance* instance = new AudioInstance();

        ma_uint32 flags = 0;
        if (!is3D) {
            flags |= MA_SOUND_FLAG_NO_SPATIALIZATION; // Play as standard 2D sound if requested
        }

        if (assetManager.IsVFSActive())
        {
            // GAME MODE: Read the audio binary blob from the VFS archive
            instance->buffer = assetManager.ReadAssetFromVFS(assetID);
            if (instance->buffer.empty()) {
                delete instance;
                return nullptr;
            }

            // Decode the audio from the held memory buffer
            ma_result result = ma_decoder_init_memory(instance->buffer.data(), instance->buffer.size(), NULL, &instance->decoder);
            if (result != MA_SUCCESS) {
                delete instance;
                return nullptr;
            }
            instance->usesDecoder = true;

            // Initialize the sound using the decoded memory data source instead of a file
            result = ma_sound_init_from_data_source(m_engine, &instance->decoder, flags, NULL, &instance->sound);
            if (result != MA_SUCCESS) {
                ma_decoder_uninit(&instance->decoder);
                delete instance;
                return nullptr;
            }
        }
        else
        {
            // EDITOR MODE: Initialize the sound using the retrieved physical filepath
            ma_result result = ma_sound_init_from_file(m_engine, meta->filepath.c_str(), flags, NULL, NULL, &instance->sound);
            if (result != MA_SUCCESS) {
                delete instance;
                return nullptr;
            }
        }

        ma_sound_set_looping(&instance->sound, loop ? MA_TRUE : MA_FALSE);
        return instance; // Return as an opaque void* pointer
    }


    void AudioManager::PlayAudio(void* soundHandle)
    {
        if (soundHandle) {
			// cast to AudioInstance to access the underlying ma_sound for playback
            ma_sound_start(&static_cast<AudioInstance*>(soundHandle)->sound);
        }
    }


    void AudioManager::StopAudio(void* soundHandle)
    {
        if (soundHandle) {
            ma_sound_stop(&static_cast<AudioInstance*>(soundHandle)->sound);
        }
    }


    void AudioManager::SetAudioPosition(void* soundHandle, float x, float y, float z)
    {
        if (soundHandle) {
            ma_sound_set_position(&static_cast<AudioInstance*>(soundHandle)->sound, x, y, z);
        }
    }


    void AudioManager::SetAudioVolume(void* soundHandle, float volume)
    {
        if (soundHandle) {
            // miniaudio natively clamps and handles volume scaling
            ma_sound_set_volume(&static_cast<AudioInstance*>(soundHandle)->sound, volume);
        }
    }


    void AudioManager::DestroyAudio(void* soundHandle)
    {
        if (soundHandle) {
            // Clean up miniaudio, the decoder, and the memory buffer simultaneously
            AudioInstance* instance = static_cast<AudioInstance*>(soundHandle);

            ma_sound_uninit(&instance->sound);  // Unhook from miniaudio

            if (instance->usesDecoder) {
                ma_decoder_uninit(&instance->decoder);
            }

            delete instance;    // Free heap memory
        }
    }


    void AudioManager::SetListenerPosition(float px, float py, float pz, float fx, float fy, float fz)
    {
        if (m_engine) {
            ma_engine_listener_set_position(m_engine, 0, px, py, pz);
            ma_engine_listener_set_direction(m_engine, 0, fx, fy, fz);
        }
    }
}