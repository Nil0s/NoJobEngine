#pragma once
#include "Engine/Assets/Asset.h"
#include <filesystem>
#include <memory>
namespace NoJob { class Material; class Shader; class AssetRegistry;
class MaterialSerializer {
public:
 static bool Save(const Material&,const std::filesystem::path&,const AssetRegistry&);
 static std::shared_ptr<Material> Load(const std::filesystem::path&,const std::shared_ptr<Shader>&,AssetRegistry&);
};}
