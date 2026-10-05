#pragma once
#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace NoJob
{
    struct AnimationVecKey { double Time = 0.0; glm::vec3 Value{0.0f}; };
    struct AnimationQuatKey { double Time = 0.0; glm::quat Value{1.0f,0.0f,0.0f,0.0f}; };

    struct AnimationChannel
    {
        std::string NodeName;
        std::vector<AnimationVecKey> Positions;
        std::vector<AnimationQuatKey> Rotations;
        std::vector<AnimationVecKey> Scales;
    };

    struct AnimationClip
    {
        std::string Name;
        double DurationTicks = 0.0;
        double TicksPerSecond = 25.0;
        std::vector<AnimationChannel> Channels;
        double DurationSeconds() const
        {
            return TicksPerSecond > 0.0 ? DurationTicks / TicksPerSecond : 0.0;
        }
    };

    struct SkeletonBone
    {
        std::string Name;
        int Parent = -1;
        glm::mat4 LocalBind{1.0f};
        glm::mat4 Offset{1.0f};
    };

    class AnimationAsset
    {
    public:
        static std::shared_ptr<AnimationAsset> Load(const std::filesystem::path& path);
        const std::vector<SkeletonBone>& Bones() const { return m_Bones; }
        const std::vector<AnimationClip>& Clips() const { return m_Clips; }
        const std::filesystem::path& SourcePath() const { return m_SourcePath; }
        bool HasAnimations() const { return !m_Clips.empty(); }
        std::vector<glm::mat4> EvaluatePose(int clipIndex, float timeSeconds) const;
    private:
        std::filesystem::path m_SourcePath;
        std::vector<SkeletonBone> m_Bones;
        std::vector<AnimationClip> m_Clips;
        glm::mat4 m_GlobalInverseTransform{1.0f};
    };
}
