#include "Engine/Asset/AssetManager.h"

#include "Engine/Renderer/Texture.h"

#include <stdexcept>

namespace NoJob
{
    std::filesystem::path AssetManager::s_ProjectRoot;
    std::filesystem::path AssetManager::s_AssetsDirectory;
    std::unordered_map<
        std::string,
        std::weak_ptr<Texture2D>> AssetManager::s_TextureCache;
    std::unordered_map<
        const Texture2D*,
        std::filesystem::path> AssetManager::s_TexturePaths;

    void AssetManager::Init(const std::filesystem::path& projectRoot)
    {
        s_ProjectRoot = std::filesystem::absolute(projectRoot).lexically_normal();
        s_AssetsDirectory = s_ProjectRoot / "Assets";

        std::filesystem::create_directories(s_AssetsDirectory / "Textures");
        std::filesystem::create_directories(s_AssetsDirectory / "Models");
        std::filesystem::create_directories(s_AssetsDirectory / "Materials");
        std::filesystem::create_directories(s_AssetsDirectory / "Scenes");
    }

    const std::filesystem::path& AssetManager::GetProjectRoot()
    {
        return s_ProjectRoot;
    }

    const std::filesystem::path& AssetManager::GetAssetsDirectory()
    {
        return s_AssetsDirectory;
    }

    std::filesystem::path AssetManager::MakeUniqueDestination(
        const std::filesystem::path& directory,
        const std::filesystem::path& filename)
    {
        std::filesystem::path destination = directory / filename;

        if (!std::filesystem::exists(destination))
            return destination;

        const std::string stem = filename.stem().string();
        const std::string extension = filename.extension().string();

        for (int index = 1; ; ++index)
        {
            destination =
                directory /
                (stem + "_" + std::to_string(index) + extension);

            if (!std::filesystem::exists(destination))
                return destination;
        }
    }

    std::filesystem::path AssetManager::ImportTexture(
        const std::filesystem::path& sourcePath)
    {
        if (s_ProjectRoot.empty())
            throw std::runtime_error("AssetManager has not been initialized.");

        if (!std::filesystem::exists(sourcePath))
            throw std::runtime_error("Texture source file does not exist.");

        const auto texturesDirectory =
            s_AssetsDirectory / "Textures";

        std::filesystem::create_directories(texturesDirectory);

        const auto absoluteSource =
            std::filesystem::absolute(sourcePath).lexically_normal();

        // If it is already inside Assets, do not duplicate it.
        const auto relativeExisting =
            ToProjectRelative(absoluteSource);

        if (!relativeExisting.empty()
            && relativeExisting.native().find(
                std::filesystem::path("Assets").native()) == 0)
        {
            return relativeExisting;
        }

        const auto destination =
            MakeUniqueDestination(
                texturesDirectory,
                sourcePath.filename());

        std::filesystem::copy_file(
            sourcePath,
            destination,
            std::filesystem::copy_options::none);

        return ToProjectRelative(destination);
    }

    std::shared_ptr<Texture2D> AssetManager::LoadTexture(
        const std::filesystem::path& path)
    {
        std::filesystem::path absolutePath = path;

        if (absolutePath.is_relative())
            absolutePath = s_ProjectRoot / absolutePath;

        absolutePath =
            std::filesystem::absolute(absolutePath).lexically_normal();

        const std::string key = absolutePath.generic_string();

        if (const auto found = s_TextureCache.find(key);
            found != s_TextureCache.end())
        {
            if (auto cached = found->second.lock())
                return cached;
        }

        auto texture =
            Texture2D::Create(absolutePath.string());

        s_TextureCache[key] = texture;
        s_TexturePaths[texture.get()] = ToProjectRelative(absolutePath);
        return texture;
    }


    std::filesystem::path AssetManager::GetTexturePath(
        const std::shared_ptr<Texture2D>& texture)
    {
        if (!texture)
            return {};

        const auto found = s_TexturePaths.find(texture.get());
        if (found == s_TexturePaths.end())
            return {};

        return found->second;
    }

    std::filesystem::path AssetManager::ToProjectRelative(
        const std::filesystem::path& path)
    {
        if (s_ProjectRoot.empty())
            return {};

        std::error_code error;
        auto relative =
            std::filesystem::relative(path, s_ProjectRoot, error);

        if (error || relative.empty())
            return {};

        // Paths outside the project start with "..".
        const auto text = relative.generic_string();
        if (text == ".." || text.rfind("../", 0) == 0)
            return {};

        return relative.lexically_normal();
    }
}
