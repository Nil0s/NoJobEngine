#include "Engine/Animation/Animation.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <stdexcept>
#include <functional>
#include <algorithm>
#include <cmath>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

namespace NoJob
{
    static glm::mat4 ToGlm(const aiMatrix4x4& a)
    {
        glm::mat4 m;
        m[0][0]=a.a1;m[1][0]=a.a2;m[2][0]=a.a3;m[3][0]=a.a4;
        m[0][1]=a.b1;m[1][1]=a.b2;m[2][1]=a.b3;m[3][1]=a.b4;
        m[0][2]=a.c1;m[1][2]=a.c2;m[2][2]=a.c3;m[3][2]=a.c4;
        m[0][3]=a.d1;m[1][3]=a.d2;m[2][3]=a.d3;m[3][3]=a.d4;
        return m;
    }

    std::shared_ptr<AnimationAsset> AnimationAsset::Load(const std::filesystem::path& path)
    {
        Assimp::Importer importer;
        const aiScene* scene=importer.ReadFile(path.string(),
            aiProcess_Triangulate|aiProcess_JoinIdenticalVertices|
            aiProcess_GenSmoothNormals|aiProcess_LimitBoneWeights);
        if(!scene) throw std::runtime_error("Animation import failed: "+std::string(importer.GetErrorString()));

        auto out=std::make_shared<AnimationAsset>();
        out->m_SourcePath=path;
        if (scene->mRootNode)
            out->m_GlobalInverseTransform = glm::inverse(ToGlm(scene->mRootNode->mTransformation));

        std::unordered_map<std::string,int> boneIndex;
        for(unsigned mi=0;mi<scene->mNumMeshes;++mi)
        {
            const aiMesh* mesh=scene->mMeshes[mi];
            for(unsigned bi=0;bi<mesh->mNumBones;++bi)
            {
                const aiBone* b=mesh->mBones[bi];
                const std::string name=b->mName.C_Str();
                if(!boneIndex.contains(name))
                {
                    int idx=(int)out->m_Bones.size();
                    boneIndex[name]=idx;
                    SkeletonBone sb; sb.Name=name; sb.Offset=ToGlm(b->mOffsetMatrix);
                    out->m_Bones.push_back(std::move(sb));
                }
            }
        }

        std::function<void(aiNode*,int)> walk=[&](aiNode* node,int parentBone)
        {
            int current=parentBone;
            auto it=boneIndex.find(node->mName.C_Str());
            if(it!=boneIndex.end())
            {
                current=it->second;
                out->m_Bones[current].Parent=parentBone;
                out->m_Bones[current].LocalBind=ToGlm(node->mTransformation);
            }
            for(unsigned i=0;i<node->mNumChildren;++i) walk(node->mChildren[i],current);
        };
        if(scene->mRootNode) walk(scene->mRootNode,-1);

        for(unsigned ai=0;ai<scene->mNumAnimations;++ai)
        {
            const aiAnimation* a=scene->mAnimations[ai];
            AnimationClip clip;
            clip.Name=a->mName.length?a->mName.C_Str():("Animation "+std::to_string(ai));
            clip.DurationTicks=a->mDuration;
            clip.TicksPerSecond=a->mTicksPerSecond>0.0?a->mTicksPerSecond:25.0;
            clip.Channels.reserve(a->mNumChannels);
            for(unsigned ci=0;ci<a->mNumChannels;++ci)
            {
                const aiNodeAnim* c=a->mChannels[ci];
                AnimationChannel ch; ch.NodeName=c->mNodeName.C_Str();
                for(unsigned k=0;k<c->mNumPositionKeys;++k)
                    ch.Positions.push_back({c->mPositionKeys[k].mTime,
                        {c->mPositionKeys[k].mValue.x,c->mPositionKeys[k].mValue.y,c->mPositionKeys[k].mValue.z}});
                for(unsigned k=0;k<c->mNumRotationKeys;++k)
                    ch.Rotations.push_back({c->mRotationKeys[k].mTime,
                        {c->mRotationKeys[k].mValue.w,c->mRotationKeys[k].mValue.x,c->mRotationKeys[k].mValue.y,c->mRotationKeys[k].mValue.z}});
                for(unsigned k=0;k<c->mNumScalingKeys;++k)
                    ch.Scales.push_back({c->mScalingKeys[k].mTime,
                        {c->mScalingKeys[k].mValue.x,c->mScalingKeys[k].mValue.y,c->mScalingKeys[k].mValue.z}});
                clip.Channels.push_back(std::move(ch));
            }
            out->m_Clips.push_back(std::move(clip));
        }
        return out;
    }
    namespace
    {
        glm::vec3 SampleVec(const std::vector<AnimationVecKey>& keys, double tick, const glm::vec3& fallback)
        {
            if (keys.empty()) return fallback;
            if (keys.size()==1 || tick<=keys.front().Time) return keys.front().Value;
            if (tick>=keys.back().Time) return keys.back().Value;
            for (std::size_t i=0;i+1<keys.size();++i)
            {
                if (tick < keys[i+1].Time)
                {
                    const double span=keys[i+1].Time-keys[i].Time;
                    const float a=span>0.0?static_cast<float>((tick-keys[i].Time)/span):0.0f;
                    return glm::mix(keys[i].Value,keys[i+1].Value,glm::clamp(a,0.0f,1.0f));
                }
            }
            return keys.back().Value;
        }

        glm::quat SampleQuat(const std::vector<AnimationQuatKey>& keys, double tick, const glm::quat& fallback)
        {
            if (keys.empty()) return fallback;
            if (keys.size()==1 || tick<=keys.front().Time) return glm::normalize(keys.front().Value);
            if (tick>=keys.back().Time) return glm::normalize(keys.back().Value);
            for (std::size_t i=0;i+1<keys.size();++i)
            {
                if (tick < keys[i+1].Time)
                {
                    const double span=keys[i+1].Time-keys[i].Time;
                    const float a=span>0.0?static_cast<float>((tick-keys[i].Time)/span):0.0f;
                    return glm::normalize(glm::slerp(keys[i].Value,keys[i+1].Value,glm::clamp(a,0.0f,1.0f)));
                }
            }
            return glm::normalize(keys.back().Value);
        }
    }

    std::vector<glm::mat4> AnimationAsset::EvaluatePose(int clipIndex, float timeSeconds) const
    {
        std::vector<glm::mat4> palette(m_Bones.size(), glm::mat4(1.0f));
        if (m_Bones.empty() || m_Clips.empty()) return palette;

        clipIndex = std::clamp(clipIndex, 0, static_cast<int>(m_Clips.size())-1);
        const AnimationClip& clip=m_Clips[clipIndex];
        const double duration=std::max(clip.DurationTicks,0.0);
        double tick=static_cast<double>(timeSeconds)*clip.TicksPerSecond;
        if (duration>0.0) tick=std::fmod(std::max(tick,0.0),duration);

        std::unordered_map<std::string,const AnimationChannel*> channels;
        channels.reserve(clip.Channels.size());
        for (const auto& channel:clip.Channels) channels[channel.NodeName]=&channel;

        std::vector<glm::mat4> globals(m_Bones.size(),glm::mat4(1.0f));
        std::vector<unsigned char> evaluated(m_Bones.size(),0);
        std::function<void(std::size_t)> evaluateBone = [&](std::size_t i)
        {
            if (evaluated[i]) return;
            const auto& bone=m_Bones[i];
            glm::mat4 local=bone.LocalBind;
            if (auto found=channels.find(bone.Name);found!=channels.end())
            {
                const auto* ch=found->second;
                glm::vec3 bindScale(1.0f), bindTranslation(local[3]);
                glm::quat bindRotation=glm::quat_cast(glm::mat3(local));
                const glm::vec3 pos=SampleVec(ch->Positions,tick,bindTranslation);
                const glm::quat rot=SampleQuat(ch->Rotations,tick,bindRotation);
                const glm::vec3 scale=SampleVec(ch->Scales,tick,bindScale);
                local=glm::translate(glm::mat4(1.0f),pos)*glm::mat4_cast(rot)*glm::scale(glm::mat4(1.0f),scale);
            }
            if (bone.Parent>=0)
            {
                const auto parent=static_cast<std::size_t>(bone.Parent);
                evaluateBone(parent);
                globals[i]=globals[parent]*local;
            }
            else globals[i]=local;
            palette[i]=m_GlobalInverseTransform*globals[i]*bone.Offset;
            evaluated[i]=1;
        };
        for (std::size_t i=0;i<m_Bones.size();++i) evaluateBone(i);
        return palette;
    }

}
