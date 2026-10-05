#pragma once
#include <cstdint>
#include <memory>
#include <vector>
#include <filesystem>
#include <glm/glm.hpp>

namespace NoJob
{
    class VertexArray;
    class VertexBuffer;
    class IndexBuffer;

    struct MeshVertex
    {
        glm::vec3 Position{ 0.0f };
        glm::vec3 Normal{ 0.0f, 1.0f, 0.0f };
        glm::vec2 TexCoord{ 0.0f };
        // Up to four skeletal influences. IDs are stored as floats to keep the
        // current renderer vertex-layout API simple; shaders cast them to ints.
        glm::vec4 BoneIDs{ 0.0f };
        glm::vec4 BoneWeights{ 0.0f };
    };

    struct Submesh
    {
        std::uint32_t IndexOffset = 0;
        std::uint32_t IndexCount = 0;
        std::uint32_t MaterialIndex = 0;
    };

    class Mesh
    {
    public:
        Mesh(const std::vector<MeshVertex>& vertices,
             const std::vector<std::uint32_t>& indices);

        const std::shared_ptr<VertexArray>& GetVertexArray() const
        {
            return m_VertexArray;
        }

        static std::shared_ptr<Mesh> CreateCube();
        static std::shared_ptr<Mesh> LoadOBJ(const std::filesystem::path& path);
        static std::shared_ptr<Mesh> LoadModel(const std::filesystem::path& path);

        const std::vector<Submesh>& GetSubmeshes() const { return m_Submeshes; }
        void SetSubmeshes(std::vector<Submesh> submeshes) { m_Submeshes = std::move(submeshes); }
        bool HasSkinning() const { return m_HasSkinning; }
        void SetHasSkinning(bool value) { m_HasSkinning = value; }

    private:
        std::shared_ptr<VertexArray> m_VertexArray;
        std::shared_ptr<VertexBuffer> m_VertexBuffer;
        std::shared_ptr<IndexBuffer> m_IndexBuffer;
        std::vector<Submesh> m_Submeshes;
        bool m_HasSkinning = false;
    };
}
