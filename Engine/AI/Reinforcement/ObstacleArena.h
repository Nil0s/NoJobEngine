/** @file ObstacleArena.h @brief Canonical scene geometry and collision sensing for the learning arena. */
#pragma once
#include "Engine/AI/Reinforcement/ObstacleLearning.h"
#include "Engine/Scene/Scene.h"
namespace NoJob::RL {
/** @brief The two static axis-aligned wall boxes used in training and comparisons. */
struct ArenaWall { glm::vec3 centre,size; };
/** @brief Shared geometry; the plane itself remains testplane_100x100.obj at identity. */
inline std::array<ArenaWall,2> ArenaWalls(){return {{{{0,1.5f,0},{1.2f,3,7.6f}},{{4,1.5f,-4.8f},{2.8f,3,1.2f}}}};}
/** @brief Convert a cell to a world-space cube centre. */
inline glm::vec3 CellWorld(GridCell p,float height=.75f){return {(p.x-4)*ObstacleEnvironment::Spacing,height,(p.z-4)*ObstacleEnvironment::Spacing};}
/** @brief Reject unsupported edits rather than silently evaluating a different map. */
inline bool MatchesObstacleArena(Scene& scene) {
    const auto expected=ArenaWalls();std::array<bool,2> found{};int count=0;
    for(auto e:scene.GetEntities()) {
        if(!e.HasComponent<BoxColliderComponent>())continue;
        const auto& box=e.GetComponent<BoxColliderComponent>();if(box.IsTrigger)continue;
        // Any extra solid collider or moved/rotated wall changes the trained task.
        const auto w=scene.GetWorldTransform(e);const glm::vec3 p(w[3]);
        bool match=false;
        for(size_t i=0;i<expected.size();++i){
            const auto&s=expected[i].size;
            if(glm::length(p-expected[i].centre)<.001f && glm::length(glm::vec3(w[0])-glm::vec3(s.x,0,0))<.001f &&
               glm::length(glm::vec3(w[1])-glm::vec3(0,s.y,0))<.001f && glm::length(glm::vec3(w[2])-glm::vec3(0,0,s.z))<.001f &&
               glm::length(box.Size-glm::vec3(1))<.001f && !found[i]){found[i]=true;match=true;break;}
        }
        if(!match)return false;++count;
    }
    return count==2;
}
/** @brief Sense candidate-cell overlap from live scene colliders, not from navigation. */
inline bool SceneCellBlocked(Scene& scene,GridCell p) {
    if(p.x<0||p.z<0||p.x>=9||p.z>=9)return true;
    const auto centre=CellWorld(p);
    for(auto e:scene.GetEntities())if(e.HasComponent<BoxColliderComponent>()) {
        const auto& b=e.GetComponent<BoxColliderComponent>();if(b.IsTrigger)continue;
        const auto w=scene.GetWorldTransform(e);const glm::vec3 c(w[3]);
        const auto half=b.Size*glm::vec3(glm::length(glm::vec3(w[0])),glm::length(glm::vec3(w[1])),glm::length(glm::vec3(w[2])))*.5f;
        if(std::abs(centre.x-c.x)<half.x+.5f && std::abs(centre.z-c.z)<half.z+.5f &&
           std::abs(centre.y-c.y)<half.y+.5f)return true;
    }return false;
}
/** @brief Conservatively test a segment swept by a nonrotating unit cube. */
inline bool ArenaSegmentClear(const glm::vec3& start,const glm::vec3& end) {
    for(const auto&wall:ArenaWalls()){
        const auto lo=wall.centre-wall.size*.5f-glm::vec3(.5f);
        const auto hi=wall.centre+wall.size*.5f+glm::vec3(.5f);
        float t0=0,t1=1;bool intersects=true;
        for(int axis:{0,2}){
            const float delta=end[axis]-start[axis];
            if(std::abs(delta)<1e-6f){if(start[axis]<lo[axis]||start[axis]>hi[axis]){intersects=false;break;}}
            else{float a=(lo[axis]-start[axis])/delta,b=(hi[axis]-start[axis])/delta;if(a>b)std::swap(a,b);t0=std::max(t0,a);t1=std::min(t1,b);if(t0>t1){intersects=false;break;}}
        }if(intersects)return false;
    }return true;
}
}
