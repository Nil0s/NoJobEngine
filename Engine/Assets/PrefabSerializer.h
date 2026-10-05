#pragma once
#include <filesystem>
#include <memory>
namespace NoJob { class Scene; class Entity; class Mesh; class Material;
class PrefabSerializer {
public:
 static bool Save(Entity entity,const std::filesystem::path& path);
 static Entity Instantiate(Scene& scene,const std::filesystem::path& path,const std::shared_ptr<Mesh>& mesh,const std::shared_ptr<Material>& material);
};}
