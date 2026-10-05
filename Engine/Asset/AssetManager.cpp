#include "Engine/Asset/AssetManager.h"

#include "Engine/Renderer/Texture.h"
#include "Engine/Renderer/Mesh.h"
#include "Engine/Assets/AssetRegistry.h"
#include "Engine/Assets/MaterialSerializer.h"
#include "Engine/Renderer/Material.h"
#include "Engine/Renderer/Shader.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/material.h>
#include <assimp/postprocess.h>

#include <stdexcept>
#include <algorithm>
#include <cctype>
#include <vector>
#include <fstream>
#include <iomanip>

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
    std::unordered_map<std::string,std::weak_ptr<Mesh>> AssetManager::s_MeshCache;
    std::unordered_map<const Mesh*,std::filesystem::path> AssetManager::s_MeshPaths;

    void AssetManager::Init(const std::filesystem::path& projectRoot)
    {
        s_ProjectRoot = std::filesystem::absolute(projectRoot).lexically_normal();
        s_AssetsDirectory = s_ProjectRoot / "Assets";

        std::filesystem::create_directories(s_AssetsDirectory / "Textures");
        std::filesystem::create_directories(s_AssetsDirectory / "Models");
        std::filesystem::create_directories(s_AssetsDirectory / "Materials");
        std::filesystem::create_directories(s_AssetsDirectory / "Scenes");
        std::filesystem::create_directories(s_AssetsDirectory / "Prefabs");
        AssetRegistry registry(s_AssetsDirectory); registry.Load(); registry.Scan();
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

    std::filesystem::path AssetManager::ImportModel(const std::filesystem::path& sourcePath)
    {
        if(!std::filesystem::exists(sourcePath))throw std::runtime_error("Model does not exist");
        std::string ext=sourcePath.extension().string();std::transform(ext.begin(),ext.end(),ext.begin(),[](unsigned char c){return(char)std::tolower(c);});
        static const std::vector<std::string> supported{
            ".obj",".fbx",".gltf",".glb",".dae",".stl",".ply",".3ds",".blend"
        };
        if(std::find(supported.begin(),supported.end(),ext)==supported.end())
            throw std::runtime_error("Unsupported model format: "+ext);
        auto dst=MakeUniqueDestination(s_AssetsDirectory/"Models",sourcePath.filename());
        std::filesystem::copy_file(sourcePath,dst);

        // Preserve the original import location. Assimp material texture paths
        // are commonly relative to the source FBX/OBJ/glTF directory.
        {
            std::ofstream sidecar(dst.string()+".source",std::ios::trunc);
            if(sidecar) sidecar<<std::quoted(std::filesystem::absolute(sourcePath).lexically_normal().generic_string())<<"\n";
        }

        AssetRegistry r(s_AssetsDirectory);
        r.Load();
        r.Register(dst,AssetType::Mesh);
        r.Save();
        return ToProjectRelative(dst);
    }

    std::shared_ptr<Material> AssetManager::ImportModelMaterial(
        const std::filesystem::path& modelPath,
        const std::shared_ptr<Shader>& shader)
    {
        if (!shader)
            return {};

        std::filesystem::path imported =
            modelPath.is_absolute() ? modelPath : s_ProjectRoot / modelPath;
        imported = std::filesystem::absolute(imported).lexically_normal();

        // Prefer the original source when available so relative texture
        // references still resolve even though the model was copied to Assets.
        std::filesystem::path source = imported;
        {
            std::ifstream sidecar(imported.string() + ".source");
            std::string original;
            if (sidecar >> std::quoted(original))
            {
                std::filesystem::path candidate(original);
                if (std::filesystem::exists(candidate))
                    source = candidate;
            }
        }

        Assimp::Importer importer;
        const aiScene* scene = importer.ReadFile(
            source.string(),
            aiProcess_Triangulate |
            aiProcess_GenSmoothNormals |
            aiProcess_JoinIdenticalVertices);

        if (!scene || scene->mNumMaterials == 0)
            return {};

        unsigned materialIndex = 0;
        if (scene->mNumMeshes > 0)
            materialIndex = scene->mMeshes[0]->mMaterialIndex;
        if (materialIndex >= scene->mNumMaterials)
            materialIndex = 0;

        aiMaterial* sourceMaterial = scene->mMaterials[materialIndex];
        auto material = std::make_shared<Material>(shader);

        aiColor4D color;
        if (aiGetMaterialColor(
                sourceMaterial, AI_MATKEY_BASE_COLOR, &color) == AI_SUCCESS ||
            aiGetMaterialColor(
                sourceMaterial, AI_MATKEY_COLOR_DIFFUSE, &color) == AI_SUCCESS)
        {
            material->GetColor() =
                glm::vec4(color.r, color.g, color.b, color.a);
        }

        ai_real value = 0.0f;
        if (aiGetMaterialFloat(
                sourceMaterial, AI_MATKEY_METALLIC_FACTOR, &value) == AI_SUCCESS)
            material->Metallic() = static_cast<float>(value);

        if (aiGetMaterialFloat(
                sourceMaterial, AI_MATKEY_ROUGHNESS_FACTOR, &value) == AI_SUCCESS)
            material->Roughness() =
                std::clamp(static_cast<float>(value), 0.04f, 1.0f);

        aiColor3D emissive(0.0f);
        if (sourceMaterial->Get(
                AI_MATKEY_COLOR_EMISSIVE, emissive) == AI_SUCCESS)
        {
            material->EmissiveColor() =
                glm::vec3(emissive.r, emissive.g, emissive.b);
            const float peak = std::max(
                emissive.r, std::max(emissive.g, emissive.b));
            if (peak > 0.0f)
                material->EmissiveStrength() = 1.0f;
        }

        const auto textureDirectory = s_AssetsDirectory / "Textures";
        std::filesystem::create_directories(textureDirectory);

        auto importTextureReference =
            [&](aiTextureType type) -> std::shared_ptr<Texture2D>
        {
            if (sourceMaterial->GetTextureCount(type) == 0)
                return {};

            aiString textureRef;
            if (sourceMaterial->GetTexture(type, 0, &textureRef) != AI_SUCCESS)
                return {};

            const std::string ref = textureRef.C_Str();
            if (ref.empty())
                return {};

            try
            {
                // Embedded Assimp texture reference, e.g. "*0".
                if (ref[0] == '*')
                {
                    const unsigned index =
                        static_cast<unsigned>(std::stoul(ref.substr(1)));
                    if (index >= scene->mNumTextures)
                        return {};

                    const aiTexture* embedded = scene->mTextures[index];
                    std::string extension =
                        embedded->achFormatHint[0]
                            ? ("." + std::string(embedded->achFormatHint))
                            : ".png";

                    auto destination = MakeUniqueDestination(
                        textureDirectory,
                        source.stem().string() + "_embedded_" +
                            std::to_string(index) + extension);

                    if (embedded->mHeight == 0)
                    {
                        std::ofstream out(destination, std::ios::binary);
                        out.write(
                            reinterpret_cast<const char*>(embedded->pcData),
                            static_cast<std::streamsize>(embedded->mWidth));
                    }
                    else
                    {
                        // Raw aiTexel embedded textures are uncommon for FBX.
                        // They require an image encoder, so keep them as a
                        // graceful unsupported case instead of corrupting data.
                        return {};
                    }

                    AssetRegistry registry(s_AssetsDirectory);
                    registry.Load();
                    registry.Register(destination, AssetType::Texture2D);
                    registry.Save();
                    return LoadTexture(ToProjectRelative(destination));
                }

                std::filesystem::path texturePath(ref);
                if (texturePath.is_relative())
                    texturePath = source.parent_path() / texturePath;
                texturePath =
                    std::filesystem::absolute(texturePath).lexically_normal();

                if (!std::filesystem::exists(texturePath))
                    return {};

                const auto importedTexture = ImportTexture(texturePath);
                AssetRegistry registry(s_AssetsDirectory);
                registry.Load();
                registry.Register(
                    s_ProjectRoot / importedTexture,
                    AssetType::Texture2D);
                registry.Save();
                return LoadTexture(importedTexture);
            }
            catch (...)
            {
                return {};
            }
        };

        float opacity = 1.0f;
        if (aiGetMaterialFloat(sourceMaterial, AI_MATKEY_OPACITY, &opacity) == AI_SUCCESS)
            material->GetColor().a *= std::clamp(opacity, 0.0f, 1.0f);
        aiString alphaMode;
        const bool hasAlphaMode =
            sourceMaterial->Get("$mat.gltf.alphaMode", 0, 0, alphaMode) == AI_SUCCESS;
        const bool hasOpacityMap =
            sourceMaterial->GetTextureCount(aiTextureType_OPACITY) > 0;
        if (hasAlphaMode && std::string(alphaMode.C_Str()) == "BLEND")
            material->SurfaceMode() = MaterialSurfaceMode::Transparent;
        else if ((hasAlphaMode && std::string(alphaMode.C_Str()) == "MASK") || hasOpacityMap)
            material->SurfaceMode() = MaterialSurfaceMode::AlphaClip;
        else if (material->GetColor().a < 0.999f)
            material->SurfaceMode() = MaterialSurfaceMode::Transparent;

        auto albedo = importTextureReference(aiTextureType_BASE_COLOR);
        if (!albedo)
            albedo = importTextureReference(aiTextureType_DIFFUSE);
        if (albedo)
        {
            material->SetTexture(albedo);
            material->UseTexture() = true;
        }
        else
        {
            material->UseTexture() = false;
        }

        auto normal = importTextureReference(aiTextureType_NORMAL_CAMERA);
        if (!normal)
            normal = importTextureReference(aiTextureType_NORMALS);
        material->SetNormalTexture(normal);

        material->SetMetallicTexture(
            importTextureReference(aiTextureType_METALNESS));
        material->SetRoughnessTexture(
            importTextureReference(aiTextureType_DIFFUSE_ROUGHNESS));

        auto ao = importTextureReference(aiTextureType_AMBIENT_OCCLUSION);
        if (!ao)
            ao = importTextureReference(aiTextureType_LIGHTMAP);
        material->SetAOTexture(ao);
        material->SetEmissiveTexture(
            importTextureReference(aiTextureType_EMISSIVE));

        // Persist the generated material next to the rest of the project's
        // materials so it is visible/reusable in Project.
        const auto materialPath = MakeUniqueDestination(
            s_AssetsDirectory / "Materials",
            imported.stem().string() + ".nojobmat");

        AssetRegistry registry(s_AssetsDirectory);
        registry.Load();
        MaterialSerializer::Save(*material, materialPath, registry);
        registry.Register(materialPath, AssetType::Material);
        registry.Save();

        return material;
    }


    std::vector<std::shared_ptr<Material>> AssetManager::ImportModelMaterials(
        const std::filesystem::path& modelPath,
        const std::shared_ptr<Shader>& shader)
    {
        std::vector<std::shared_ptr<Material>> materials;
        if(!shader) return materials;

        std::filesystem::path imported=modelPath.is_absolute()?modelPath:s_ProjectRoot/modelPath;
        imported=std::filesystem::absolute(imported).lexically_normal();
        std::filesystem::path source=imported;
        {
            std::ifstream sidecar(imported.string()+".source");
            std::string original;
            if(sidecar>>std::quoted(original))
            {
                std::filesystem::path candidate(original);
                if(std::filesystem::exists(candidate)) source=candidate;
            }
        }

        Assimp::Importer importer;
        const aiScene* scene=importer.ReadFile(source.string(),
            aiProcess_Triangulate|aiProcess_GenSmoothNormals|aiProcess_JoinIdenticalVertices);
        if(!scene || scene->mNumMaterials==0) return materials;

        const auto textureDirectory=s_AssetsDirectory/"Textures";
        std::filesystem::create_directories(textureDirectory);
        std::filesystem::create_directories(s_AssetsDirectory/"Materials");

        auto loadRef=[&](aiMaterial* sm,aiTextureType type)->std::shared_ptr<Texture2D>
        {
            if(sm->GetTextureCount(type)==0) return {};
            aiString ar; if(sm->GetTexture(type,0,&ar)!=AI_SUCCESS) return {};
            std::string ref=ar.C_Str(); if(ref.empty()) return {};
            try
            {
                if(ref[0]=='*')
                {
                    unsigned idx=static_cast<unsigned>(std::stoul(ref.substr(1)));
                    if(idx>=scene->mNumTextures) return {};
                    const aiTexture* e=scene->mTextures[idx];
                    if(e->mHeight!=0) return {};
                    std::string ext=e->achFormatHint[0]?("."+std::string(e->achFormatHint)):".png";
                    auto dest=MakeUniqueDestination(textureDirectory,
                        source.stem().string()+"_embedded_"+std::to_string(idx)+ext);
                    std::ofstream out(dest,std::ios::binary);
                    out.write(reinterpret_cast<const char*>(e->pcData),
                              static_cast<std::streamsize>(e->mWidth));
                    out.close();
                    AssetRegistry r(s_AssetsDirectory);r.Load();
                    r.Register(dest,AssetType::Texture2D);r.Save();
                    return LoadTexture(ToProjectRelative(dest));
                }
                std::filesystem::path tp(ref);
                if(tp.is_relative()) tp=source.parent_path()/tp;
                tp=std::filesystem::absolute(tp).lexically_normal();
                if(!std::filesystem::exists(tp)) return {};
                auto rel=ImportTexture(tp);
                AssetRegistry r(s_AssetsDirectory);r.Load();
                r.Register(s_ProjectRoot/rel,AssetType::Texture2D);r.Save();
                return LoadTexture(rel);
            }catch(...){return {};}
        };

        for(unsigned i=0;i<scene->mNumMaterials;++i)
        {
            aiMaterial* sm=scene->mMaterials[i];
            auto m=std::make_shared<Material>(shader);
            aiColor4D c;
            if(aiGetMaterialColor(sm,AI_MATKEY_BASE_COLOR,&c)==AI_SUCCESS ||
               aiGetMaterialColor(sm,AI_MATKEY_COLOR_DIFFUSE,&c)==AI_SUCCESS)
                m->GetColor()={c.r,c.g,c.b,c.a};
            ai_real v=0;
            if(aiGetMaterialFloat(sm,AI_MATKEY_METALLIC_FACTOR,&v)==AI_SUCCESS)m->Metallic()=(float)v;
            if(aiGetMaterialFloat(sm,AI_MATKEY_ROUGHNESS_FACTOR,&v)==AI_SUCCESS)m->Roughness()=std::clamp((float)v,0.04f,1.0f);
            aiColor3D ec(0);
            if(sm->Get(AI_MATKEY_COLOR_EMISSIVE,ec)==AI_SUCCESS)
            {m->EmissiveColor()={ec.r,ec.g,ec.b};if(std::max(ec.r,std::max(ec.g,ec.b))>0)m->EmissiveStrength()=1;}

            // Import surface/transparency semantics. FBX commonly exposes
            // foliage masks through aiTextureType_OPACITY; glTF exposes alphaMode.
            float opacity = 1.0f;
            if (aiGetMaterialFloat(sm, AI_MATKEY_OPACITY, &opacity) == AI_SUCCESS)
                m->GetColor().a *= std::clamp(opacity, 0.0f, 1.0f);

            aiString alphaMode;
            const bool hasAlphaMode =
                sm->Get("$mat.gltf.alphaMode", 0, 0, alphaMode) == AI_SUCCESS;
            const bool hasOpacityMap =
                sm->GetTextureCount(aiTextureType_OPACITY) > 0;

            if (hasAlphaMode && std::string(alphaMode.C_Str()) == "BLEND")
                m->SurfaceMode() = MaterialSurfaceMode::Transparent;
            else if (hasAlphaMode && std::string(alphaMode.C_Str()) == "MASK")
                m->SurfaceMode() = MaterialSurfaceMode::AlphaClip;
            else if (hasOpacityMap)
                m->SurfaceMode() = MaterialSurfaceMode::AlphaClip;
            else if (m->GetColor().a < 0.999f)
                m->SurfaceMode() = MaterialSurfaceMode::Transparent;

            ai_real cutoff = 0.5f;
            if (sm->Get("$mat.gltf.alphaCutoff", 0, 0, cutoff) == AI_SUCCESS)
                m->AlphaCutoff() =
                    std::clamp(static_cast<float>(cutoff), 0.0f, 1.0f);

            auto al=loadRef(sm,aiTextureType_BASE_COLOR);
            if(!al) al=loadRef(sm,aiTextureType_DIFFUSE);
            if(al){m->SetTexture(al);m->UseTexture()=true;}else m->UseTexture()=false;
            auto normal=loadRef(sm,aiTextureType_NORMAL_CAMERA);
            if(!normal)normal=loadRef(sm,aiTextureType_NORMALS);
            m->SetNormalTexture(normal);
            m->SetMetallicTexture(loadRef(sm,aiTextureType_METALNESS));
            m->SetRoughnessTexture(loadRef(sm,aiTextureType_DIFFUSE_ROUGHNESS));
            auto ao=loadRef(sm,aiTextureType_AMBIENT_OCCLUSION);
            if(!ao)ao=loadRef(sm,aiTextureType_LIGHTMAP);
            m->SetAOTexture(ao);
            m->SetEmissiveTexture(loadRef(sm,aiTextureType_EMISSIVE));

            aiString nm; std::string name="Material_"+std::to_string(i);
            if(sm->Get(AI_MATKEY_NAME,nm)==AI_SUCCESS && nm.length>0)name=nm.C_Str();
            for(char& ch:name)if(ch=='/'||ch=='\\'||ch==':'||ch=='*'||ch=='?'||ch=='"'||ch=='<'||ch=='>'||ch=='|')ch='_';
            auto mp=MakeUniqueDestination(s_AssetsDirectory/"Materials",
                                          imported.stem().string()+"_"+name+".nojobmat");
            AssetRegistry r(s_AssetsDirectory);r.Load();
            MaterialSerializer::Save(*m,mp,r);r.Register(mp,AssetType::Material);r.Save();
            materials.push_back(m);
        }
        return materials;
    }

    std::shared_ptr<Mesh> AssetManager::LoadMesh(const std::filesystem::path& path)
    {
        auto a=path.is_absolute()?path:s_ProjectRoot/path;a=std::filesystem::absolute(a).lexically_normal();auto key=a.generic_string();
        if(auto it=s_MeshCache.find(key);it!=s_MeshCache.end())if(auto m=it->second.lock())return m;
        auto m=Mesh::LoadModel(a);s_MeshCache[key]=m;s_MeshPaths[m.get()]=ToProjectRelative(a);return m;
    }
    std::filesystem::path AssetManager::GetMeshPath(const std::shared_ptr<Mesh>& m){if(!m)return{};auto i=s_MeshPaths.find(m.get());return i==s_MeshPaths.end()?std::filesystem::path{}:i->second;}

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
