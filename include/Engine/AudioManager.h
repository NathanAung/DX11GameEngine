#pragma once
#include <string>
#include "Engine/UUID.h"

// AudioManager is a simple wrapper around miniaudio's engine for basic sound playback.

// Forward declare miniaudio's engine struct to avoid polluting the whole codebase with miniaudio.h
struct ma_engine;

namespace Engine
{
    class AssetManager;

    class AudioManager
    {
    public:
        AudioManager() = default;
        ~AudioManager() = default;

        bool Initialize();
        void Shutdown();

        // Basic 2D sound playback for testing (Fire-and-forget)
        void PlaySound2D(UUID assetID, Engine::AssetManager& assetManager);

        // 3D Audio & ECS Support
		void* LoadSound(UUID assetID, Engine::AssetManager& assetManager, bool is3D, bool loop); // Returns an opaque handle to the sound instance, void pointer to avoid exposing miniaudio types in the header
        void PlayAudio(void* soundHandle);
        void StopAudio(void* soundHandle);
        void SetAudioPosition(void* soundHandle, float x, float y, float z);
        void SetAudioVolume(void* soundHandle, float volume);
        void DestroyAudio(void* soundHandle);

		// Listener control for 3D spatialization
		// px, py, pz: Listener position in world space
		// fx, fy, fz: Forward direction vector of the listener (should be normalized)
        void SetListenerPosition(float px, float py, float pz, float fx, float fy, float fz);

        // Expose engine pointer for future 3D spatialization and ECS integration
        ma_engine* GetEngine() const { return m_engine; }

    private:
        ma_engine* m_engine = nullptr;
    };
}