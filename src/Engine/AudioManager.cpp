#define MINIAUDIO_IMPLEMENTATION // This tells miniaudio to generate the implementation in this file. Only do this in one .cpp file.
#include <miniaudio.h>
#include "Engine/AudioManager.h"
#include "Engine/AssetManager.h"
#include <iostream>

namespace Engine
{
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
            // ma_engine_play_sound acts as a simple "fire-and-forget" function.
        // It streams or loads the file automatically and cleans up when finished.
            ma_engine_play_sound(m_engine, meta->filepath.c_str(), NULL);
        }
    }


    void* AudioManager::LoadSound(UUID assetID, Engine::AssetManager& assetManager, bool is3D, bool loop)
    {
        if (!m_engine) return nullptr;

        // Extract the filepath from the central asset ledger
        const AssetMetadata* meta = assetManager.GetMetadata(assetID);
        if (!meta || meta->filepath.empty()) return nullptr;

        // Allocate the sound struct on the heap so it persists
        ma_sound* sound = new ma_sound();

        ma_uint32 flags = 0;
        if (!is3D) {
            flags |= MA_SOUND_FLAG_NO_SPATIALIZATION; // Play as standard 2D sound if requested
        }

        // Initialize the sound using the retrieved filepath
        ma_result result = ma_sound_init_from_file(m_engine, meta->filepath.c_str(), flags, NULL, NULL, sound);
        if (result != MA_SUCCESS) {
            delete sound;
            return nullptr;
        }

        ma_sound_set_looping(sound, loop ? MA_TRUE : MA_FALSE);
        return sound; // Return as an opaque void* pointer
    }


    void AudioManager::PlayAudio(void* soundHandle)
    {
        if (soundHandle) {
            ma_sound_start(static_cast<ma_sound*>(soundHandle));
        }
    }


    void AudioManager::StopAudio(void* soundHandle)
    {
        if (soundHandle) {
            ma_sound_stop(static_cast<ma_sound*>(soundHandle));
        }
    }


    void AudioManager::SetAudioPosition(void* soundHandle, float x, float y, float z)
    {
        if (soundHandle) {
            ma_sound_set_position(static_cast<ma_sound*>(soundHandle), x, y, z);
        }
    }


    void AudioManager::SetAudioVolume(void* soundHandle, float volume)
    {
        if (soundHandle) {
            // miniaudio natively clamps and handles volume scaling
            ma_sound_set_volume(static_cast<ma_sound*>(soundHandle), volume);
        }
    }


    void AudioManager::DestroyAudio(void* soundHandle)
    {
        if (soundHandle) {
            ma_sound* sound = static_cast<ma_sound*>(soundHandle);
            ma_sound_uninit(sound); // Unhook from miniaudio
            delete sound;           // Free heap memory
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