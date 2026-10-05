#pragma once
#include "Engine/Scene/Entity.h"
#include <filesystem>
#include <memory>
namespace NoJob{class Scene;class Mesh;class Material;class PrefabSerializer{public:static bool Save(Entity,const std::filesystem::path&);static Entity Instantiate(Scene&,const std::filesystem::path&,const std::shared_ptr<Mesh>&,const std::shared_ptr<Material>&);};}
