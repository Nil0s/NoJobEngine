#pragma once
#include <filesystem>
#include <memory>

namespace NoJob
{
    class Scene;
    class Mesh;
    class Material;

    class SceneSerializer
    {
    public:
        static bool Save(const Scene& scene, const std::filesystem::path& path);
        static bool Load(
            Scene& scene,
            const std::filesystem::path& path,
            const std::shared_ptr<Mesh>& defaultMesh,
            const std::shared_ptr<Material>& defaultMaterial);
    };
}
