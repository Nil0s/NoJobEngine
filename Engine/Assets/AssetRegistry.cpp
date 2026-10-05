#include "Engine/Assets/AssetRegistry.h"
#include <fstream>
#include <iomanip>
#include <algorithm>

namespace NoJob
{
    AssetRegistry::AssetRegistry(std::filesystem::path root):m_Root(std::move(root)){}

    std::string AssetRegistry::Normalize(const std::filesystem::path& p)
    {
        return p.lexically_normal().generic_string();
    }

    AssetHandle AssetRegistry::StableHandle(const std::filesystem::path& p)
    {
        // FNV-1a 64: deterministic across editor sessions and machines.
        std::uint64_t h=1469598103934665603ull;
        for(unsigned char c:Normalize(p)){h^=c;h*=1099511628211ull;}
        return h==0?1:h;
    }

    AssetType AssetRegistry::TypeFromExtension(const std::filesystem::path& p)
    {
        auto e=p.extension().string();
        std::transform(e.begin(),e.end(),e.begin(),[](unsigned char c){return char(std::tolower(c));});
        if(e==".png"||e==".jpg"||e==".jpeg"||e==".bmp"||e==".tga") return AssetType::Texture2D;
        if(e==".obj"||e==".fbx"||e==".gltf"||e==".glb") return AssetType::Mesh;
        if(e==".nojobmat") return AssetType::Material;
        if(e==".nojobscene") return AssetType::Scene;
        if(e==".nojobprefab") return AssetType::Prefab;
        return AssetType::Unknown;
    }

    AssetHandle AssetRegistry::Register(const std::filesystem::path& path,AssetType type)
    {
        auto normalized=path.lexically_normal();
        auto key=Normalize(normalized);
        if(auto it=m_ByPath.find(key);it!=m_ByPath.end()) return it->second;
        AssetHandle handle=StableHandle(normalized);
        while(m_ByHandle.contains(handle) && Normalize(m_ByHandle.at(handle).Path)!=key) ++handle;
        AssetMetadata md;md.Handle=handle;md.Path=normalized;md.Name=normalized.stem().string();
        md.Type=type==AssetType::Unknown?TypeFromExtension(normalized):type;
        std::error_code ec;
        if(std::filesystem::exists(normalized,ec)){
            if(std::filesystem::is_regular_file(normalized,ec)) md.FileSize=std::filesystem::file_size(normalized,ec);
            auto ft=std::filesystem::last_write_time(normalized,ec);
            if(!ec) md.LastWriteTime=(std::uint64_t)ft.time_since_epoch().count();
        }
        m_ByHandle[handle]=md;m_ByPath[key]=handle;return handle;
    }

    void AssetRegistry::Scan()
    {
        std::filesystem::create_directories(m_Root);
        for(auto const& e:std::filesystem::recursive_directory_iterator(m_Root))
            if(e.is_regular_file() && e.path().filename()!="AssetRegistry.nojob")
                Register(e.path());
        Save();
    }

    bool AssetRegistry::Save() const
    {
        std::ofstream out(m_Root/"AssetRegistry.nojob");if(!out)return false;
        out<<"NOJOB_ASSET_REGISTRY 1\n";
        for(auto& [h,m]:m_ByHandle)
            out<<h<<' '<<int(m.Type)<<' '<<std::quoted(m.Path.generic_string())<<' '<<std::quoted(m.Name)<<'\n';
        return true;
    }

    bool AssetRegistry::Load()
    {
        std::ifstream in(m_Root/"AssetRegistry.nojob");if(!in)return false;
        std::string header;std::getline(in,header);if(header.rfind("NOJOB_ASSET_REGISTRY",0)!=0)return false;
        m_ByHandle.clear();m_ByPath.clear();
        AssetHandle h;int ty;std::string path,name;
        while(in>>h>>ty>>std::quoted(path)>>std::quoted(name)){
            AssetMetadata md;md.Handle=h;md.Type=AssetType(ty);md.Path=path;md.Name=name;
            m_ByHandle[h]=md;m_ByPath[Normalize(md.Path)]=h;
        }
        return true;
    }

    AssetHandle AssetRegistry::FindHandle(const std::filesystem::path& p) const
    {auto it=m_ByPath.find(Normalize(p));return it==m_ByPath.end()?InvalidAssetHandle:it->second;}
    const AssetMetadata* AssetRegistry::Find(AssetHandle h) const
    {auto it=m_ByHandle.find(h);return it==m_ByHandle.end()?nullptr:&it->second;}
    const AssetMetadata* AssetRegistry::Find(const std::filesystem::path& p) const
    {auto h=FindHandle(p);return Find(h);}
    std::vector<AssetMetadata> AssetRegistry::GetAll() const
    {std::vector<AssetMetadata> v;v.reserve(m_ByHandle.size());for(auto&[_,m]:m_ByHandle)v.push_back(m);return v;}
}
