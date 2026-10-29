/** @file BTDemoNavigation.cpp @brief Navigation lifetime for the packaged BT example. */
#include "BTDemoNavigation.h"
#include "Engine/Scene/Scene.h"
#include "Engine/Scene/ScriptRegistry.h"
#include "Engine/Core/ProjectPaths.h"
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <stdexcept>
#include <cstdio>
namespace NoJob {
void BTDemoNavigation::OnCreate() {
    auto* scene=GetEntity().GetScene();
    if(!scene) throw std::runtime_error("Behaviour Tree demo has no scene.");
    auto* navigation=scene->GetNavigationSystem();
    if(!navigation) {
        navigation_=std::make_unique<NavigationSystem>();
        navigation=navigation_.get();
        scene->SetNavigationSystem(navigation);
    }
    // A previous editor scene may have left a different bake: rebuild this demo's geometry.
    // Project DLLs have their own asset registry. Import the actual floor file directly
    // instead of looking up host-owned mesh pointers in the DLL's empty registry.
    Assimp::Importer importer;
    const auto path=Paths::FindProjectRoot()/"Assets/Models/testplane_100x100.obj";
    const auto* imported=importer.ReadFile(path.string(),aiProcess_Triangulate);
    NavMeshSourceGeometry source;
    if(imported) for(unsigned int m=0;m<imported->mNumMeshes;++m) {
        const auto* mesh=imported->mMeshes[m];
        const auto base=static_cast<std::uint32_t>(source.Vertices.size());
        const auto world=scene->GetWorldTransform(GetEntity());
        for(unsigned int v=0;v<mesh->mNumVertices;++v) {
            const auto& p=mesh->mVertices[v];
            source.Vertices.emplace_back(world*glm::vec4(p.x,p.y,p.z,1.f));
        }
        for(unsigned int f=0;f<mesh->mNumFaces;++f) {
            const auto& face=mesh->mFaces[f];
            if(face.mNumIndices==3) for(unsigned int i=0;i<3;++i)
                source.Indices.push_back(base+face.mIndices[i]);
        }
    }
    if(!navigation->Bake(*scene,source)) {
        OnDestroy();
        throw std::runtime_error("Behaviour Tree demo bake failed. Check Assets/Models/testplane_100x100.obj.");
    }
    std::puts("[BT Demo] Scene navigation baked successfully.");
}
void BTDemoNavigation::OnDestroy() {
    auto* scene=GetEntity().GetScene();
    if(navigation_ && scene && scene->GetNavigationSystem()==navigation_.get())
        scene->SetNavigationSystem(nullptr);
    navigation_.reset();
}
NOJOB_REGISTER_SCRIPT(BTDemoNavigation,"AI / Examples")
}
