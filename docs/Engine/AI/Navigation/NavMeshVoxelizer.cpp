#include "Engine/AI/Navigation/NavMeshVoxelizer.h"
#include "Engine/Scene/Scene.h"
#include "Engine/Scene/Components.h"
#include <glm/gtc/matrix_inverse.hpp>
#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace NoJob {
namespace {
struct Box { glm::mat4 world, inverse; glm::vec3 half; float bottom, top; bool floor; };
struct Tri { glm::vec3 a,b,c; float slope; };
float cross2(float ax,float az,float bx,float bz) { return ax*bz-az*bx; }
bool heightAt(const Tri& t,float x,float z,float& y) {
    float d=cross2(t.b.x-t.a.x,t.b.z-t.a.z,t.c.x-t.a.x,t.c.z-t.a.z);
    if(std::abs(d)<1e-7f) return false;
    float u=cross2(x-t.a.x,z-t.a.z,t.c.x-t.a.x,t.c.z-t.a.z)/d;
    float v=cross2(t.b.x-t.a.x,t.b.z-t.a.z,x-t.a.x,z-t.a.z)/d;
    if(u < -1e-4f || v < -1e-4f || u+v > 1.0001f) return false;
    y=t.a.y+u*(t.b.y-t.a.y)+v*(t.c.y-t.a.y); return true;
}
}
NavMeshSourceGeometry NavMeshVoxelizer::Rasterize(Scene& scene,const NavMeshSourceGeometry& source,
    float cellSize,float agentRadius,float agentHeight,float maxSlopeAngle) {
    NavMeshSourceGeometry result;
    cellSize=std::max(cellSize,0.1f); agentRadius=std::max(agentRadius,0.0f);
    agentHeight=std::max(agentHeight,0.1f);
    std::vector<Tri> triangles;
    float minX=std::numeric_limits<float>::max(), minZ=minX;
    float maxX=-minX,maxZ=-minX;
    auto bounds=[&](const glm::vec3& p){minX=std::min(minX,p.x);maxX=std::max(maxX,p.x);minZ=std::min(minZ,p.z);maxZ=std::max(maxZ,p.z);};
    for(size_t i=0;i+2<source.Indices.size();i+=3) {
        const auto a=source.Indices[i],b=source.Indices[i+1],c=source.Indices[i+2];
        if(a>=source.Vertices.size()||b>=source.Vertices.size()||c>=source.Vertices.size())continue;
        Tri t{source.Vertices[a],source.Vertices[b],source.Vertices[c],0};
        glm::vec3 n=glm::cross(t.b-t.a,t.c-t.a);
        if(glm::length(n)<1e-6f)continue;
        n=glm::normalize(n);
        t.slope=glm::degrees(std::acos(glm::clamp(n.y,-1.0f,1.0f)));
        // Imported meshes may use reversed winding; accept upward-facing geometry only.
        if(t.slope>maxSlopeAngle)continue;
        triangles.push_back(t);bounds(t.a);bounds(t.b);bounds(t.c);
    }
    std::vector<Box> boxes;
    for(Entity e:scene.GetEntities()) {
        if(!e.HasComponent<BoxColliderComponent>())continue;
        const auto& collider=e.GetComponent<BoxColliderComponent>();
        if(collider.IsTrigger)continue;
        if(e.HasComponent<RigidbodyComponent>() &&
           e.GetComponent<RigidbodyComponent>().Type!=RigidbodyType::Static)continue;
        const glm::mat4 w=scene.GetWorldTransform(e);
        if(std::abs(glm::determinant(w))<1e-7f)continue;
        const glm::vec3 half=glm::max(collider.Size*0.5f,glm::vec3(0.001f));
        float bottom=std::numeric_limits<float>::max(),top=-bottom;
        glm::vec3 lo(std::numeric_limits<float>::max()),hi(-std::numeric_limits<float>::max());
        for(int ix=-1;ix<=1;ix+=2)for(int iy=-1;iy<=1;iy+=2)for(int iz=-1;iz<=1;iz+=2){
            glm::vec3 p=glm::vec3(w*glm::vec4(half*glm::vec3(ix,iy,iz),1));
            lo=glm::min(lo,p);hi=glm::max(hi,p);bottom=std::min(bottom,p.y);top=std::max(top,p.y);
        }
        // Thin, nearly horizontal boxes are walkable floors; tall boxes are obstacles.
        glm::vec3 up=glm::normalize(glm::vec3(w*glm::vec4(0,1,0,0)));
        bool floor=std::abs(up.y)>0.98f && (hi.y-lo.y)<=std::max(0.35f, std::min(hi.x-lo.x,hi.z-lo.z)*0.25f);
        boxes.push_back({w,glm::inverse(w),half,bottom,top,floor});
        if(floor){bounds(lo);bounds(hi);}
    }
    if(minX>maxX||minZ>maxZ)return result;
    const int nx=static_cast<int>(std::ceil((maxX-minX)/cellSize));
    const int nz=static_cast<int>(std::ceil((maxZ-minZ)/cellSize));
    if(nx<=0||nz<=0||static_cast<long long>(nx)*nz>250000)return result;
    std::vector<float> heights(static_cast<size_t>(nx)*nz,0);
    std::vector<unsigned char> walk(static_cast<size_t>(nx)*nz,0);
    auto index=[&](int x,int z){return static_cast<size_t>(z)*nx+x;};
    auto floorHeight=[&](float x,float z,float& y){
        bool found=false; y=-std::numeric_limits<float>::max();
        for(const auto& t:triangles){float h;if(heightAt(t,x,z,h) && (!found||h>y)){y=h;found=true;}}
        for(const auto& b:boxes)if(b.floor){
            glm::vec3 local=glm::vec3(b.inverse*glm::vec4(x,b.top,z,1));
            if(std::abs(local.x)<=b.half.x+1e-4f && std::abs(local.z)<=b.half.z+1e-4f && (!found||b.top>y)){
                y=b.top;found=true;
            }
        }
        return found;
    };
    auto blocked=[&](float x,float y,float z){
        const float r=agentRadius;
        const float offsets[3]={-r,0.0f,r};
        for(const auto& b:boxes)if(!b.floor && b.top>y+0.05f && b.bottom<y+agentHeight){
            for(float dx:offsets)for(float dz:offsets){
                glm::vec3 local=glm::vec3(b.inverse*glm::vec4(x+dx,(std::max(y,b.bottom)+std::min(y+agentHeight,b.top))*0.5f,z+dz,1));
                if(glm::all(glm::lessThanEqual(glm::abs(local),b.half+glm::vec3(1e-4f))))return true;
            }
        }
        return false;
    };
    for(int z=0;z<nz;++z)for(int x=0;x<nx;++x){
        float px=minX+(x+0.5f)*cellSize,pz=minZ+(z+0.5f)*cellSize,h;
        if(!floorHeight(px,pz,h)||blocked(px,h,pz))continue;
        // Ensure cell corners have ground at roughly the same elevation.
        bool valid=true;
        for(float dx:{-0.49f,0.49f})for(float dz:{-0.49f,0.49f}){
            float cornerH;
            if(!floorHeight(px+dx*cellSize,pz+dz*cellSize,cornerH)||std::abs(cornerH-h)>cellSize*0.6f)
                valid=false;
        }
        if(valid){walk[index(x,z)]=1;heights[index(x,z)]=h;}
    }
    // Shared vertex IDs allow BuildAdjacency to connect adjacent cells.
    std::vector<int> vertices(static_cast<size_t>(nx+1)*(nz+1),-1);
    auto vertex=[&](int x,int z,float y)->uint32_t{
        size_t k=static_cast<size_t>(z)*(nx+1)+x;
        if(vertices[k]<0){vertices[k]=static_cast<int>(result.Vertices.size());
            result.Vertices.emplace_back(minX+x*cellSize,y,minZ+z*cellSize);}
        return static_cast<uint32_t>(vertices[k]);
    };
    for(int z=0;z<nz;++z)for(int x=0;x<nx;++x)if(walk[index(x,z)]){
        float h=heights[index(x,z)];
        // Adjacent cells of differing heights need separate vertices; do not bridge steep steps.
        bool compatible=true;
        for(int dz=-1;dz<=1;++dz)for(int dx=-1;dx<=1;++dx){
            int xx=x+dx,zz=z+dz;
            if(xx>=0&&zz>=0&&xx<nx&&zz<nz&&walk[index(xx,zz)] &&
                std::abs(heights[index(xx,zz)]-h)>0.05f)compatible=false;
        }
        if(!compatible)continue;
        uint32_t a=vertex(x,z,h),b=vertex(x+1,z,h),c=vertex(x+1,z+1,h),d=vertex(x,z+1,h);
        result.Indices.insert(result.Indices.end(),{a,d,b,b,d,c});
    }
    return result;
}
}
