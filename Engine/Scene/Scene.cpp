#include "Engine/Scene/Scene.h"

#include <algorithm>
#include <glm/gtc/matrix_inverse.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/quaternion.hpp>

namespace NoJob
{
    namespace
    {
        TransformComponent TransformFromMatrix(const glm::mat4& matrix)
        {
            glm::vec3 scale;
            glm::quat orientation;
            glm::vec3 translation;
            glm::vec3 skew;
            glm::vec4 perspective;

            TransformComponent result;

            if (glm::decompose(
                    matrix,
                    scale,
                    orientation,
                    translation,
                    skew,
                    perspective))
            {
                result.Position = translation;
                result.Rotation = glm::eulerAngles(orientation);
                result.Scale = scale;
            }

            return result;
        }
    }

    Entity Scene::CreateEntity(const std::string& name)
    {
        const std::uint32_t handle = m_NextHandle++;
        auto [iterator, inserted] = m_Entities.try_emplace(handle);
        (void)inserted;

        EntityData& data = iterator->second;
        data.ID.ID = m_NextID++;
        data.Tag.Tag = name.empty() ? "Entity" : name;

        return Entity(handle, this);
    }

    void Scene::DestroyEntity(Entity entity)
    {
        if (!entity || entity.m_Scene != this)
            return;

        const auto relationship =
            entity.GetComponent<RelationshipComponent>();

        // Children become roots while preserving their world transforms.
        const auto children = relationship.Children;
        for (const auto childHandle : children)
        {
            if (IsValid(childHandle))
                Unparent(Entity(childHandle, this), true);
        }

        if (relationship.Parent != 0 && IsValid(relationship.Parent))
        {
            auto& siblings =
                m_Entities.at(relationship.Parent).Relationship.Children;
            std::erase(siblings, entity.m_Handle);
        }

        m_Entities.erase(entity.m_Handle);
    }

    bool Scene::IsValid(std::uint32_t handle) const
    {
        return handle != 0 && m_Entities.contains(handle);
    }

    std::vector<Entity> Scene::GetEntities()
    {
        std::vector<Entity> result;
        result.reserve(m_Entities.size());

        for (const auto& [handle, data] : m_Entities)
        {
            (void)data;
            result.emplace_back(handle, this);
        }
        return result;
    }

    void Scene::OnUpdate(float deltaTime)
    {
        m_DeltaTime = deltaTime;
        // Runtime components/scripts/physics will be updated here later.
    }

    Entity Scene::GetParent(Entity entity)
    {
        if (!entity || entity.m_Scene != this)
            return {};

        const auto parent =
            entity.GetComponent<RelationshipComponent>().Parent;

        return IsValid(parent) ? Entity(parent, this) : Entity{};
    }

    std::vector<Entity> Scene::GetChildren(Entity entity)
    {
        std::vector<Entity> result;
        if (!entity || entity.m_Scene != this)
            return result;

        for (const auto handle :
             entity.GetComponent<RelationshipComponent>().Children)
        {
            if (IsValid(handle))
                result.emplace_back(handle, this);
        }
        return result;
    }

    bool Scene::IsDescendant(
        Entity possibleDescendant,
        Entity ancestor) const
    {
        if (!possibleDescendant || !ancestor)
            return false;

        std::uint32_t current = possibleDescendant.m_Handle;
        while (IsValid(current))
        {
            const auto parent =
                m_Entities.at(current).Relationship.Parent;

            if (parent == ancestor.m_Handle)
                return true;

            if (parent == 0)
                break;

            current = parent;
        }
        return false;
    }

    bool Scene::SetParent(
        Entity child,
        Entity parent,
        bool keepWorldTransform)
    {
        if (!child || !parent
            || child.m_Scene != this
            || parent.m_Scene != this
            || child == parent
            || IsDescendant(parent, child))
            return false;

        const glm::mat4 oldWorld =
            keepWorldTransform
                ? GetWorldTransform(child)
                : glm::mat4(1.0f);

        auto& childRelationship =
            child.GetComponent<RelationshipComponent>();

        if (childRelationship.Parent != 0
            && IsValid(childRelationship.Parent))
        {
            auto& oldChildren =
                m_Entities.at(
                    childRelationship.Parent).Relationship.Children;
            std::erase(oldChildren, child.m_Handle);
        }

        childRelationship.Parent = parent.m_Handle;

        auto& children =
            parent.GetComponent<RelationshipComponent>().Children;

        if (std::find(
                children.begin(),
                children.end(),
                child.m_Handle) == children.end())
        {
            children.push_back(child.m_Handle);
        }

        if (keepWorldTransform)
            SetWorldTransform(child, oldWorld);

        return true;
    }

    void Scene::Unparent(
        Entity child,
        bool keepWorldTransform)
    {
        if (!child || child.m_Scene != this)
            return;

        auto& relationship =
            child.GetComponent<RelationshipComponent>();

        if (relationship.Parent == 0)
            return;

        const glm::mat4 oldWorld =
            keepWorldTransform
                ? GetWorldTransform(child)
                : glm::mat4(1.0f);

        if (IsValid(relationship.Parent))
        {
            auto& siblings =
                m_Entities.at(
                    relationship.Parent).Relationship.Children;
            std::erase(siblings, child.m_Handle);
        }

        relationship.Parent = 0;

        if (keepWorldTransform)
            child.GetComponent<TransformComponent>() =
                TransformFromMatrix(oldWorld);
    }

    glm::mat4 Scene::GetWorldTransform(Entity entity) const
    {
        if (!entity || entity.m_Scene != this)
            return glm::mat4(1.0f);

        const auto& data = m_Entities.at(entity.m_Handle);
        const glm::mat4 local = data.Transform.GetTransform();

        const auto parent = data.Relationship.Parent;
        if (parent == 0 || !IsValid(parent))
            return local;

        return GetWorldTransform(Entity(parent, const_cast<Scene*>(this)))
            * local;
    }

    void Scene::SetWorldTransform(
        Entity entity,
        const glm::mat4& worldTransform)
    {
        if (!entity || entity.m_Scene != this)
            return;

        const auto parent =
            entity.GetComponent<RelationshipComponent>().Parent;

        glm::mat4 local = worldTransform;

        if (parent != 0 && IsValid(parent))
        {
            const glm::mat4 parentWorld =
                GetWorldTransform(Entity(parent, this));
            local = glm::inverse(parentWorld) * worldTransform;
        }

        entity.GetComponent<TransformComponent>() =
            TransformFromMatrix(local);
    }
}
