/**
 * @file SceneSerializer.h
 * @brief Scene serialization and deserialization.
 * @ingroup Scene
 */
#pragma once
#include <filesystem>
#include <memory>
#include <iosfwd>

namespace NoJob
{
    class Scene;
    class Mesh;
    class Material;

    class SceneSerializer
    {
    public:
        /** @brief Write the scene format to a stream for saving and unsaved-change comparisons. */
        static bool Serialize(const Scene& scene, std::ostream& out);
        static bool Save(const Scene& scene, const std::filesystem::path& path);
        static bool Load(
            Scene& scene,
            const std::filesystem::path& path,
            const std::shared_ptr<Mesh>& defaultMesh,
            const std::shared_ptr<Material>& defaultMaterial);
    };
}
