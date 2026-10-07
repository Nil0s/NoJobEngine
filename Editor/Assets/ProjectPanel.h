#pragma once

#include "Engine/Scene/Entity.h"
#include <filesystem>
#include <functional>
#include <memory>
#include <string>

namespace NoJob
{
    class Scene;
    class Mesh;
    class Material;

    class ProjectPanel
    {
    public:
        using LogFunction = std::function<void(std::string)>;
        using CreateModelFunction = std::function<void(const std::filesystem::path&)>;

        void Initialize();
        void Draw(Scene* scene,
                  Entity& selectedEntity,
                  const std::shared_ptr<Mesh>& defaultCubeMesh,
                  const std::shared_ptr<Material>& defaultCubeMaterial,
                  const CreateModelFunction& createModel,
                  const LogFunction& log);

        const std::filesystem::path& GetCurrentDirectoryPath() const { return m_CurrentDirectory; }

    private:
        std::filesystem::path m_CurrentDirectory;
        char m_Search[128]{};
        bool m_RequestCreateCppScript = false;
    };
}
