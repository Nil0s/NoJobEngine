#pragma once
#include <cstdint>
#include <filesystem>
#include <string>

namespace NoJob
{
    using AssetHandle = std::uint64_t;
    constexpr AssetHandle InvalidAssetHandle = 0;

    enum class AssetType : std::uint8_t
    {
        Unknown = 0, Texture2D, Mesh, Material, Scene, Prefab, Script, Audio
    };

    struct AssetMetadata
    {
        AssetHandle Handle = InvalidAssetHandle;
        AssetType Type = AssetType::Unknown;
        std::filesystem::path Path;
        std::string Name;
        std::uint64_t FileSize = 0;
        std::uint64_t LastWriteTime = 0;
    };

    inline const char* AssetTypeName(AssetType type)
    {
        switch(type)
        {
            case AssetType::Texture2D: return "Texture2D";
            case AssetType::Mesh: return "Mesh";
            case AssetType::Material: return "Material";
            case AssetType::Scene: return "Scene";
            case AssetType::Prefab: return "Prefab";
            case AssetType::Script: return "Script";
            case AssetType::Audio: return "Audio";
            default: return "Unknown";
        }
    }
}
