#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>

namespace NoJob
{
    class Texture2D;

    class AssetManager
    {
    public:
        static void Init(const std::filesystem::path& projectRoot);

        static const std::filesystem::path& GetProjectRoot();
        static const std::filesystem::path& GetAssetsDirectory();

        // Copies an external image into Assets/Textures and returns
        // a project-relative path such as Assets/Textures/brick.png.
        static std::filesystem::path ImportTexture(
            const std::filesystem::path& sourcePath);

        // Loads/caches a texture using a project-relative or absolute path.
        static std::shared_ptr<Texture2D> LoadTexture(
            const std::filesystem::path& path);

        static std::filesystem::path ToProjectRelative(
            const std::filesystem::path& path);

    private:
        static std::filesystem::path MakeUniqueDestination(
            const std::filesystem::path& directory,
            const std::filesystem::path& filename);

        static std::filesystem::path s_ProjectRoot;
        static std::filesystem::path s_AssetsDirectory;
        static std::unordered_map<
            std::string,
            std::weak_ptr<Texture2D>> s_TextureCache;
    };
}
