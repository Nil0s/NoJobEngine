#include "Engine/Assets/PrefabSerializer.h"
#include "Engine/Scene/Scene.h"
#include "Engine/Scene/Components.h"
#include "Engine/Asset/AssetManager.h"
#include "Engine/Renderer/Material.h"
#include "Engine/Renderer/Texture.h"
#include <fstream>
#include <iomanip>
#include <unordered_map>
#include <algorithm>
#include <functional>
#include "Engine/Animation/Animation.h"

namespace NoJob {
namespace {
void WriteMaterial(std::ostream& o,const std::shared_ptr<Material>& m)
{
 if(!m){o<<"MAT_NONE\n";return;}
 auto c=m->GetColor(); auto e=m->EmissiveColor();
 o<<"MAT "<<c.r<<' '<<c.g<<' '<<c.b<<' '<<c.a<<' '<<m->Metallic()<<' '<<m->Roughness()<<' '
  <<m->AmbientOcclusion()<<' '<<m->NormalStrength()<<' '<<e.r<<' '<<e.g<<' '<<e.b<<' '
  <<m->EmissiveStrength()<<' '<<(m->UseTexture()?1:0)<<' '<<(int)m->SurfaceMode()<<' '<<m->AlphaCutoff()<<'\n';
 o<<"TEX "<<std::quoted(AssetManager::GetTexturePath(m->GetTexture()).generic_string())<<' '
  <<std::quoted(AssetManager::GetTexturePath(m->GetNormalTexture()).generic_string())<<' '
  <<std::quoted(AssetManager::GetTexturePath(m->GetMetallicTexture()).generic_string())<<' '
  <<std::quoted(AssetManager::GetTexturePath(m->GetRoughnessTexture()).generic_string())<<' '
  <<std::quoted(AssetManager::GetTexturePath(m->GetAOTexture()).generic_string())<<' '
  <<std::quoted(AssetManager::GetTexturePath(m->GetEmissiveTexture()).generic_string())<<"\n";
}
void WriteEntity(std::ostream& o,Entity e,Scene& scene,int parentLocal,int& nextLocal)
{
 const int local=nextLocal++;
 auto& tag=e.GetComponent<TagComponent>(); auto& x=e.GetComponent<TransformComponent>();
 o<<"ENTITY "<<local<<' '<<parentLocal<<' '<<std::quoted(tag.Tag)<<'\n';
 o<<"TRANSFORM "<<x.Position.x<<' '<<x.Position.y<<' '<<x.Position.z<<' '
  <<x.Rotation.x<<' '<<x.Rotation.y<<' '<<x.Rotation.z<<' '
  <<x.Scale.x<<' '<<x.Scale.y<<' '<<x.Scale.z<<'\n';
 if(e.HasComponent<MeshComponent>())
 {
  const auto meshPath = AssetManager::GetMeshPath(e.GetComponent<MeshComponent>().MeshAsset).generic_string();
  // Procedural meshes (currently the editor cube) are not registered as file assets.
  // Preserve them explicitly instead of serializing an empty path.
  o<<"MESH "<<std::quoted(meshPath.empty() ? std::string("CUBE") : meshPath)<<"\n";
 }
 else o<<"MESH "<<std::quoted(std::string())<<"\n";
 if(e.HasComponent<MeshRendererComponent>()){
  auto& r=e.GetComponent<MeshRendererComponent>();
  const size_t count=r.Materials.empty()?(r.MaterialAsset?1:0):r.Materials.size();
  o<<"MATERIALS "<<count<<"\n";
  for(size_t i=0;i<count;++i) WriteMaterial(o,r.Materials.empty()?r.MaterialAsset:r.Materials[i]);
 } else o<<"MATERIALS 0\n";
 if(e.HasComponent<AnimatorComponent>())
 {
  auto& a=e.GetComponent<AnimatorComponent>();
  const std::string source=a.Animation ? a.Animation->SourcePath().generic_string() : std::string();
  o<<"ANIMATOR "<<std::quoted(source)<<' '<<a.ClipIndex<<' '<<a.Speed<<' '
   <<(a.Playing?1:0)<<' '<<(a.Loop?1:0)<<"\n";
 }
 if(e.HasComponent<AudioSourceComponent>())
 {
  auto& a=e.GetComponent<AudioSourceComponent>();
  o<<"AUDIO_SOURCE "<<std::quoted(a.ClipPath)<<' '<<a.PlayOnAwake<<' '<<a.Loop<<' '
   <<a.Volume<<' '<<a.Pitch<<' '<<a.SpatialBlend<<' '<<a.MinDistance<<' '
   <<a.MaxDistance<<' '<<a.DopplerFactor<<"\n";
 }
 if(e.HasComponent<AudioListenerComponent>())
 {
  o<<"AUDIO_LISTENER "<<e.GetComponent<AudioListenerComponent>().Enabled<<"\n";
 }
 if(e.HasComponent<ParticleSystemComponent>())
 {
  auto& v=e.GetComponent<ParticleSystemComponent>();
  o<<"PARTICLE_SYSTEM_V2 "<<v.Playing<<' '<<v.Loop<<' '<<v.Duration<<' '<<v.StartLifetime<<' '
   <<v.LifetimeRandom<<' '<<v.StartSpeed<<' '<<v.SpeedRandom<<' '<<v.StartSize<<' '<<v.SizeRandom<<' '
   <<v.StartColor.r<<' '<<v.StartColor.g<<' '<<v.StartColor.b<<' '<<v.StartColor.a<<' '
   <<v.EndColor.r<<' '<<v.EndColor.g<<' '<<v.EndColor.b<<' '<<v.EndColor.a<<' '
   <<v.EndSizeMultiplier<<' '<<v.EmissionRate<<' '<<v.MaxParticles<<' '
   <<v.Direction.x<<' '<<v.Direction.y<<' '<<v.Direction.z<<' '
   <<v.Gravity.x<<' '<<v.Gravity.y<<' '<<v.Gravity.z<<' '
   <<static_cast<int>(v.Shape)<<' '<<v.ShapeRadius<<' '<<v.ConeAngle<<' '
   <<static_cast<int>(v.BlendMode)<<' '<<std::quoted(v.TexturePath)<<"\n";
 }
 if(e.HasComponent<NativeScriptComponent>())
 {
  auto& script=e.GetComponent<NativeScriptComponent>();
  o<<"SCRIPT_V2 "<<script.Enabled<<' '<<std::quoted(script.ScriptName)<<"\n";
  for(const auto& [fieldName,value]:script.Fields)
  {
   o<<"SCRIPT_FIELD "<<std::quoted(fieldName)<<' '<<int(value.Type)<<' ';
   if(value.Type==ScriptFieldType::Float)o<<value.Float;
   else if(value.Type==ScriptFieldType::Int)o<<value.Int;
   else if(value.Type==ScriptFieldType::Bool)o<<value.Bool;
   else o<<value.Vec3.x<<' '<<value.Vec3.y<<' '<<value.Vec3.z;
   o<<"\n";
  }
 }
 o<<"END_ENTITY\n";
 for(Entity child:scene.GetChildren(e)) WriteEntity(o,child,scene,local,nextLocal);
}
std::shared_ptr<Texture2D> LoadTex(const std::string& p,const std::filesystem::path& prefabPath)
{
 if(p.empty()) return {};
 try
 {
  // Current assets normally store project-relative paths.
  auto tex=AssetManager::LoadTexture(p);
  if(tex) return tex;
 }
 catch(...){}
 try
 {
  // Compatibility/fallback for paths that were serialized relative to the prefab itself.
  const auto candidate=(prefabPath.parent_path()/p).lexically_normal();
  if(std::filesystem::exists(candidate)) return AssetManager::LoadTexture(candidate);
 }
 catch(...){}
 return {};
}
}
bool PrefabSerializer::Save(Entity root,const std::filesystem::path& p)
{
 if(!root)return false; std::filesystem::create_directories(p.parent_path()); std::ofstream o(p); if(!o)return false;
 o<<"NOJOB_PREFAB 6\n"; int next=0;
 // Entity keeps its Scene private; derive hierarchy through the root's relationship is not enough.
 // PrefabSerializer is intentionally a Scene friend via Entity access is unavailable, so V4 Save is
 // rooted by recursively following handles through a lightweight local lambda in the public Scene API.
 // Root scene pointer is available here because PrefabSerializer is declared friend below in Entity.h.
 Scene& scene=*root.m_Scene;
 WriteEntity(o,root,scene,-1,next);
 return true;
}
Entity PrefabSerializer::Instantiate(Scene& s,const std::filesystem::path& p,const std::shared_ptr<Mesh>& fallback,const std::shared_ptr<Material>& base)
{
 std::ifstream i(p); if(!i)return{}; std::string magic;int version=0;i>>magic>>version;
 if(magic!="NOJOB_PREFAB") return {};
 if(version<4)
 {
  std::string k,name="Prefab",mp,a,n,me,r,aop,em;
  glm::vec3 pos{},rot{},sc{1},ec{};glm::vec4 col{1};
  float mt=0,ro=.5f,ao=1,ns=1,es=0;bool hm=false,useTexture=false;
  while(i>>k){
   if(k=="NAME")i>>std::quoted(name);
   else if(k=="TRANSFORM")i>>pos.x>>pos.y>>pos.z>>rot.x>>rot.y>>rot.z>>sc.x>>sc.y>>sc.z;
   else if(k=="MESH")i>>std::quoted(mp);
   else if(k=="MATERIAL"){i>>col.r>>col.g>>col.b>>col.a>>mt>>ro>>ao>>ns>>ec.r>>ec.g>>ec.b>>es;hm=true;}
   else if(k=="USE_TEXTURE"){int enabled=0;i>>enabled;useTexture=enabled!=0;}
   else if(k=="TEXTURES")i>>std::quoted(a)>>std::quoted(n)>>std::quoted(me)>>std::quoted(r)>>std::quoted(aop)>>std::quoted(em);
  }
  auto e=s.CreateEntity(name);auto& tr=e.GetComponent<TransformComponent>();tr.Position=pos;tr.Rotation=rot;tr.Scale=sc;
  if(!mp.empty()){try{e.AddComponent<MeshComponent>(mp=="CUBE"?fallback:AssetManager::LoadMesh(mp));}catch(...){if(fallback)e.AddComponent<MeshComponent>(fallback);}}
  if(hm&&base){
   auto m=std::make_shared<Material>(*base);m->GetColor()=col;m->Metallic()=mt;m->Roughness()=ro;m->AmbientOcclusion()=ao;
   m->NormalStrength()=ns;m->EmissiveColor()=ec;m->EmissiveStrength()=es;m->UseTexture()=false;
   m->SetTexture(LoadTex(a,p));if(version<3&&!a.empty())useTexture=true;m->UseTexture()=useTexture&&(m->GetTexture()!=nullptr);
   m->SetNormalTexture(LoadTex(n,p));m->SetMetallicTexture(LoadTex(me,p));m->SetRoughnessTexture(LoadTex(r,p));
   m->SetAOTexture(LoadTex(aop,p));m->SetEmissiveTexture(LoadTex(em,p));e.AddComponent<MeshRendererComponent>(m);
  }
  e.AddComponent<PrefabInstanceComponent>(PrefabInstanceComponent{p.generic_string(),true});
  return e;
 }
 std::unordered_map<int,Entity> made; Entity root{};
 std::string k;
 while(i>>k){
  if(k!="ENTITY"){std::string skip;std::getline(i,skip);continue;}
  int id,parent;i>>id>>parent;std::string name;i>>std::quoted(name);
  Entity e=s.CreateEntity(name);made[id]=e;if(!root)root=e;
  glm::vec3 pos{},rot{},sc{1};std::string meshPath;size_t matCount=0;
  std::string animationPath; int animationClip=0; float animationSpeed=1.0f;
  bool animationPlaying=true,animationLoop=true;
  bool hasScript=false; NativeScriptComponent nativeScript;
  bool hasAudioSource=false; AudioSourceComponent audioSource;
  bool hasAudioListener=false; AudioListenerComponent audioListener;
  bool hasParticleSystem=false; ParticleSystemComponent particleSystem;
  while(i>>k && k!="END_ENTITY"){
   if(k=="TRANSFORM"){i>>pos.x>>pos.y>>pos.z>>rot.x>>rot.y>>rot.z>>sc.x>>sc.y>>sc.z;}
   else if(k=="MESH"){i>>std::quoted(meshPath);}
   else if(k=="ANIMATOR"){
    int playing=1,loop=1;
    i>>std::quoted(animationPath)>>animationClip>>animationSpeed>>playing>>loop;
    animationPlaying=playing!=0; animationLoop=loop!=0;
   }
   else if(k=="AUDIO_SOURCE"){
    i>>std::quoted(audioSource.ClipPath)>>audioSource.PlayOnAwake>>audioSource.Loop
     >>audioSource.Volume>>audioSource.Pitch>>audioSource.SpatialBlend
     >>audioSource.MinDistance>>audioSource.MaxDistance>>audioSource.DopplerFactor;
    hasAudioSource=true;
   }
   else if(k=="AUDIO_LISTENER"){
    i>>audioListener.Enabled; hasAudioListener=true;
   }
   else if(k=="PARTICLE_SYSTEM"){
    i>>particleSystem.Playing>>particleSystem.Loop>>particleSystem.Duration
     >>particleSystem.StartLifetime>>particleSystem.StartSpeed>>particleSystem.StartSize
     >>particleSystem.StartColor.r>>particleSystem.StartColor.g>>particleSystem.StartColor.b>>particleSystem.StartColor.a
     >>particleSystem.EmissionRate>>particleSystem.MaxParticles
     >>particleSystem.Direction.x>>particleSystem.Direction.y>>particleSystem.Direction.z
     >>particleSystem.Gravity.x>>particleSystem.Gravity.y>>particleSystem.Gravity.z;
    hasParticleSystem=true;
   }
   else if(k=="PARTICLE_SYSTEM_V2"){
    int shape=0,blend=0;
    i>>particleSystem.Playing>>particleSystem.Loop>>particleSystem.Duration
     >>particleSystem.StartLifetime>>particleSystem.LifetimeRandom
     >>particleSystem.StartSpeed>>particleSystem.SpeedRandom
     >>particleSystem.StartSize>>particleSystem.SizeRandom
     >>particleSystem.StartColor.r>>particleSystem.StartColor.g>>particleSystem.StartColor.b>>particleSystem.StartColor.a
     >>particleSystem.EndColor.r>>particleSystem.EndColor.g>>particleSystem.EndColor.b>>particleSystem.EndColor.a
     >>particleSystem.EndSizeMultiplier>>particleSystem.EmissionRate>>particleSystem.MaxParticles
     >>particleSystem.Direction.x>>particleSystem.Direction.y>>particleSystem.Direction.z
     >>particleSystem.Gravity.x>>particleSystem.Gravity.y>>particleSystem.Gravity.z
     >>shape>>particleSystem.ShapeRadius>>particleSystem.ConeAngle>>blend>>std::quoted(particleSystem.TexturePath);
    particleSystem.Shape=static_cast<ParticleShape>(shape);
    particleSystem.BlendMode=static_cast<ParticleBlendMode>(blend);
    hasParticleSystem=true;
   }
   else if(k=="SCRIPT_V2"){
    i>>nativeScript.Enabled>>std::quoted(nativeScript.ScriptName); hasScript=true;
   }
   else if(k=="SCRIPT_FIELD" && hasScript){
    std::string fieldName; int type=0; i>>std::quoted(fieldName)>>type;
    ScriptFieldValue value; value.Type=ScriptFieldType(type);
    if(value.Type==ScriptFieldType::Float)i>>value.Float;
    else if(value.Type==ScriptFieldType::Int)i>>value.Int;
    else if(value.Type==ScriptFieldType::Bool)i>>value.Bool;
    else i>>value.Vec3.x>>value.Vec3.y>>value.Vec3.z;
    nativeScript.Fields[fieldName]=value;
   }
   else if(k=="MATERIALS"){
    i>>matCount; std::vector<std::shared_ptr<Material>> mats;
    for(size_t m=0;m<matCount;++m){
     std::string mk;i>>mk;if(mk=="MAT_NONE"){mats.push_back({});continue;}
     if(mk!="MAT")continue;
     auto mat=std::make_shared<Material>(*base);auto& c=mat->GetColor();auto& ec=mat->EmissiveColor();
     int use=0,mode=0;i>>c.r>>c.g>>c.b>>c.a>>mat->Metallic()>>mat->Roughness()>>mat->AmbientOcclusion()
      >>mat->NormalStrength()>>ec.r>>ec.g>>ec.b>>mat->EmissiveStrength()>>use>>mode>>mat->AlphaCutoff();
     mat->SurfaceMode()=(MaterialSurfaceMode)mode;
     std::string tk,a,n,me,r,ao,em;i>>tk>>std::quoted(a)>>std::quoted(n)>>std::quoted(me)>>std::quoted(r)>>std::quoted(ao)>>std::quoted(em);
     mat->SetTexture(LoadTex(a,p));mat->UseTexture()=use&&mat->GetTexture();
     mat->SetNormalTexture(LoadTex(n,p));mat->SetMetallicTexture(LoadTex(me,p));mat->SetRoughnessTexture(LoadTex(r,p));
     mat->SetAOTexture(LoadTex(ao,p));mat->SetEmissiveTexture(LoadTex(em,p));mats.push_back(mat);
    }
    if(!mats.empty()) e.AddComponent<MeshRendererComponent>().SetMaterials(std::move(mats));
   }
  }
  auto& tr=e.GetComponent<TransformComponent>();tr.Position=pos;tr.Rotation=rot;tr.Scale=sc;
  if(!meshPath.empty()){
   try{e.AddComponent<MeshComponent>(meshPath=="CUBE"?fallback:AssetManager::LoadMesh(meshPath));}
   catch(...){if(fallback)e.AddComponent<MeshComponent>(fallback);}
  }
  if(!animationPath.empty()){
   try{
    const std::filesystem::path animationFilePath(animationPath);
    const auto resolvedAnimationPath=animationFilePath.is_absolute()
     ?animationFilePath
     :(AssetManager::GetProjectRoot()/animationFilePath);
    auto animation=AnimationAsset::Load(std::filesystem::absolute(resolvedAnimationPath).lexically_normal());
    if(animation&&animation->HasAnimations()){
     AnimatorComponent animator;
     animator.Animation=std::move(animation);
     animator.ClipIndex=std::clamp(animationClip,0,(int)animator.Animation->Clips().size()-1);
     animator.TimeSeconds=0.0f;
     animator.Speed=animationSpeed;
     animator.Playing=animationPlaying;
     animator.Loop=animationLoop;
     e.AddComponent<AnimatorComponent>(std::move(animator));
    }
   }catch(...){}
  }
  if(hasAudioSource)e.AddComponent<AudioSourceComponent>(std::move(audioSource));
  if(hasAudioListener)e.AddComponent<AudioListenerComponent>(audioListener);
  if(hasParticleSystem)e.AddComponent<ParticleSystemComponent>(particleSystem);
  if(hasScript)e.AddComponent<NativeScriptComponent>(std::move(nativeScript));
  if(parent>=0&&made.contains(parent))s.SetParent(e,made[parent],false);
 }
 if(root)root.AddComponent<PrefabInstanceComponent>(PrefabInstanceComponent{p.generic_string(),true});
 return root;
}
bool PrefabSerializer::Apply(Entity instance,const std::filesystem::path& p){return Save(instance,p);}

Entity PrefabSerializer::Revert(Entity instance,const std::shared_ptr<Mesh>& fallback,const std::shared_ptr<Material>& base)
{
 if(!instance || !instance.HasComponent<PrefabInstanceComponent>()) return {};
 Scene* scene=instance.m_Scene;
 const auto source=instance.GetComponent<PrefabInstanceComponent>().SourcePath;
 Entity parent=scene->GetParent(instance);

 // Destroy the whole current instance hierarchy. Scene::DestroyEntity normally
 // preserves children as roots, which is not what prefab Revert should do.
 std::function<void(Entity)> destroyTree=[&](Entity e){
  auto children=scene->GetChildren(e);
  for(Entity child:children) destroyTree(child);
  scene->DestroyEntity(e);
 };
 destroyTree(instance);

 Entity fresh=Instantiate(*scene,source,fallback,base);
 if(fresh && parent) scene->SetParent(fresh,parent,false);
 return fresh;
}
}
