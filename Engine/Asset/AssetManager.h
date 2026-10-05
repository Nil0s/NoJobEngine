#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace NoJob
{
    class Texture2D;
    class Mesh;
    class Shader;
    class Material;

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
        static std::filesystem::path ImportModel(const std::filesystem::path& sourcePath);

        // Reads the first material referenced by an imported model, imports
        // its external/embedded textures and creates a persistent .nojobmat.
        // This is the V1.1 automatic material pipeline. Multi-material
        // submeshes are intentionally the next renderer-level extension.
        static std::shared_ptr<Material> ImportModelMaterial(
            const std::filesystem::path& modelPath,
            const std::shared_ptr<Shader>& shader);
        static std::vector<std::shared_ptr<Material>> ImportModelMaterials(
            const std::filesystem::path& modelPath,
            const std::shared_ptr<Shader>& shader);
        static std::shared_ptr<Mesh> LoadMesh(const std::filesystem::path& path);
        static std::filesystem::path GetMeshPath(const std::shared_ptr<Mesh>& mesh);

        // Loads/caches a texture using a project-relative or absolute path.
        static std::shared_ptr<Texture2D> LoadTexture(
            const std::filesystem::path& path);

        static std::filesystem::path ToProjectRelative(
            const std::filesystem::path& path);

        // Returns the canonical project-relative source path for a texture
        // loaded by AssetManager. Empty means procedural/non-persistent.
        static std::filesystem::path GetTexturePath(
            const std::shared_ptr<Texture2D>& texture);

    private:
        static std::filesystem::path MakeUniqueDestination(
            const std::filesystem::path& directory,
            const std::filesystem::path& filename);

        static std::filesystem::path s_ProjectRoot;
        static std::filesystem::path s_AssetsDirectory;
        static std::unordered_map<
            std::string,
            std::weak_ptr<Texture2D>> s_TextureCache;
        static std::unordered_map<
            const Texture2D*,
            std::filesystem::path> s_TexturePaths;
        static std::unordered_map<std::string,std::weak_ptr<Mesh>> s_MeshCache;
        static std::unordered_map<const Mesh*,std::filesystem::path> s_MeshPaths;
    };
}
