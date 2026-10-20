#pragma once
#include "Engine/Assets/Asset.h"
#include <filesystem>
#include <unordered_map>
#include <vector>

namespace NoJob
{
    class AssetRegistry
    {
    public:
        explicit AssetRegistry(std::filesystem::path root = "Assets");
        void Scan();
        bool Save() const;
        bool Load();

        AssetHandle Register(const std::filesystem::path& path, AssetType type = AssetType::Unknown);
        AssetHandle FindHandle(const std::filesystem::path& path) const;
        const AssetMetadata* Find(AssetHandle handle) const;
        const AssetMetadata* Find(const std::filesystem::path& path) const;
        std::vector<AssetMetadata> GetAll() const;

        const std::filesystem::path& GetRoot() const { return m_Root; }
        static AssetType TypeFromExtension(const std::filesystem::path& path);
    private:
        static AssetHandle StableHandle(const std::filesystem::path& normalized);
        static std::string Normalize(const std::filesystem::path& path);
        std::filesystem::path m_Root;
        std::unordered_map<AssetHandle,AssetMetadata> m_ByHandle;
        std::unordered_map<std::string,AssetHandle> m_ByPath;
    };
}
