/** @file ObstacleNavBaseline.h @brief Native baked-NavMesh comparator for the learned arena. */
#pragma once
#include "ObstacleArena.h"
#include "Engine/AI/Navigation/NavigationSystem.h"
#include "ObstacleEvaluation.h"
#include <fstream>
#include <filesystem>
#include <sstream>
namespace NoJob::RL {
/** @brief Own a separate baked navigation world; never read/mutate the user's active scene. */
class ObstacleNavBaseline {
public:
    /** @brief Bake the actual testplane OBJ with the canonical learning-scene colliders. */
    explicit ObstacleNavBaseline(const std::filesystem::path& root) {
        Scene scene;
        for(const auto& wall:ArenaWalls()) {
            auto e=scene.CreateEntity("Obstacle baseline wall");auto&t=e.GetComponent<TransformComponent>();
            t.Position=wall.centre;t.Scale=wall.size;e.AddComponent<BoxColliderComponent>();
        }
        NavMeshSourceGeometry source;
        std::ifstream in(root/"Assets/Models/testplane_100x100.obj");
        if(!in)throw std::runtime_error("Missing testplane_100x100.obj for native comparison");
        std::string line;
        while(std::getline(in,line)){
            std::istringstream row(line);std::string kind;row>>kind;
            if(kind=="v"){glm::vec3 p;row>>p.x>>p.y>>p.z;source.Vertices.push_back(p);}
            else if(kind=="f")for(int i=0;i<3;++i){std::string token;row>>token;source.Indices.push_back(uint32_t(std::stoul(token)-1));}
        }
        if(source.Vertices.size()!=4||source.Indices.size()!=6||!navigation_.Bake(scene,source))
            throw std::runtime_error("Unexpected or invalid testplane bake fixture");
    }
    /** @brief Apply the real native NavAgent with the same 1.6-unit movement budget per decision. */
    ObstacleOutcome Run(GridCell start,GridCell goal)const{
        ObstacleOutcome result;NavAgent agent;agent.Speed()=ObstacleEnvironment::Spacing;agent.StoppingDistance()=.05f;
        glm::vec3 p=CellWorld(start),target=CellWorld(goal);
        if(!agent.SetDestination(navigation_,p,target)){result.distance=glm::length(target-p);return result;}
        for(int step=0;step<ObstacleEnvironment::Budget;++step){
            const auto movement=agent.Update(p,1.f);const auto next=p+movement;
            result.steps=step+1;
            if(!ArenaSegmentClear(p,next)){++result.collisions;break;}
            result.pathLength+=glm::length(movement);p=next;
            if(std::hypot(p.x-target.x,p.z-target.z)<=.05f){result.success=true;break;}
        }
        result.distance=std::hypot(p.x-target.x,p.z-target.z);return result;
    }
private:NavigationSystem navigation_;
};
}
