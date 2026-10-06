#include "Engine/Scene/SceneSerializer.h"
#include "Engine/Scene/Scene.h"
#include "Engine/Scene/Components.h"
#include "Engine/Renderer/Material.h"
#include "Engine/Renderer/Mesh.h"
#include "Engine/Asset/AssetManager.h"

#include <fstream>
#include <iomanip>
#include <sstream>
#include <unordered_map>

namespace NoJob
{
    namespace
    {
        template<typename T> void V3(std::ostream& o,const T& v){o<<v.x<<' '<<v.y<<' '<<v.z;}
        bool ReadV3(std::istringstream& s,glm::vec3& v){return bool(s>>v.x>>v.y>>v.z);}
    }

    bool SceneSerializer::Save(const Scene& scene, const std::filesystem::path& path)
    {
        std::ofstream out(path);
        if(!out) return false;
        out<<"NOJOB_SCENE 1\n";
        out<<std::setprecision(9);

        auto& mutableScene=const_cast<Scene&>(scene);
        for(Entity e: mutableScene.GetEntities())
        {
            const auto& tag=e.GetComponent<TagComponent>();
            const auto& tr=e.GetComponent<TransformComponent>();
            const auto& rel=e.GetComponent<RelationshipComponent>();
            out<<"ENTITY "<<e.GetComponent<IDComponent>().ID<<' '<<std::quoted(tag.Tag)<<'\n';
            out<<"TRANSFORM "; V3(out,tr.Position); out<<' '; V3(out,tr.Rotation); out<<' '; V3(out,tr.Scale); out<<'\n';
            out<<"PARENT "<<rel.Parent<<'\n';

            if(e.HasComponent<CameraComponent>()){auto& c=e.GetComponent<CameraComponent>();out<<"CAMERA "<<int(c.ProjectionType)<<' '<<c.Primary<<' '<<c.PerspectiveFOV<<' '<<c.PerspectiveNear<<' '<<c.PerspectiveFar<<' '<<c.OrthographicSize<<' '<<c.OrthographicNear<<' '<<c.OrthographicFar<<'\n';}
            if(e.HasComponent<DirectionalLightComponent>()){auto& c=e.GetComponent<DirectionalLightComponent>();out<<"DIRECTIONAL ";V3(out,c.Color);out<<' '<<c.Intensity<<' '<<c.CastShadows<<' '<<c.ShadowBias<<'\n';}
            if(e.HasComponent<PointLightComponent>()){auto& c=e.GetComponent<PointLightComponent>();out<<"POINT ";V3(out,c.Color);out<<' '<<c.Intensity<<' '<<c.Range<<' '<<c.CastShadows<<' '<<c.ShadowBias<<'\n';}
            if(e.HasComponent<SpotLightComponent>()){auto& c=e.GetComponent<SpotLightComponent>();out<<"SPOT ";V3(out,c.Color);out<<' '<<c.Intensity<<' '<<c.Range<<' '<<c.InnerAngle<<' '<<c.OuterAngle<<' '<<c.CastShadows<<' '<<c.ShadowBias<<'\n';}
            if(e.HasComponent<AudioSourceComponent>()){auto& c=e.GetComponent<AudioSourceComponent>();out<<"AUDIO_SOURCE_V2 "<<std::quoted(c.ClipPath)<<' '<<c.PlayOnAwake<<' '<<c.Loop<<' '<<c.Volume<<' '<<c.Pitch<<' '<<c.SpatialBlend<<' '<<c.MinDistance<<' '<<c.MaxDistance<<' '<<c.DopplerFactor<<'\n';}
            if(e.HasComponent<AudioListenerComponent>()){auto& c=e.GetComponent<AudioListenerComponent>();out<<"AUDIO_LISTENER "<<c.Enabled<<'\n';}
            if(e.HasComponent<ParticleSystemComponent>()){auto& c=e.GetComponent<ParticleSystemComponent>();
                out<<"PARTICLE_SYSTEM_V2 "<<c.Playing<<' '<<c.Loop<<' '<<c.Duration<<' '<<c.StartLifetime<<' '
                   <<c.LifetimeRandom<<' '<<c.StartSpeed<<' '<<c.SpeedRandom<<' '<<c.StartSize<<' '
                   <<c.SizeRandom<<' '<<c.StartColor.r<<' '<<c.StartColor.g<<' '<<c.StartColor.b<<' '
                   <<c.StartColor.a<<' '<<c.EndColor.r<<' '<<c.EndColor.g<<' '<<c.EndColor.b<<' '
                   <<c.EndColor.a<<' '<<c.EndSizeMultiplier<<' '<<c.EmissionRate<<' '<<c.MaxParticles<<' '
                   <<c.Direction.x<<' '<<c.Direction.y<<' '<<c.Direction.z<<' '
                   <<c.Gravity.x<<' '<<c.Gravity.y<<' '<<c.Gravity.z<<' '
                   <<static_cast<int>(c.Shape)<<' '<<c.ShapeRadius<<' '<<c.ConeAngle<<' '
                   <<static_cast<int>(c.BlendMode)<<' '<<std::quoted(c.TexturePath)<<'\n';}
            if(e.HasComponent<NativeScriptComponent>())
            {
                auto& c=e.GetComponent<NativeScriptComponent>();
                out<<"SCRIPT_V2 "<<c.Enabled<<' '<<std::quoted(c.ScriptName)<<' '<<c.Fields.size()<<'\n';
                for(const auto& [name,value]:c.Fields)
                {
                    out<<"SCRIPT_FIELD "<<std::quoted(name)<<' '<<int(value.Type)<<' ';
                    if(value.Type==ScriptFieldType::Float) out<<value.Float;
                    else if(value.Type==ScriptFieldType::Int) out<<value.Int;
                    else if(value.Type==ScriptFieldType::Bool) out<<value.Bool;
                    else V3(out,value.Vec3);
                    out<<'\n';
                }
            }
            if(e.HasComponent<RigidbodyComponent>()){auto& c=e.GetComponent<RigidbodyComponent>();out<<"RIGIDBODY "<<int(c.Type)<<' '<<c.Mass<<' '<<c.UseGravity<<'\n';}
            if(e.HasComponent<BoxColliderComponent>()){auto& c=e.GetComponent<BoxColliderComponent>();out<<"BOX ";V3(out,c.Size);out<<' '<<c.IsTrigger<<' '<<c.Material.Friction<<' '<<c.Material.Bounciness<<'\n';}
            if(e.HasComponent<SphereColliderComponent>()){auto& c=e.GetComponent<SphereColliderComponent>();out<<"SPHERE "<<c.Radius<<' '<<c.IsTrigger<<' '<<c.Material.Friction<<' '<<c.Material.Bounciness<<'\n';}
            if(e.HasComponent<CapsuleColliderComponent>()){auto& c=e.GetComponent<CapsuleColliderComponent>();out<<"CAPSULE "<<c.Radius<<' '<<c.Height<<' '<<c.IsTrigger<<' '<<c.Material.Friction<<' '<<c.Material.Bounciness<<'\n';}
            if(e.HasComponent<MeshComponent>()){auto mp=AssetManager::GetMeshPath(e.GetComponent<MeshComponent>().MeshAsset).generic_string();if(mp.empty())out<<"MESH CUBE\n";else out<<"MESH "<<std::quoted(mp)<<'\n';}
            if(e.HasComponent<MeshRendererComponent>() && e.GetComponent<MeshRendererComponent>().MaterialAsset)
            {
                auto m=e.GetComponent<MeshRendererComponent>().MaterialAsset;
                auto col=m->GetColor();
                out<<"MATERIAL "<<col.r<<' '<<col.g<<' '<<col.b<<' '<<col.a<<' '<<m->Metallic()<<' '<<m->Roughness()<<' '<<m->AmbientOcclusion()<<' '<<m->NormalStrength()<<' ';
                V3(out,m->EmissiveColor()); out<<' '<<m->EmissiveStrength()<<'\n';

                const auto albedo = AssetManager::GetTexturePath(m->GetTexture()).generic_string();
                const auto normal = AssetManager::GetTexturePath(m->GetNormalTexture()).generic_string();
                const auto metallic = AssetManager::GetTexturePath(m->GetMetallicTexture()).generic_string();
                const auto roughness = AssetManager::GetTexturePath(m->GetRoughnessTexture()).generic_string();
                const auto ao = AssetManager::GetTexturePath(m->GetAOTexture()).generic_string();
                const auto emissive = AssetManager::GetTexturePath(m->GetEmissiveTexture()).generic_string();
                out<<"TEXTURES "<<std::quoted(albedo)<<' '<<std::quoted(normal)<<' '
                   <<std::quoted(metallic)<<' '<<std::quoted(roughness)<<' '
                   <<std::quoted(ao)<<' '<<std::quoted(emissive)<<'\n';
            }
            out<<"END\n";
        }
        return true;
    }

    bool SceneSerializer::Load(Scene& scene,const std::filesystem::path& path,const std::shared_ptr<Mesh>& defaultMesh,const std::shared_ptr<Material>& defaultMaterial)
    {
        std::ifstream in(path); if(!in) return false;
        std::string line; std::getline(in,line); if(line.rfind("NOJOB_SCENE",0)!=0) return false;
        for(auto e:scene.GetEntities()) scene.DestroyEntity(e);

        Entity current{}; std::uint32_t parentHandle=0;
        struct ParentRequest{Entity child;std::uint32_t oldParent;};
        std::vector<ParentRequest> parents;
        std::unordered_map<std::uint32_t,Entity> handleMap;
        

        while(std::getline(in,line))
        {
            std::istringstream s(line); std::string k; s>>k;
            if(k=="ENTITY"){std::uint64_t id;std::string name;s>>id>>std::quoted(name);current=scene.CreateEntity(name);handleMap[static_cast<std::uint32_t>(id)]=current;}
            else if(!current) continue;
            else if(k=="TRANSFORM"){auto& c=current.GetComponent<TransformComponent>();ReadV3(s,c.Position);ReadV3(s,c.Rotation);ReadV3(s,c.Scale);}
            else if(k=="PARENT"){s>>parentHandle;if(parentHandle)parents.push_back({current,parentHandle});}
            else if(k=="CAMERA"){CameraComponent c;int p;s>>p>>c.Primary>>c.PerspectiveFOV>>c.PerspectiveNear>>c.PerspectiveFar>>c.OrthographicSize>>c.OrthographicNear>>c.OrthographicFar;c.ProjectionType=CameraProjectionType(p);current.AddComponent<CameraComponent>(c);}
            else if(k=="DIRECTIONAL"){DirectionalLightComponent c;ReadV3(s,c.Color);s>>c.Intensity>>c.CastShadows>>c.ShadowBias;current.AddComponent<DirectionalLightComponent>(c);}
            else if(k=="POINT"){PointLightComponent c;ReadV3(s,c.Color);s>>c.Intensity>>c.Range>>c.CastShadows>>c.ShadowBias;current.AddComponent<PointLightComponent>(c);}
            else if(k=="SPOT"){SpotLightComponent c;ReadV3(s,c.Color);s>>c.Intensity>>c.Range>>c.InnerAngle>>c.OuterAngle>>c.CastShadows>>c.ShadowBias;current.AddComponent<SpotLightComponent>(c);}
            else if(k=="AUDIO_SOURCE"){AudioSourceComponent c;s>>std::quoted(c.ClipPath)>>c.PlayOnAwake>>c.Loop>>c.Volume>>c.Pitch;current.AddComponent<AudioSourceComponent>(c);}
            else if(k=="AUDIO_SOURCE_V2"){AudioSourceComponent c;s>>std::quoted(c.ClipPath)>>c.PlayOnAwake>>c.Loop>>c.Volume>>c.Pitch>>c.SpatialBlend>>c.MinDistance>>c.MaxDistance>>c.DopplerFactor;current.AddComponent<AudioSourceComponent>(c);}
            else if(k=="AUDIO_LISTENER"){AudioListenerComponent c;s>>c.Enabled;current.AddComponent<AudioListenerComponent>(c);}
            else if(k=="PARTICLE_SYSTEM"){ParticleSystemComponent c;
                s>>c.Playing>>c.Loop>>c.Duration>>c.StartLifetime>>c.StartSpeed>>c.StartSize
                 >>c.StartColor.r>>c.StartColor.g>>c.StartColor.b>>c.StartColor.a
                 >>c.EmissionRate>>c.MaxParticles
                 >>c.Direction.x>>c.Direction.y>>c.Direction.z
                 >>c.Gravity.x>>c.Gravity.y>>c.Gravity.z;
                current.AddComponent<ParticleSystemComponent>(c);}
            else if(k=="PARTICLE_SYSTEM_V2"){ParticleSystemComponent c; int shape=0,blend=0;
                s>>c.Playing>>c.Loop>>c.Duration>>c.StartLifetime>>c.LifetimeRandom
                 >>c.StartSpeed>>c.SpeedRandom>>c.StartSize>>c.SizeRandom
                 >>c.StartColor.r>>c.StartColor.g>>c.StartColor.b>>c.StartColor.a
                 >>c.EndColor.r>>c.EndColor.g>>c.EndColor.b>>c.EndColor.a
                 >>c.EndSizeMultiplier>>c.EmissionRate>>c.MaxParticles
                 >>c.Direction.x>>c.Direction.y>>c.Direction.z
                 >>c.Gravity.x>>c.Gravity.y>>c.Gravity.z
                 >>shape>>c.ShapeRadius>>c.ConeAngle>>blend>>std::quoted(c.TexturePath);
                c.Shape=static_cast<ParticleShape>(shape);
                c.BlendMode=static_cast<ParticleBlendMode>(blend);
                current.AddComponent<ParticleSystemComponent>(c);}
            else if(k=="SCRIPT")
            {
                // V1.0-V1.3 compatibility: SCRIPT <enabled> <rotationSpeed>
                NativeScriptComponent c; float legacySpeed=1.0f; s>>c.Enabled>>legacySpeed;
                c.ScriptName="Rotator"; c.Fields["Speed"]=ScriptFieldValue::MakeFloat(legacySpeed);
                c.Fields["Axis"]=ScriptFieldValue::MakeVec3({0.0f,1.0f,0.0f});
                current.AddComponent<NativeScriptComponent>(c);
            }
            else if(k=="SCRIPT_V2")
            {
                NativeScriptComponent c; std::size_t fieldCount=0;
                s>>c.Enabled>>std::quoted(c.ScriptName)>>fieldCount;
                current.AddComponent<NativeScriptComponent>(c);
            }
            else if(k=="SCRIPT_FIELD" && current.HasComponent<NativeScriptComponent>())
            {
                std::string name; int type=0; s>>std::quoted(name)>>type;
                ScriptFieldValue v; v.Type=ScriptFieldType(type);
                if(v.Type==ScriptFieldType::Float) s>>v.Float;
                else if(v.Type==ScriptFieldType::Int) s>>v.Int;
                else if(v.Type==ScriptFieldType::Bool) s>>v.Bool;
                else ReadV3(s,v.Vec3);
                current.GetComponent<NativeScriptComponent>().Fields[name]=v;
            }
            else if(k=="RIGIDBODY"){RigidbodyComponent c;int ty;s>>ty>>c.Mass>>c.UseGravity;c.Type=RigidbodyType(ty);current.AddComponent<RigidbodyComponent>(c);}
            else if(k=="BOX"){BoxColliderComponent c;ReadV3(s,c.Size);s>>c.IsTrigger>>c.Material.Friction>>c.Material.Bounciness;current.AddComponent<BoxColliderComponent>(c);}
            else if(k=="SPHERE"){SphereColliderComponent c;s>>c.Radius>>c.IsTrigger>>c.Material.Friction>>c.Material.Bounciness;current.AddComponent<SphereColliderComponent>(c);}
            else if(k=="CAPSULE"){CapsuleColliderComponent c;s>>c.Radius>>c.Height>>c.IsTrigger>>c.Material.Friction>>c.Material.Bounciness;current.AddComponent<CapsuleColliderComponent>(c);}
            else if(k=="MESH"){std::string mp;s>>std::quoted(mp);if(mp=="CUBE"||mp.empty())current.AddComponent<MeshComponent>(defaultMesh);else{try{current.AddComponent<MeshComponent>(AssetManager::LoadMesh(mp));}catch(...){current.AddComponent<MeshComponent>(defaultMesh);}}}
            else if(k=="MATERIAL"){
                auto m=std::make_shared<Material>(*defaultMaterial);
                glm::vec4 col; s>>col.r>>col.g>>col.b>>col.a>>m->Metallic()>>m->Roughness()>>m->AmbientOcclusion()>>m->NormalStrength();
                ReadV3(s,m->EmissiveColor());s>>m->EmissiveStrength();m->GetColor()=col;
                // The editor default material may contain the checker/default texture.
                // A serialized material must not inherit it just because it was cloned.
                // TEXTURES below explicitly restores a real albedo texture when one was saved.
                m->UseTexture() = false;
                current.AddComponent<MeshRendererComponent>(m);
            }
            else if(k=="TEXTURES" && current.HasComponent<MeshRendererComponent>()){
                std::string albedo,normal,metallic,roughness,ao,emissive;
                s>>std::quoted(albedo)>>std::quoted(normal)>>std::quoted(metallic)
                 >>std::quoted(roughness)>>std::quoted(ao)>>std::quoted(emissive);
                auto m=current.GetComponent<MeshRendererComponent>().MaterialAsset;
                auto load=[](const std::string& path)->std::shared_ptr<Texture2D>{
                    return path.empty()?nullptr:AssetManager::LoadTexture(path);
                };
                if(!albedo.empty()){m->SetTexture(load(albedo));m->UseTexture()=true;}
                m->SetNormalTexture(load(normal));
                m->SetMetallicTexture(load(metallic));
                m->SetRoughnessTexture(load(roughness));
                m->SetAOTexture(load(ao));
                m->SetEmissiveTexture(load(emissive));
            }
        }
        // Resolve saved parent IDs after all entities have been created.
        for(auto& pr:parents){auto it=handleMap.find(pr.oldParent);if(it!=handleMap.end())scene.SetParent(pr.child,it->second,false);}
        return true;
    }
}
