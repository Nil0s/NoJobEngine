#include "Engine/Assets/PrefabSerializer.h"
#include "Engine/Scene/Scene.h"
#include "Engine/Scene/Components.h"
#include "Engine/Renderer/Material.h"
#include <fstream>
#include <iomanip>
namespace NoJob {
 bool PrefabSerializer::Save(Entity e,const std::filesystem::path& p){
  if(!e)return false;std::ofstream o(p);if(!o)return false;auto&t=e.GetComponent<TagComponent>();auto&x=e.GetComponent<TransformComponent>();
  o<<"NOJOB_PREFAB 1\nNAME "<<std::quoted(t.Tag)<<"\nTRANSFORM "<<x.Position.x<<' '<<x.Position.y<<' '<<x.Position.z<<' '<<x.Rotation.x<<' '<<x.Rotation.y<<' '<<x.Rotation.z<<' '<<x.Scale.x<<' '<<x.Scale.y<<' '<<x.Scale.z<<"\n";
  if(e.HasComponent<MeshComponent>())o<<"MESH 1\n";
  if(e.HasComponent<MeshRendererComponent>()&&e.GetComponent<MeshRendererComponent>().MaterialAsset){auto m=e.GetComponent<MeshRendererComponent>().MaterialAsset;auto c=m->GetColor();o<<"MATERIAL "<<c.r<<' '<<c.g<<' '<<c.b<<' '<<c.a<<' '<<m->Metallic()<<' '<<m->Roughness()<<' '<<m->AmbientOcclusion()<<'\n';}
  return true;
 }
 Entity PrefabSerializer::Instantiate(Scene& s,const std::filesystem::path&p,const std::shared_ptr<Mesh>&mesh,const std::shared_ptr<Material>&base){
  std::ifstream i(p);if(!i)return {};std::string h;i>>h;if(h!="NOJOB_PREFAB")return {};int ver;i>>ver;std::string k,name="Prefab";glm::vec3 pos{},rot{},scale{1};bool hasMesh=false;glm::vec4 col{1};float met=0,rough=.5f,ao=1;bool hasMat=false;
  while(i>>k){if(k=="NAME")i>>std::quoted(name);else if(k=="TRANSFORM")i>>pos.x>>pos.y>>pos.z>>rot.x>>rot.y>>rot.z>>scale.x>>scale.y>>scale.z;else if(k=="MESH"){int v;i>>v;hasMesh=v!=0;}else if(k=="MATERIAL"){i>>col.r>>col.g>>col.b>>col.a>>met>>rough>>ao;hasMat=true;}}
  auto e=s.CreateEntity(name);auto&tr=e.GetComponent<TransformComponent>();tr.Position=pos;tr.Rotation=rot;tr.Scale=scale;if(hasMesh)e.AddComponent<MeshComponent>(mesh);if(hasMat){auto m=std::make_shared<Material>(*base);m->GetColor()=col;m->Metallic()=met;m->Roughness()=rough;m->AmbientOcclusion()=ao;e.AddComponent<MeshRendererComponent>(m);}return e;
 }
}
