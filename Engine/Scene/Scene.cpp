#include "Engine/Scene/Scene.h"

namespace NoJob
{
    Entity Scene::CreateEntity(const std::string& name)
    {
        const std::uint32_t handle = m_NextHandle++;

        auto [iterator, inserted] =
            m_Entities.try_emplace(handle);

        (void)inserted;

        EntityData& data = iterator->second;
        data.ID.ID = m_NextID++;
        data.Tag.Tag = name.empty() ? "Entity" : name;

        return Entity(handle, this);
    }

    void Scene::DestroyEntity(Entity entity)
    {
        if (entity.m_Scene != this)
            return;

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
}
