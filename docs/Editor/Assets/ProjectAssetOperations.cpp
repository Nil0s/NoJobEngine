#include "Editor/Assets/ProjectAssetOperations.h"

#include "Engine/Asset/AssetManager.h"
#include "Engine/Assets/AssetRegistry.h"
#include "Engine/Assets/PrefabSerializer.h"
#include "Engine/Scene/Components.h"
#include "Engine/Scene/Scene.h"

#include <algorithm>
#include <cctype>
#include <fstream>

namespace NoJob
{
    std::string ProjectAssetOperations::SanitizeCppIdentifier(std::string name)
    {
        name.erase(std::remove_if(name.begin(), name.end(),
            [](unsigned char c){ return !(std::isalnum(c) || c == '_'); }),
            name.end());
        if (name.empty()) name = "NewScript";
        if (std::isdigit(static_cast<unsigned char>(name.front())))
            name.insert(name.begin(), '_');
        return name;
    }

    bool ProjectAssetOperations::CreateCppScript(
        const std::filesystem::path& scriptsDirectory,
        const std::string& requestedName,
        const LogFunction& log)
    {
        const std::string name = SanitizeCppIdentifier(requestedName);
        std::error_code ec;
        std::filesystem::create_directories(scriptsDirectory, ec);
        if (ec)
        {
            if (log) log("[Scripts] Could not create Assets/Scripts: " + ec.message());
            return false;
        }

        const auto headerPath = scriptsDirectory / (name + ".h");
        const auto sourcePath = scriptsDirectory / (name + ".cpp");
        if (std::filesystem::exists(headerPath) || std::filesystem::exists(sourcePath))
            return false;

        std::ofstream header(headerPath);
        std::ofstream source(sourcePath);
        if (!header || !source)
        {
            if (log) log("[Scripts] Could not write script files to: " + scriptsDirectory.string());
            return false;
        }

        header << "#pragma once\n"
               << "#include \"Engine/Scene/ScriptableEntity.h\"\n\n"
               << "namespace NoJob\n{\n"
               << "    class " << name << " final : public Script\n"
               << "    {\n    public:\n"
               << "        void OnCreate() override;\n"
               << "        void OnUpdate(float deltaTime) override;\n"
               << "        void OnDestroy() override;\n"
               << "    };\n}\n";

        source << "#include \"" << name << ".h\"\n"
               << "#include \"Engine/Scene/ScriptRegistry.h\"\n\n"
               << "namespace NoJob\n{\n"
               << "    void " << name << "::OnCreate()\n    {\n    }\n\n"
               << "    void " << name << "::OnUpdate(float deltaTime)\n"
               << "    {\n        (void)deltaTime;\n    }\n\n"
               << "    void " << name << "::OnDestroy()\n    {\n    }\n\n"
               << "}\n\n"
               << "NOJOB_REGISTER_SCRIPT(" << name << ", \"Gameplay\")\n";

        AssetRegistry registry(AssetManager::GetAssetsDirectory());
        registry.Load();
        registry.Register(headerPath, AssetType::Script);
        registry.Register(sourcePath, AssetType::Script);
        registry.Save();
        if (log) log("[Scripts] Created " + name + ".h / " + name + ".cpp in Assets/Scripts.");
        return true;
    }

    std::filesystem::path ProjectAssetOperations::CreatePrefab(
        Entity source, const std::filesystem::path& directory, const LogFunction& log)
    {
        if (!source || !source.HasComponent<TagComponent>()) return {};

        auto prefabName = source.GetComponent<TagComponent>().Tag;
        for (char& c : prefabName)
            if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' ||
                c == '"' || c == '<' || c == '>' || c == '|') c = '_';
        if (prefabName.empty()) prefabName = "Prefab";

        std::error_code ec;
        std::filesystem::create_directories(directory, ec);
        if (ec)
        {
            if (log) log("[Prefab] Could not create target directory: " + ec.message());
            return {};
        }

        auto prefabPath = directory / (prefabName + ".nojobprefab");
        int suffix = 1;
        while (std::filesystem::exists(prefabPath))
            prefabPath = directory / (prefabName + " (" + std::to_string(suffix++) + ").nojobprefab");

        if (!PrefabSerializer::Save(source, prefabPath))
        {
            if (log) log("[Prefab] Failed to save: " + prefabPath.string());
            return {};
        }

        AssetRegistry registry(AssetManager::GetAssetsDirectory());
        registry.Load();
        registry.Register(prefabPath, AssetType::Prefab);
        registry.Save();
        return prefabPath;
    }
}
