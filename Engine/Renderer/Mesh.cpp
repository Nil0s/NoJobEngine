#include "Engine/Renderer/Mesh.h"
#include "Engine/Renderer/Buffer.h"
#include "Engine/Renderer/VertexArray.h"
#include "Engine/Renderer/BufferLayout.h"
#include <fstream>
#include <sstream>
#include <string>
#include <stdexcept>
#include <glm/geometric.hpp>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

namespace NoJob
{
    Mesh::Mesh(const std::vector<MeshVertex>& vertices,
               const std::vector<std::uint32_t>& indices)
    {
        std::vector<float> data;
        data.reserve(vertices.size() * 8);

        for (const MeshVertex& vertex : vertices)
        {
            data.insert(data.end(),
            {
                vertex.Position.x, vertex.Position.y, vertex.Position.z,
                vertex.Normal.x, vertex.Normal.y, vertex.Normal.z,
                vertex.TexCoord.x, vertex.TexCoord.y
            });
        }

        m_VertexBuffer = VertexBuffer::Create(
            data.data(),
            static_cast<std::uint32_t>(data.size() * sizeof(float)));

        m_IndexBuffer = IndexBuffer::Create(
            indices.data(),
            static_cast<std::uint32_t>(indices.size()));

        m_VertexArray = VertexArray::Create();
        m_VertexArray->SetVertexBuffer(
            m_VertexBuffer,
            BufferLayout{
                { ShaderDataType::Float3, "a_Position" },
                { ShaderDataType::Float3, "a_Normal" },
                { ShaderDataType::Float2, "a_TexCoord" }
            });
        m_VertexArray->SetIndexBuffer(m_IndexBuffer);
    }


    std::shared_ptr<Mesh> Mesh::LoadOBJ(const std::filesystem::path& path)
    {
        std::ifstream input(path); if(!input) throw std::runtime_error("Could not open OBJ: "+path.string());
        std::vector<glm::vec3> ps,ns; std::vector<glm::vec2> uvs; std::vector<MeshVertex> vs; std::vector<std::uint32_t> is;
        struct C{int p=0,t=0,n=0;};
        auto parse=[](const std::string& q){C c{};std::stringstream s(q);std::string x;if(std::getline(s,x,'/')&&!x.empty())c.p=std::stoi(x);if(std::getline(s,x,'/')&&!x.empty())c.t=std::stoi(x);if(std::getline(s,x,'/')&&!x.empty())c.n=std::stoi(x);return c;};
        auto idx=[](int i,std::size_t n){return i>0?std::size_t(i-1):(i<0?std::size_t((long long)n+i):n);};
        std::string line;while(std::getline(input,line)){if(line.empty()||line[0]=='#')continue;std::istringstream s(line);std::string k;s>>k;
          if(k=="v"){glm::vec3 v{};s>>v.x>>v.y>>v.z;ps.push_back(v);}else if(k=="vn"){glm::vec3 n{};s>>n.x>>n.y>>n.z;ns.push_back(n);}else if(k=="vt"){glm::vec2 u{};s>>u.x>>u.y;u.y=1-u.y;uvs.push_back(u);}
          else if(k=="f"){std::vector<C> f;std::string q;while(s>>q)f.push_back(parse(q));for(std::size_t a=1;a+1<f.size();++a){C cs[3]={f[0],f[a],f[a+1]};MeshVertex v[3]{};bool hn=true;
            for(int j=0;j<3;++j){auto pi=idx(cs[j].p,ps.size());if(pi>=ps.size())throw std::runtime_error("Bad OBJ index");v[j].Position=ps[pi];if(cs[j].t){auto ti=idx(cs[j].t,uvs.size());if(ti<uvs.size())v[j].TexCoord=uvs[ti];}if(cs[j].n){auto ni=idx(cs[j].n,ns.size());if(ni<ns.size())v[j].Normal=ns[ni];else hn=false;}else hn=false;}
            if(!hn){auto n=glm::cross(v[1].Position-v[0].Position,v[2].Position-v[0].Position);n=glm::dot(n,n)>.000001f?glm::normalize(n):glm::vec3(0,1,0);v[0].Normal=v[1].Normal=v[2].Normal=n;}for(auto& x:v){is.push_back((std::uint32_t)vs.size());vs.push_back(x);}}}}
        if(vs.empty())throw std::runtime_error("OBJ has no triangles");return std::make_shared<Mesh>(vs,is);
    }


    std::shared_ptr<Mesh> Mesh::LoadModel(const std::filesystem::path& path)
    {
        Assimp::Importer importer;
        const aiScene* scene = importer.ReadFile(
            path.string(),
            aiProcess_Triangulate | aiProcess_JoinIdenticalVertices |
            aiProcess_GenSmoothNormals | aiProcess_ImproveCacheLocality |
            aiProcess_SortByPType | aiProcess_PreTransformVertices |
            aiProcess_FlipUVs);
        if (!scene || !scene->HasMeshes())
            throw std::runtime_error("Assimp import failed: " + std::string(importer.GetErrorString()));

        std::vector<MeshVertex> vertices;
        std::vector<std::uint32_t> indices;
        std::vector<Submesh> submeshes;

        for (unsigned mi=0; mi<scene->mNumMeshes; ++mi)
        {
            const aiMesh* s=scene->mMeshes[mi];
            const std::uint32_t base=static_cast<std::uint32_t>(vertices.size());
            const std::uint32_t first=static_cast<std::uint32_t>(indices.size());
            for(unsigned i=0;i<s->mNumVertices;++i)
            {
                MeshVertex v{};
                v.Position={s->mVertices[i].x,s->mVertices[i].y,s->mVertices[i].z};
                if(s->HasNormals()) v.Normal={s->mNormals[i].x,s->mNormals[i].y,s->mNormals[i].z};
                if(s->HasTextureCoords(0)) v.TexCoord={s->mTextureCoords[0][i].x,s->mTextureCoords[0][i].y};
                vertices.push_back(v);
            }
            for(unsigned f=0;f<s->mNumFaces;++f)
            {
                const aiFace& face=s->mFaces[f];
                if(face.mNumIndices!=3) continue;
                indices.push_back(base+face.mIndices[0]);
                indices.push_back(base+face.mIndices[1]);
                indices.push_back(base+face.mIndices[2]);
            }
            const auto count=static_cast<std::uint32_t>(indices.size())-first;
            if(count) submeshes.push_back({first,count,s->mMaterialIndex});
        }
        if(vertices.empty()||indices.empty()) throw std::runtime_error("Imported model has no triangles.");
        auto mesh=std::make_shared<Mesh>(vertices,indices);
        mesh->SetSubmeshes(std::move(submeshes));
        return mesh;
    }

    std::shared_ptr<Mesh> Mesh::CreateCube()
    {
        constexpr float h = 0.5f;
        const std::vector<MeshVertex> vertices =
        {
            // Front
            {{-h,-h, h},{ 0, 0, 1},{0,0}}, {{ h,-h, h},{ 0, 0, 1},{1,0}},
            {{ h, h, h},{ 0, 0, 1},{1,1}}, {{-h, h, h},{ 0, 0, 1},{0,1}},
            // Back
            {{ h,-h,-h},{ 0, 0,-1},{0,0}}, {{-h,-h,-h},{ 0, 0,-1},{1,0}},
            {{-h, h,-h},{ 0, 0,-1},{1,1}}, {{ h, h,-h},{ 0, 0,-1},{0,1}},
            // Left
            {{-h,-h,-h},{-1, 0, 0},{0,0}}, {{-h,-h, h},{-1, 0, 0},{1,0}},
            {{-h, h, h},{-1, 0, 0},{1,1}}, {{-h, h,-h},{-1, 0, 0},{0,1}},
            // Right
            {{ h,-h, h},{ 1, 0, 0},{0,0}}, {{ h,-h,-h},{ 1, 0, 0},{1,0}},
            {{ h, h,-h},{ 1, 0, 0},{1,1}}, {{ h, h, h},{ 1, 0, 0},{0,1}},
            // Top
            {{-h, h, h},{ 0, 1, 0},{0,0}}, {{ h, h, h},{ 0, 1, 0},{1,0}},
            {{ h, h,-h},{ 0, 1, 0},{1,1}}, {{-h, h,-h},{ 0, 1, 0},{0,1}},
            // Bottom
            {{-h,-h,-h},{ 0,-1, 0},{0,0}}, {{ h,-h,-h},{ 0,-1, 0},{1,0}},
            {{ h,-h, h},{ 0,-1, 0},{1,1}}, {{-h,-h, h},{ 0,-1, 0},{0,1}}
        };

        std::vector<std::uint32_t> indices;
        indices.reserve(36);
        for (std::uint32_t face = 0; face < 6; ++face)
        {
            const std::uint32_t b = face * 4;
            indices.insert(indices.end(), { b, b+1, b+2, b+2, b+3, b });
        }

        return std::make_shared<Mesh>(vertices, indices);
    }
}
