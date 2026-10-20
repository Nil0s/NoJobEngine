#pragma once
#include <cstdint>
#include <string>
#include <glm/glm.hpp>

namespace NoJob
{
    struct AudioSourceComponent;

    class AudioEngine
    {
    public:
        static bool Init();
        static void Shutdown();
        static bool IsInitialized();

        static bool Play(std::uint32_t entityHandle, const AudioSourceComponent& source,
                         const glm::vec3& position = glm::vec3(0.0f));
        static void Stop(std::uint32_t entityHandle);
        static void StopAll();
        static void Update(std::uint32_t entityHandle, const AudioSourceComponent& source,
                           const glm::vec3& position = glm::vec3(0.0f),
                           const glm::vec3& velocity = glm::vec3(0.0f));
        static void SetListener(const glm::vec3& position,
                                const glm::vec3& forward,
                                const glm::vec3& up,
                                const glm::vec3& velocity = glm::vec3(0.0f));
        static bool IsPlaying(std::uint32_t entityHandle);
        static const std::string& GetLastError();
    };
}
