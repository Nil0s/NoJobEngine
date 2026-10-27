/** @file NavTrackingTests.cpp @brief Headless bake, static-obstacle and live-retarget regressions. */
#include "Engine/AI/Navigation/NavTargetTracking.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <iomanip>
/** @brief Fail a regression in every build configuration. */
void Require(bool value,const char* message) { if(!value) throw std::runtime_error(message); }
/** @brief Load the actual demo OBJ without rendering or changing its geometry. */
NoJob::NavMeshSourceGeometry LoadPlane() {
    NoJob::NavMeshSourceGeometry source;
    std::ifstream in("Assets/Models/testplane_100x100.obj");
    Require(bool(in),"Missing testplane_100x100.obj");
    std::string line;
    while(std::getline(in,line)) {
        std::istringstream row(line); std::string kind; row>>kind;
        if(kind=="v") { glm::vec3 v; row>>v.x>>v.y>>v.z; source.Vertices.push_back(v); }
        else if(kind=="f") {
            for(int i=0;i<3;++i) { std::string token; row>>token; source.Indices.push_back(static_cast<uint32_t>(std::stoul(token)-1)); }
        }
    }
    Require(source.Vertices.size()==4 && source.Indices.size()==6,"Unexpected plane geometry");
    return source;
}
/** @brief Import the demo's wall transforms/colliders without any OpenGL resource creation. */
std::vector<NoJob::Entity> LoadWalls(NoJob::Scene& scene) {
    std::ifstream in("Assets/Scenes/RLNavigationDemo.nojobscene");
    Require(bool(in),"Missing RLNavigationDemo scene");
    std::vector<NoJob::Entity> walls; NoJob::Entity current;
    std::string line;
    while(std::getline(in,line)) {
        std::istringstream row(line); std::string kind; row>>kind;
        if(kind=="ENTITY") {
            unsigned id; std::string name; row>>id>>std::quoted(name);
            current=name.find("NavWall_")==0?scene.CreateEntity(name):NoJob::Entity{};
            if(current) walls.push_back(current);
        } else if(current && kind=="TRANSFORM") {
            auto& t=current.GetComponent<NoJob::TransformComponent>();
            row>>t.Position.x>>t.Position.y>>t.Position.z>>t.Rotation.x>>t.Rotation.y>>t.Rotation.z>>t.Scale.x>>t.Scale.y>>t.Scale.z;
        } else if(current && kind=="BOX") {
            auto& box=current.AddComponent<NoJob::BoxColliderComponent>();
            row>>box.Size.x>>box.Size.y>>box.Size.z>>box.IsTrigger>>box.Material.Friction>>box.Material.Bounciness;
        }
    }
    Require(walls.size()==2,"Expected both demo walls"); return walls;
}
/** @brief Use separating axes to check the rotated cube footprint against demo walls. */
void CheckClear(const glm::mat4& world,const std::vector<NoJob::Entity>& walls) {
    const glm::vec2 centre(world[3].x,world[3].z);
    const glm::vec2 basisX(world[0].x,world[0].z),basisZ(world[2].x,world[2].z);
    const glm::vec2 axes[]={{1,0},{0,1},glm::normalize(basisX),glm::normalize(basisZ)};
    for(auto wall:walls) {
        const auto& t=wall.GetComponent<NoJob::TransformComponent>();
        const auto half=t.Scale*wall.GetComponent<NoJob::BoxColliderComponent>().Size*.5f;
        const glm::vec2 delta=centre-glm::vec2(t.Position.x,t.Position.z);
        bool separated=false;
        for(const auto& axis:axes) {
            const float cubeRadius=.5f*(std::abs(glm::dot(basisX,axis))+std::abs(glm::dot(basisZ,axis)));
            const float wallRadius=half.x*std::abs(axis.x)+half.z*std::abs(axis.y);
            if(std::abs(glm::dot(delta,axis))>=cubeRadius+wallRadius-.001f) separated=true;
        }
        Require(separated,"Rotated cube clipped an obstacle");
    }
}
/** @brief Exercise the same scene NavAgent movement used by the script's navigation mode. */
int main() {
    try {
        using namespace NoJob;
        Scene scene; NavigationSystem nav; scene.SetNavigationSystem(&nav);
        const auto walls=LoadWalls(scene); const auto plane=LoadPlane();
        Entity agent=scene.CreateEntity("TestAgent");
        agent.GetComponent<TransformComponent>().Position={-6.4f,.75f,0};
        auto& component=agent.AddComponent<NavAgentComponent>();
        component.StoppingDistance=.25f; component.RepathInterval=.5f;
        NavTargetTracker tracker;
        glm::vec3 goal(6.4f,.75f,0);
        Require(tracker.Update(scene,agent,goal,.01f)==NavTargetTracker::State::MissingBake,"Missing bake must stop");
        Require(!component.HasDestination,"Missing bake left a destination active");
        Require(nav.Bake(scene,plane),"Plane bake failed");
        Entity noAgent=scene.CreateEntity("NoAgent");
        Require(tracker.Update(scene,noAgent,goal,.01f)==NavTargetTracker::State::MissingAgent,"Missing component was accepted");
        Require(tracker.Update(scene,agent,{std::numeric_limits<float>::quiet_NaN(),0,0},.01f)==NavTargetTracker::State::InvalidPosition,"Invalid position accepted");
        std::cout<<"Baked polygons: "<<nav.GetNavMesh().GetPolygonCount()<<std::endl;
        Require(nav.FindPolygon({0,0,0})==InvalidNavPolygonID,"Wall was not carved out of bake");
        const auto path=nav.CalculatePath(agent.GetComponent<TransformComponent>().Position,goal);
        Require(path.Points.size()>2,"Wall route must detour");
        scene.OnRuntimeStart();
        auto reach=[&](const glm::vec3& destination) {
            for(int tick=0;tick<2400;++tick) {
                const auto state=tracker.Update(scene,agent,destination,1.f/60);
                scene.OnUpdate(1.f/60);
                const auto p=glm::vec3(scene.GetWorldTransform(agent)[3]);
                CheckClear(scene.GetWorldTransform(agent),walls);
                Require(std::abs(p.y-.75f)<.001f,"Navigation changed visual height");
                if(state==NavTargetTracker::State::Arrived) return;
            }
            throw std::runtime_error("Navigation did not arrive within 40 seconds");
        };
        reach(goal);
        std::cout<<"PASS: wall detour, cube clearance, arrival"<<std::endl;
        reach({-6.4f,.75f,6.f});
        std::cout<<"PASS: retarget after arrival"<<std::endl;
        for(int tick=0;tick<60;++tick) { tracker.Update(scene,agent,goal,1.f/60); scene.OnUpdate(1.f/60); }
        reach({-6.4f,.75f,-6.f});
        std::cout<<"PASS: retarget while moving"<<std::endl;
        Require(tracker.Update(scene,agent,{0,.75f,0},.01f)==NavTargetTracker::State::NoRoute && !component.HasDestination,"Blocked goal must cancel previous destination");
        Require(tracker.Update(scene,agent,{60,.75f,0},.01f)==NavTargetTracker::State::NoRoute,"Off-mesh goal accepted");
        nav.Clear();
        Require(tracker.Update(scene,agent,goal,.01f)==NavTargetTracker::State::MissingBake && !component.HasDestination,"Cleared bake did not stop agent");
        Require(nav.Bake(scene,plane),"Rebake failed");
        reach(goal);
        scene.OnRuntimeStop();
        std::cout<<"PASS: blocked/off-mesh goals, clear and rebake recovery"<<std::endl;
        return 0;
    } catch(const std::exception& e) { std::cerr<<e.what()<<std::endl; return 1; }
}
