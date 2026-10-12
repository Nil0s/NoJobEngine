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

    inline ScriptFieldValue MakeScriptFieldValue(const std::string& value)
    { return ScriptFieldValue::MakeString(value); }

    template<typename T> inline T ReadScriptFieldValue(const ScriptFieldValue& value);
    template<> inline float ReadScriptFieldValue<float>(const ScriptFieldValue& value) { return value.Float; }
    template<> inline int ReadScriptFieldValue<int>(const ScriptFieldValue& value) { return value.Int; }
    template<> inline bool ReadScriptFieldValue<bool>(const ScriptFieldValue& value) { return value.Bool; }
    template<> inline std::string ReadScriptFieldValue<std::string>(const ScriptFieldValue& value) { return value.String; }
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

    // Editor-only metadata. No BTNativeRegistry, std::function or script instances.
    struct BTCallbackMetadata
    {
        std::string Name;
        bool IsCondition = false;
    };

    // Project scripts opt in by declaring a static DescribeBehaviorTreeBindings().
    // The same names must be registered in RegisterBehaviorTreeBindings().
    template<typename TScript>
    std::vector<BTCallbackMetadata> DescribeScriptBTCallbacks()
    {
        if constexpr (requires { TScript::DescribeBehaviorTreeBindings(); })
            return TScript::DescribeBehaviorTreeBindings();
        else
            return {};
    }

    struct ScriptDefinition
    {
        std::string Name;
        std::string Category = "Gameplay";
        std::vector<ScriptFieldDefinition> Fields;
        std::function<std::unique_ptr<Script>()> Factory;
        std::vector<BTCallbackMetadata> BehaviorTreeCallbacks;
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

        // Return a value-only catalog. Do not keep pointers into ScriptRegistry:
        // Unload() can invalidate those pointers during project DLL hot reload.
        static std::vector<BTCallbackMetadata> AllBehaviorTreeCallbacks()
        {
            std::vector<BTCallbackMetadata> callbacks;
            for (const auto& [scriptName, definition] : Registry())
            {
                (void)scriptName;
                callbacks.insert(callbacks.end(), definition.BehaviorTreeCallbacks.begin(),
                                 definition.BehaviorTreeCallbacks.end());
            }
            return callbacks;
        }

        static void ApplyDefaults(NativeScriptComponent& component)
        {
            const auto* def = Find(component.ScriptName);
            if (!def) return;

            // V1.9 migration: numeric TreeSlot -> named .njbt asset.
            if (component.ScriptName == "BTAssetAgent" &&
                !component.Fields.contains("BehaviorTreeAsset"))
            {
                const auto old = component.Fields.find("TreeSlot");
                if (old != component.Fields.end() && old->second.Type == ScriptFieldType::Int)
                {
                    const int slot = old->second.Int;
                    component.Fields["BehaviorTreeAsset"] = ScriptFieldValue::MakeString(
                        slot == 0 ? "Default" : "Tree_" + std::to_string(slot));
                }
            }
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
            [] { return std::make_unique<::NoJob::ScriptClass>(); }, \
            ::NoJob::DescribeScriptBTCallbacks<::NoJob::ScriptClass>() \
        }); \
    } \
    extern "C" __declspec(dllexport) void NoJobBTMetadata_##ScriptClass( \
        void(*emit)(const char*, int)) \
    { \
        if (!emit) return; \
        for (const auto& callback : ::NoJob::DescribeScriptBTCallbacks<::NoJob::ScriptClass>()) \
            emit(callback.Name.c_str(), callback.IsCondition ? 1 : 0); \
    }
#else
#define NOJOB_REGISTER_SCRIPT(ScriptClass, Category, ...)
#endif
