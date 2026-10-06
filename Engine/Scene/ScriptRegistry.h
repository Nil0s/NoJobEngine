#pragma once
#include "Engine/Scene/ScriptableEntity.h"
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <type_traits>
#include <utility>

namespace NoJob
{
    struct ScriptFieldDefinition
    {
        std::string Name;
        ScriptFieldValue DefaultValue;
        std::function<void(Script&, const ScriptFieldValue&)> SetMember;
        std::function<ScriptFieldValue(const Script&)> GetMember;
    };

    inline ScriptFieldValue MakeScriptFieldValue(float value)
    { return ScriptFieldValue::MakeFloat(value); }
    inline ScriptFieldValue MakeScriptFieldValue(int value)
    { return ScriptFieldValue::MakeInt(value); }
    inline ScriptFieldValue MakeScriptFieldValue(bool value)
    { return ScriptFieldValue::MakeBool(value); }
    inline ScriptFieldValue MakeScriptFieldValue(const glm::vec3& value)
    { return ScriptFieldValue::MakeVec3(value); }

    template<typename T> inline T ReadScriptFieldValue(const ScriptFieldValue& value);
    template<> inline float ReadScriptFieldValue<float>(const ScriptFieldValue& value) { return value.Float; }
    template<> inline int ReadScriptFieldValue<int>(const ScriptFieldValue& value) { return value.Int; }
    template<> inline bool ReadScriptFieldValue<bool>(const ScriptFieldValue& value) { return value.Bool; }
    template<> inline glm::vec3 ReadScriptFieldValue<glm::vec3>(const ScriptFieldValue& value) { return value.Vec3; }

    template<typename TScript, typename T>
    ScriptFieldDefinition MakeMemberFieldDefinition(
        const char* name, T TScript::* member, T defaultValue)
    {
        ScriptFieldDefinition field;
        field.Name = name;
        field.DefaultValue = MakeScriptFieldValue(defaultValue);
        field.SetMember = [member](Script& script, const ScriptFieldValue& value)
        {
            if (auto* typed = dynamic_cast<TScript*>(&script))
                typed->*member = ReadScriptFieldValue<T>(value);
        };
        field.GetMember = [member](const Script& script)
        {
            if (const auto* typed = dynamic_cast<const TScript*>(&script))
                return MakeScriptFieldValue(typed->*member);
            return ScriptFieldValue{};
        };
        return field;
    }

    struct ScriptDefinition
    {
        std::string Name;
        std::string Category = "Gameplay";
        std::vector<ScriptFieldDefinition> Fields;
        std::function<std::unique_ptr<Script>()> Factory;
    };

    class ScriptRegistry
    {
    public:
        static void Register(ScriptDefinition definition)
        {
            Registry()[definition.Name] = std::move(definition);
        }

        static const ScriptDefinition* Find(const std::string& name)
        {
            auto& r=Registry(); auto it=r.find(name);
            return it==r.end()?nullptr:&it->second;
        }

        static std::vector<const ScriptDefinition*> All()
        {
            std::vector<const ScriptDefinition*> result;
            for(auto& [name,def]:Registry()){(void)name;result.push_back(&def);}
            return result;
        }

        static void Unregister(const std::string& name)
        {
            Registry().erase(name);
        }

        static void ApplyDefaults(NativeScriptComponent& component)
        {
            const auto* def = Find(component.ScriptName);
            if (!def) return;

            std::unordered_map<std::string, ScriptFieldValue> migrated;
            for (const auto& field : def->Fields)
            {
                const auto existing = component.Fields.find(field.Name);
                if (existing != component.Fields.end() &&
                    existing->second.Type == field.DefaultValue.Type)
                    migrated[field.Name] = existing->second;
                else
                    migrated[field.Name] = field.DefaultValue;
            }
            component.Fields = std::move(migrated);
        }

        static void ApplyFieldsToInstance(
            const NativeScriptComponent& component, Script& instance)
        {
            const auto* def = Find(component.ScriptName);
            if (!def) return;
            for (const auto& field : def->Fields)
            {
                if (!field.SetMember) continue;
                const auto it = component.Fields.find(field.Name);
                if (it != component.Fields.end() &&
                    it->second.Type == field.DefaultValue.Type)
                    field.SetMember(instance, it->second);
            }
        }

        static void ReadFieldsFromInstance(
            NativeScriptComponent& component, const Script& instance)
        {
            const auto* def = Find(component.ScriptName);
            if (!def) return;
            for (const auto& field : def->Fields)
                if (field.GetMember)
                    component.Fields[field.Name] = field.GetMember(instance);
        }

    private:
        static std::unordered_map<std::string,ScriptDefinition>& Registry()
        {
            static std::unordered_map<std::string,ScriptDefinition> registry;
            return registry;
        }
    };
}


#define NOJOB_FIELD(ScriptClass, Member) \
    ::NoJob::MakeMemberFieldDefinition<ScriptClass>( \
        #Member, &ScriptClass::Member, ScriptClass{}.Member)

#ifdef _WIN32
#define NOJOB_REGISTER_SCRIPT(ScriptClass, Category, ...) \
    extern "C" __declspec(dllexport) void NoJobRegister_##ScriptClass( \
        void(*hostRegister)(::NoJob::ScriptDefinition)) \
    { \
        hostRegister({ \
            #ScriptClass, Category, { __VA_ARGS__ }, \
            [] { return std::make_unique<::NoJob::ScriptClass>(); } \
        }); \
    }
#else
#define NOJOB_REGISTER_SCRIPT(ScriptClass, Category, ...)
#endif
