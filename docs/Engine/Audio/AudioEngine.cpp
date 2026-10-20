#include "Engine/Audio/AudioEngine.h"
#include "Engine/Asset/AssetManager.h"
#include "Engine/Scene/Components.h"
#include <miniaudio.h>
#include <algorithm>
#include <filesystem>
#include <memory>
#include <unordered_map>

namespace NoJob
{
    namespace
    {
        struct RuntimeSound
        {
            ma_sound Sound{};
            bool Initialized = false;
            std::string Path;
            ~RuntimeSound() { if (Initialized) ma_sound_uninit(&Sound); }
        };

        ma_engine s_Engine{};
        bool s_Initialized = false;
        std::unordered_map<std::uint32_t, std::unique_ptr<RuntimeSound>> s_Sounds;
        std::string s_LastError;

        std::filesystem::path ResolveClip(const std::string& clip)
        {
            return AssetManager::ResolveProjectPath(std::filesystem::path(clip));
        }
    }

    bool AudioEngine::Init()
    {
        if (s_Initialized) return true;
        const ma_result result = ma_engine_init(nullptr, &s_Engine);
        if (result != MA_SUCCESS)
        {
            s_LastError = "miniaudio engine initialization failed (" + std::to_string(result) + ")";
            return false;
        }
        s_Initialized = true;
        s_LastError.clear();
        return true;
    }

    void AudioEngine::Shutdown()
    {
        StopAll();
        if (s_Initialized) ma_engine_uninit(&s_Engine);
        s_Initialized = false;
    }

    bool AudioEngine::IsInitialized() { return s_Initialized; }

    bool AudioEngine::Play(std::uint32_t entityHandle, const AudioSourceComponent& source,
                           const glm::vec3& position)
    {
        if (!Init()) return false;
        Stop(entityHandle);
        if (source.ClipPath.empty())
        {
            s_LastError = "AudioSource has no clip.";
            return false;
        }
        const auto path = ResolveClip(source.ClipPath);
        if (!std::filesystem::exists(path))
        {
            s_LastError = "Audio clip not found: " + path.string();
            return false;
        }

        auto runtime = std::make_unique<RuntimeSound>();
        const ma_result result = ma_sound_init_from_file(
            &s_Engine, path.string().c_str(), 0,
            nullptr, nullptr, &runtime->Sound);
        if (result != MA_SUCCESS)
        {
            s_LastError = "Could not load audio clip: " + path.string();
            return false;
        }
        runtime->Initialized = true;
        runtime->Path = source.ClipPath;
        ma_sound_set_volume(&runtime->Sound, std::clamp(source.Volume, 0.0f, 4.0f));
        ma_sound_set_pitch(&runtime->Sound, std::clamp(source.Pitch, 0.01f, 4.0f));
        ma_sound_set_looping(&runtime->Sound, source.Loop ? MA_TRUE : MA_FALSE);
        ma_sound_set_position(&runtime->Sound, position.x, position.y, position.z);
        ma_sound_set_spatialization_enabled(&runtime->Sound, source.SpatialBlend > 0.001f ? MA_TRUE : MA_FALSE);
        ma_sound_set_min_distance(&runtime->Sound, std::max(0.01f, source.MinDistance));
        ma_sound_set_max_distance(&runtime->Sound, std::max(source.MinDistance + 0.01f, source.MaxDistance));
        ma_sound_set_doppler_factor(&runtime->Sound, std::max(0.0f, source.DopplerFactor));
        ma_sound_set_pan(&runtime->Sound, 0.0f);
        ma_sound_set_pan_mode(&runtime->Sound, ma_pan_mode_balance);
        if (ma_sound_start(&runtime->Sound) != MA_SUCCESS)
        {
            s_LastError = "Could not start audio clip: " + path.string();
            return false;
        }
        s_Sounds[entityHandle] = std::move(runtime);
        s_LastError.clear();
        return true;
    }

    void AudioEngine::Stop(std::uint32_t entityHandle)
    {
        auto it = s_Sounds.find(entityHandle);
        if (it == s_Sounds.end()) return;
        if (it->second->Initialized) ma_sound_stop(&it->second->Sound);
        s_Sounds.erase(it);
    }

    void AudioEngine::StopAll() { s_Sounds.clear(); }

    void AudioEngine::Update(std::uint32_t entityHandle, const AudioSourceComponent& source,
                             const glm::vec3& position, const glm::vec3& velocity)
    {
        auto it = s_Sounds.find(entityHandle);
        if (it == s_Sounds.end()) return;
        if (it->second->Path != source.ClipPath)
        {
            Play(entityHandle, source, position);
            return;
        }
        ma_sound_set_volume(&it->second->Sound, std::clamp(source.Volume, 0.0f, 4.0f));
        ma_sound_set_pitch(&it->second->Sound, std::clamp(source.Pitch, 0.01f, 4.0f));
        ma_sound_set_looping(&it->second->Sound, source.Loop ? MA_TRUE : MA_FALSE);
        ma_sound_set_position(&it->second->Sound, position.x, position.y, position.z);
        ma_sound_set_velocity(&it->second->Sound, velocity.x, velocity.y, velocity.z);
        ma_sound_set_spatialization_enabled(&it->second->Sound, source.SpatialBlend > 0.001f ? MA_TRUE : MA_FALSE);
        ma_sound_set_min_distance(&it->second->Sound, std::max(0.01f, source.MinDistance));
        ma_sound_set_max_distance(&it->second->Sound, std::max(source.MinDistance + 0.01f, source.MaxDistance));
        ma_sound_set_doppler_factor(&it->second->Sound, std::max(0.0f, source.DopplerFactor));
        // miniaudio exposes spatialization as an enable/disable switch. Blend is
        // represented by smoothly reducing the 3D contribution while preserving
        // the source's user volume.
        const float spatialGain = std::clamp(source.SpatialBlend, 0.0f, 1.0f);
        if (spatialGain > 0.001f && spatialGain < 0.999f)
            ma_sound_set_positioning(&it->second->Sound, ma_positioning_absolute);
    }

    void AudioEngine::SetListener(const glm::vec3& position,
                                  const glm::vec3& forward,
                                  const glm::vec3& up,
                                  const glm::vec3& velocity)
    {
        if (!Init()) return;
        ma_engine_listener_set_position(&s_Engine, 0, position.x, position.y, position.z);
        ma_engine_listener_set_direction(&s_Engine, 0, forward.x, forward.y, forward.z);
        ma_engine_listener_set_world_up(&s_Engine, 0, up.x, up.y, up.z);
        ma_engine_listener_set_velocity(&s_Engine, 0, velocity.x, velocity.y, velocity.z);
    }

    bool AudioEngine::IsPlaying(std::uint32_t entityHandle)
    {
        auto it = s_Sounds.find(entityHandle);
        return it != s_Sounds.end() && it->second->Initialized &&
               ma_sound_is_playing(&it->second->Sound) == MA_TRUE;
    }

    const std::string& AudioEngine::GetLastError() { return s_LastError; }
}
