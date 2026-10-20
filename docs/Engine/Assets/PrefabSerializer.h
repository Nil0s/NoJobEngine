#pragma once
#include "Engine/Scene/Entity.h"
#include <filesystem>
#include <memory>
namespace NoJob {
class Scene; class Mesh; class Material;
class PrefabSerializer {
public:
 static bool Save(Entity root,const std::filesystem::path&);
 static Entity Instantiate(Scene&,const std::filesystem::path&,const std::shared_ptr<Mesh>&,const std::shared_ptr<Material>&);
 static bool Apply(Entity instance,const std::filesystem::path&);
 static Entity Revert(Entity instance,const std::shared_ptr<Mesh>& fallback,const std::shared_ptr<Material>& base);
};
}
