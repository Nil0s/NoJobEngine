#pragma once
#include <filesystem>
#include <string>
namespace NoJob
{
    struct ProjectConfig
    {
        std::string Name = "NoJobProject";
        std::filesystem::path AssetDirectory = "Assets";
        std::filesystem::path StartScene = "Assets/Scenes/CurrentScene.nojobscene";
    };
    class Project
    {
    public:
        static bool Save(const ProjectConfig&,const std::filesystem::path&);
        static bool Load(ProjectConfig&,const std::filesystem::path&);
    };
}
