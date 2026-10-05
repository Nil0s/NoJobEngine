#include "Engine/Assets/MaterialSerializer.h"
#include "Engine/Assets/AssetRegistry.h"
#include "Engine/Renderer/Material.h"
#include "Engine/Renderer/Texture.h"
#include <fstream>
#include <iomanip>
namespace NoJob {
 bool MaterialSerializer::Save(const Material& material,const std::filesystem::path& p,const AssetRegistry&){
   std::ofstream o(p); if(!o) return false;
   // Material currently exposes some editable properties only through non-const
   // accessors. Serialization is read-only, so use a local non-const reference
   // without changing the Material ABI in this compile-fix milestone.
   auto& m = const_cast<Material&>(material);
   const auto c = material.GetColor();
   const auto emissive = m.EmissiveColor();
   o << "NOJOB_MATERIAL 1\n";
   o << "COLOR " << c.r << ' ' << c.g << ' ' << c.b << ' ' << c.a << "\n";
   o << "PBR " << material.Metallic() << ' ' << material.Roughness() << ' '
     << material.AmbientOcclusion() << ' ' << m.NormalStrength() << "\n";
   o << "EMISSIVE " << emissive.r << ' ' << emissive.g << ' ' << emissive.b
     << ' ' << m.EmissiveStrength() << "\n";
   return true;
 }
 std::shared_ptr<Material> MaterialSerializer::Load(const std::filesystem::path& p,const std::shared_ptr<Shader>& shader,AssetRegistry&){
   std::ifstream i(p);if(!i)return {};std::string line;std::getline(i,line);if(line.rfind("NOJOB_MATERIAL",0)!=0)return {};
   auto m=std::make_shared<Material>(shader);std::string k;
   while(i>>k){if(k=="COLOR")i>>m->GetColor().r>>m->GetColor().g>>m->GetColor().b>>m->GetColor().a;else if(k=="PBR")i>>m->Metallic()>>m->Roughness()>>m->AmbientOcclusion()>>m->NormalStrength();else if(k=="EMISSIVE")i>>m->EmissiveColor().r>>m->EmissiveColor().g>>m->EmissiveColor().b>>m->EmissiveStrength();}
   return m;
 }
}
