#include "Engine/AI/Navigation/NavMeshGeometryExtractor.h"

#include "Engine/Asset/AssetManager.h"
#include "Engine/Renderer/Mesh.h"
#include "Engine/Scene/Components.h"
#include "Engine/Scene/Scene.h"

#include <cstdint>
#include <filesystem>

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <glm/glm.hpp>

namespace NoJob
{
    NavMeshSourceGeometry NavMeshGeometryExtractor::Extract(
        Scene& scene)
    {
        NavMeshSourceGeometry geometry;

        for (Entity entity : scene.GetEntities())
        {
            if (!entity.HasComponent<MeshComponent>())
                continue;

            const auto& meshComponent =
                entity.GetComponent<MeshComponent>();

            if (!meshComponent.MeshAsset)
                continue;

            const std::filesystem::path meshPath =
                AssetManager::GetMeshPath(
                    meshComponent.MeshAsset);

            // Procedural meshes currently have no source asset.
            // They are intentionally ignored by the first navigation
            // geometry extraction implementation.
            if (meshPath.empty())
                continue;

            const std::filesystem::path absolutePath =
                AssetManager::ResolveProjectPath(meshPath);

            Assimp::Importer importer;

            const aiScene* importedScene =
                importer.ReadFile(
                    absolutePath.string(),
                    aiProcess_Triangulate |
                    aiProcess_JoinIdenticalVertices |
                    aiProcess_SortByPType);

            if (!importedScene ||
                !importedScene->HasMeshes())
            {
                continue;
            }

            const glm::mat4 worldTransform =
                scene.GetWorldTransform(entity);

            for (unsigned int meshIndex = 0;
                meshIndex < importedScene->mNumMeshes;
                ++meshIndex)
            {
                const aiMesh* sourceMesh =
                    importedScene->mMeshes[meshIndex];

                if (!sourceMesh)
                    continue;

                const std::uint32_t baseVertex =
                    static_cast<std::uint32_t>(
                        geometry.Vertices.size());

                for (unsigned int vertexIndex = 0;
                    vertexIndex < sourceMesh->mNumVertices;
                    ++vertexIndex)
                {
                    const aiVector3D& sourcePosition =
                        sourceMesh->mVertices[vertexIndex];

                    const glm::vec4 localPosition(
                        sourcePosition.x,
                        sourcePosition.y,
                        sourcePosition.z,
                        1.0f);

                    const glm::vec4 worldPosition =
                        worldTransform * localPosition;

                    geometry.Vertices.emplace_back(
                        worldPosition.x,
                        worldPosition.y,
                        worldPosition.z);
                }

                for (unsigned int faceIndex = 0;
                    faceIndex < sourceMesh->mNumFaces;
                    ++faceIndex)
                {
                    const aiFace& face =
                        sourceMesh->mFaces[faceIndex];

                    if (face.mNumIndices != 3)
                        continue;

                    geometry.Indices.push_back(
                        baseVertex +
                        face.mIndices[0]);

                    geometry.Indices.push_back(
                        baseVertex +
                        face.mIndices[1]);

                    geometry.Indices.push_back(
                        baseVertex +
                        face.mIndices[2]);
                }
            }
        }

        return geometry;
    }
}