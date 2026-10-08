#include "Editor/EditorLayer.h"
#include "Editor/Assets/ProjectAssetOperations.h"

#include "Engine/Asset/AssetManager.h"
#include "Engine/Renderer/Material.h"
#include "Engine/Renderer/Mesh.h"
#include "Engine/Renderer/Texture.h"
#include "Engine/Scene/Components.h"
#include "Engine/Scene/Scene.h"
#include "Engine/Scene/ScriptRegistry.h"
#include "Engine/Scene/NativeScripts.h"
#include "Engine/Assets/AssetRegistry.h"
#include "Engine/Assets/MaterialSerializer.h"
#include "Engine/Assets/PrefabSerializer.h"
#include "Engine/Animation/Animation.h"
#include "Engine/Audio/AudioEngine.h"

#include <imgui.h>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

namespace NoJob
{
    void EditorLayer::DrawInspector()
    {
        ImGui::Begin("Inspector");

        if (m_SelectedEntity)
        {
            auto& tag =
                m_SelectedEntity.GetComponent<TagComponent>().Tag;

            char tagBuffer[256]{};
            std::snprintf(
                tagBuffer,
                sizeof(tagBuffer),
                "%s",
                tag.c_str());

            if (m_RenameSelectedRequested)
            {
                ImGui::SetKeyboardFocusHere();
                m_RenameSelectedRequested = false;
            }
            if (ImGui::InputText(
                "##EntityName",
                tagBuffer,
                sizeof(tagBuffer),
                ImGuiInputTextFlags_EnterReturnsTrue))
            {
                tag = tagBuffer;
            }
            if (m_MultiSelection.size() > 1)
            {
                ImGui::SameLine();
                ImGui::TextDisabled("%zu selected", m_MultiSelection.size());
            }
            // Entity Layer
            if (m_SelectedEntity.HasComponent<LayerComponent>())
            {
                auto& layer =
                    m_SelectedEntity.GetComponent<LayerComponent>();

                constexpr const char* layerNames[] = {
                    "Default",
                    "Player",
                    "Enemy",
                    "Environment",
                    "Interactable"
                };

                const std::uint32_t currentLayer =
                    std::min(layer.Layer, std::uint32_t{ 31 });

                const char* preview =
                    currentLayer < IM_ARRAYSIZE(layerNames)
                    ? layerNames[currentLayer]
                    : "Custom Layer";

                if (ImGui::BeginCombo("Layer##Entity", preview))
                {
                    for (std::uint32_t i = 0; i < IM_ARRAYSIZE(layerNames); ++i)
                    {
                        const bool selected = layer.Layer == i;

                        if (ImGui::Selectable(layerNames[i], selected))
                        {
                            CaptureUndoSnapshot();
                            layer.Layer = i;
                        }

                        if (selected)
                            ImGui::SetItemDefaultFocus();
                    }

                    ImGui::EndCombo();
                }
            }
            ImGui::Separator();

            if (ImGui::CollapsingHeader(
                "Transform",
                ImGuiTreeNodeFlags_DefaultOpen))
            {
                auto& transform =
                    m_SelectedEntity.GetComponent<TransformComponent>();

                // Unity-style numeric transform fields with Undo/Redo transactions.
                auto editTransform = [&](const char* label, glm::vec3& value)
                    {
                        ImGui::SetNextItemWidth(-1.0f);
                        ImGui::DragFloat3(
                            label,
                            &value.x,
                            0.01f,
                            0.0f,
                            0.0f,
                            "%.3f");

                        if (ImGui::IsItemActivated() && !m_TransformEditSnapshot)
                            m_TransformEditSnapshot = m_Scene->Copy();

                        if (ImGui::IsItemDeactivatedAfterEdit()
                            && m_TransformEditSnapshot)
                        {
                            PushUndoSnapshot(std::move(m_TransformEditSnapshot));
                        }
                        else if (ImGui::IsItemDeactivated()
                            && m_TransformEditSnapshot)
                        {
                            m_TransformEditSnapshot.reset();
                        }
                    };

                editTransform("Position", transform.Position);
                editTransform("Rotation", transform.Rotation);
                editTransform("Scale", transform.Scale);

                ImGui::TextDisabled(
                    "Ctrl + click to type | Ctrl+Z / Ctrl+Y to undo/redo.");
            }

            {
                Entity parent = m_Scene->GetParent(m_SelectedEntity);
                if (parent)
                {
                    ImGui::TextDisabled(
                        "Parent: %s",
                        parent.GetComponent<TagComponent>().Tag.c_str());

                    ImGui::SameLine();
                    if (ImGui::SmallButton("Unparent"))
                    {
                        CaptureUndoSnapshot();
                        m_Scene->Unparent(m_SelectedEntity, true);
                    }
                }
                else
                {
                    ImGui::TextDisabled("Parent: None");
                }
            }

            if (m_SelectedEntity.HasComponent<MeshComponent>())
            {
                ImGui::Separator();

                if (ImGui::CollapsingHeader(
                    "Mesh",
                    ImGuiTreeNodeFlags_DefaultOpen))
                {
                    ImGui::TextDisabled("Primitive Mesh");
                }
            }

            ImGui::Separator();
            if (m_SelectedEntity.HasComponent<NativeScriptComponent>())
            {
                auto& script =
                    m_SelectedEntity.GetComponent<NativeScriptComponent>();
                RegisterBuiltinScripts();
                ScriptRegistry::ApplyDefaults(script);

                const std::string header =
                    "Native Script: " + script.ScriptName;
                if (ImGui::CollapsingHeader(
                    header.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
                {
                    auto beginEdit = [&]()
                        {
                            if (ImGui::IsItemActivated() &&
                                !m_ScriptFieldEditSnapshot)
                                m_ScriptFieldEditSnapshot = m_Scene->Copy();
                        };
                    auto endEdit = [&]()
                        {
                            if (ImGui::IsItemDeactivatedAfterEdit() &&
                                m_ScriptFieldEditSnapshot)
                                PushUndoSnapshot(
                                    std::move(m_ScriptFieldEditSnapshot));
                            else if (ImGui::IsItemDeactivated() &&
                                m_ScriptFieldEditSnapshot)
                                m_ScriptFieldEditSnapshot.reset();
                        };

                    ImGui::Checkbox(
                        "Enabled##NativeScript", &script.Enabled);
                    beginEdit(); endEdit();
                    ImGui::TextDisabled("C++ Native Script");

                    const auto* definition =
                        ScriptRegistry::Find(script.ScriptName);
                    if (!definition)
                    {
                        ImGui::TextDisabled(
                            "Script definition not loaded. Compile Scripts.");
                    }
                    else
                    {
                        for (const auto& fieldDef : definition->Fields)
                        {
                            auto it = script.Fields.find(fieldDef.Name);
                            if (it == script.Fields.end()) continue;
                            auto& field = it->second;

                            ImGui::PushID(fieldDef.Name.c_str());
                            if (field.Type == ScriptFieldType::Float)
                                ImGui::DragFloat(
                                    fieldDef.Name.c_str(), &field.Float, 0.05f);
                            else if (field.Type == ScriptFieldType::Int)
                                ImGui::DragInt(
                                    fieldDef.Name.c_str(), &field.Int);
                            else if (field.Type == ScriptFieldType::Bool)
                                ImGui::Checkbox(
                                    fieldDef.Name.c_str(), &field.Bool);
                            else if (field.Type == ScriptFieldType::Vec3)
                                ImGui::DragFloat3(
                                    fieldDef.Name.c_str(), &field.Vec3.x, 0.05f);
                            beginEdit(); endEdit();
                            ImGui::PopID();
                        }
                        ImGui::TextDisabled(
                            "Scene/Prefab persistent | Hot-reload safe");
                    }
                }
            }

            ImGui::Separator();
            if (m_SelectedEntity.HasComponent<RigidbodyComponent>())
            {
                if (ImGui::CollapsingHeader(
                    "Rigidbody",
                    ImGuiTreeNodeFlags_DefaultOpen))
                {
                    auto& body =
                        m_SelectedEntity.GetComponent<RigidbodyComponent>();

                    const char* bodyTypes[] = {
                        "Static", "Dynamic", "Kinematic"
                    };
                    int type = static_cast<int>(body.Type);
                    if (ImGui::Combo(
                        "Body Type",
                        &type,
                        bodyTypes,
                        IM_ARRAYSIZE(bodyTypes)))
                    {
                        body.Type = static_cast<RigidbodyType>(type);
                    }

                    if (body.Type == RigidbodyType::Dynamic)
                    {
                        ImGui::DragFloat(
                            "Mass",
                            &body.Mass,
                            0.05f,
                            0.001f,
                            10000.0f);
                        ImGui::Checkbox("Use Gravity", &body.UseGravity);
                    }
                    else if (body.Type == RigidbodyType::Kinematic)
                    {
                        ImGui::Checkbox("Use Gravity", &body.UseGravity);
                        ImGui::TextDisabled(
                            "Kinematic motion control comes in a later step.");
                    }

                    if (ImGui::Button("Remove Rigidbody"))
                        m_SelectedEntity.RemoveComponent<RigidbodyComponent>();
                }
            }
            if (m_SelectedEntity.HasComponent<NavAgentComponent>())
            {
                ImGui::Separator();

                if (ImGui::CollapsingHeader(
                    "Nav Agent",
                    ImGuiTreeNodeFlags_DefaultOpen))
                {
                    auto& agent =
                        m_SelectedEntity.GetComponent<NavAgentComponent>();

                    ImGui::Checkbox(
                        "Enabled##NavAgent",
                        &agent.Enabled);

                    ImGui::DragFloat(
                        "Speed##NavAgent",
                        &agent.Speed,
                        0.05f,
                        0.0f,
                        100.0f,
                        "%.2f");

                    ImGui::DragFloat(
                        "Stopping Distance##NavAgent",
                        &agent.StoppingDistance,
                        0.01f,
                        0.0f,
                        100.0f,
                        "%.2f");

                    ImGui::DragFloat(
                        "Repath Interval##NavAgent",
                        &agent.RepathInterval,
                        0.05f,
                        0.05f,
                        10.0f,
                        "%.2f s");

                    agent.RepathInterval =
                        std::clamp(agent.RepathInterval, 0.05f, 10.0f);
                    agent.Speed =
                        std::max(
                            agent.Speed,
                            0.0f);

                    agent.StoppingDistance =
                        std::max(
                            agent.StoppingDistance,
                            0.0f);

                    ImGui::SeparatorText(
                        "Destination");

                    ImGui::DragFloat3(
                        "Position##NavAgentDestination",
                        &agent.Destination.x,
                        0.05f,
                        0.0f,
                        0.0f,
                        "%.3f");

                    ImGui::Checkbox(
                        "Has Destination##NavAgent",
                        &agent.HasDestination);

                    ImGui::TextDisabled(
                        "Path is calculated in Play Mode "
                        "using the baked NavMesh.");

                    if (ImGui::Button(
                        "Remove Nav Agent"))
                    {
                        m_SelectedEntity
                            .RemoveComponent<
                            NavAgentComponent>();
                    }
                }
            }

            if (m_SelectedEntity.HasComponent<PerceptionComponent>())
            {
                ImGui::Separator();
                if (ImGui::CollapsingHeader("Perception", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    auto& perception = m_SelectedEntity.GetComponent<PerceptionComponent>();
                    ImGui::Checkbox("Enabled##Perception", &perception.Enabled);
                    ImGui::SeparatorText("Detection Layers");

                    constexpr const char* detectionLayerNames[] = {
                        "Default",
                        "Player",
                        "Enemy",
                        "Environment",
                        "Interactable"
                    };

                    for (std::uint32_t i = 0;
                        i < IM_ARRAYSIZE(detectionLayerNames);
                        ++i)
                    {
                        const LayerMask bit = EntityLayers::Bit(i);

                        bool enabled = (perception.DetectionMask & bit) != 0;

                        if (ImGui::Checkbox(detectionLayerNames[i], &enabled))
                        {
                            CaptureUndoSnapshot();

                            if (enabled)
                                perception.DetectionMask |= bit;
                            else
                                perception.DetectionMask &= ~bit;
                        }
                    }
                    ImGui::DragFloat("Detection Radius##Perception", &perception.DetectionRadius,
                        0.1f, 0.0f, 10000.0f, "%.2f");
                    ImGui::SliderFloat("Field of View##Perception", &perception.FieldOfView,
                        0.0f, 360.0f, "%.1f deg");
                    ImGui::DragFloat("Memory Duration##Perception", &perception.MemoryDuration,
                        0.05f, 0.0f, 3600.0f, "%.2f s");
                    ImGui::DragFloat("Update Interval##Perception", &perception.UpdateInterval,
                        0.01f, 0.01f, 60.0f, "%.2f s");
                    ImGui::Checkbox("Debug Draw##Perception", &perception.DebugDraw);
                    perception.DetectionRadius = std::max(0.0f, perception.DetectionRadius);
                    perception.FieldOfView = std::clamp(perception.FieldOfView, 0.0f, 360.0f);
                    perception.MemoryDuration = std::max(0.0f, perception.MemoryDuration);
                    perception.UpdateInterval = std::clamp(perception.UpdateInterval, 0.01f, 60.0f);
                    if (ImGui::Button("Remove Perception"))
                    {
                        CaptureUndoSnapshot();
                        m_SelectedEntity.RemoveComponent<PerceptionComponent>();
                    }
                }
            }

            if (m_SelectedEntity.HasComponent<BoxColliderComponent>())
            {
                if (ImGui::CollapsingHeader(
                    "Box Collider",
                    ImGuiTreeNodeFlags_DefaultOpen))
                {
                    auto& c = m_SelectedEntity.GetComponent<BoxColliderComponent>();
                    ImGui::DragFloat3("Size##Box", &c.Size.x, 0.05f, 0.01f, 1000.0f);
                    c.Size = glm::max(c.Size, glm::vec3(0.01f));
                    ImGui::Checkbox("Is Trigger##Box", &c.IsTrigger);
                    ImGui::SliderFloat("Friction##Box", &c.Material.Friction, 0.0f, 1.0f);
                    ImGui::SliderFloat("Bounciness##Box", &c.Material.Bounciness, 0.0f, 1.0f);
                    if (ImGui::Button("Remove Box Collider"))
                        m_SelectedEntity.RemoveComponent<BoxColliderComponent>();
                }
            }


            if (m_SelectedEntity.HasComponent<SphereColliderComponent>())
            {
                if (ImGui::CollapsingHeader(
                    "Sphere Collider",
                    ImGuiTreeNodeFlags_DefaultOpen))
                {
                    auto& c = m_SelectedEntity.GetComponent<SphereColliderComponent>();
                    ImGui::DragFloat("Radius##Sphere", &c.Radius, 0.02f, 0.01f, 1000.0f);
                    c.Radius = std::max(c.Radius, 0.01f);
                    ImGui::Checkbox("Is Trigger##Sphere", &c.IsTrigger);
                    ImGui::SliderFloat("Friction##Sphere", &c.Material.Friction, 0.0f, 1.0f);
                    ImGui::SliderFloat("Bounciness##Sphere", &c.Material.Bounciness, 0.0f, 1.0f);
                    if (ImGui::Button("Remove Sphere Collider"))
                        m_SelectedEntity.RemoveComponent<SphereColliderComponent>();
                }
            }


            if (m_SelectedEntity.HasComponent<CapsuleColliderComponent>())
            {
                if (ImGui::CollapsingHeader(
                    "Capsule Collider",
                    ImGuiTreeNodeFlags_DefaultOpen))
                {
                    auto& c = m_SelectedEntity.GetComponent<CapsuleColliderComponent>();
                    ImGui::DragFloat("Radius##Capsule", &c.Radius, 0.02f, 0.01f, 1000.0f);
                    ImGui::DragFloat("Height##Capsule", &c.Height, 0.05f, 0.02f, 1000.0f);
                    c.Radius = std::max(c.Radius, 0.01f);
                    c.Height = std::max(c.Height, c.Radius * 2.0f);
                    ImGui::Checkbox("Is Trigger##Capsule", &c.IsTrigger);
                    ImGui::SliderFloat("Friction##Capsule", &c.Material.Friction, 0.0f, 1.0f);
                    ImGui::SliderFloat("Bounciness##Capsule", &c.Material.Bounciness, 0.0f, 1.0f);
                    if (ImGui::Button("Remove Capsule Collider"))
                        m_SelectedEntity.RemoveComponent<CapsuleColliderComponent>();
                }
            }


            ImGui::Separator();

            if (m_SelectedEntity.HasComponent<ParticleSystemComponent>())
            {
                if (ImGui::CollapsingHeader("Particle System", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    auto& p = m_SelectedEntity.GetComponent<ParticleSystemComponent>();
                    ImGui::Checkbox("Playing##Particles", &p.Playing);
                    ImGui::SameLine();
                    ImGui::Checkbox("Loop##Particles", &p.Loop);
                    ImGui::DragFloat("Duration##Particles", &p.Duration, 0.1f, 0.0f, 120.0f);
                    ImGui::SeparatorText("Main");
                    ImGui::DragFloat("Start Lifetime##Particles", &p.StartLifetime, 0.05f, 0.01f, 60.0f);
                    ImGui::SliderFloat("Lifetime Random##Particles", &p.LifetimeRandom, 0.0f, 1.0f);
                    ImGui::DragFloat("Start Speed##Particles", &p.StartSpeed, 0.05f, -100.0f, 100.0f);
                    ImGui::SliderFloat("Speed Random##Particles", &p.SpeedRandom, 0.0f, 1.0f);
                    ImGui::DragFloat("Start Size##Particles", &p.StartSize, 0.01f, 0.001f, 20.0f);
                    ImGui::SliderFloat("Size Random##Particles", &p.SizeRandom, 0.0f, 1.0f);
                    ImGui::ColorEdit4("Start Color##Particles", &p.StartColor.x);
                    ImGui::ColorEdit4("End Color##Particles", &p.EndColor.x);
                    ImGui::SliderFloat("End Size Multiplier##Particles", &p.EndSizeMultiplier, 0.0f, 4.0f);
                    ImGui::DragFloat3("Gravity##Particles", &p.Gravity.x, 0.02f);

                    ImGui::SeparatorText("Emission");
                    ImGui::DragFloat("Emission Rate##Particles", &p.EmissionRate, 0.5f, 0.0f, 10000.0f);
                    int maxParticles = static_cast<int>(p.MaxParticles);
                    if (ImGui::DragInt("Max Particles##Particles", &maxParticles, 1.0f, 1, 100000))
                        p.MaxParticles = static_cast<std::uint32_t>(std::max(maxParticles, 1));

                    ImGui::SeparatorText("Shape");
                    const char* shapes[] = { "Point", "Sphere", "Cone" };
                    int shape = static_cast<int>(p.Shape);
                    if (ImGui::Combo("Emitter Shape##Particles", &shape, shapes, 3))
                        p.Shape = static_cast<ParticleShape>(shape);
                    ImGui::DragFloat3("Direction##Particles", &p.Direction.x, 0.02f);
                    if (p.Shape == ParticleShape::Sphere)
                        ImGui::DragFloat("Sphere Radius##Particles", &p.ShapeRadius, 0.02f, 0.0f, 100.0f);
                    if (p.Shape == ParticleShape::Cone)
                        ImGui::SliderFloat("Cone Angle##Particles", &p.ConeAngle, 0.0f, 89.0f);

                    ImGui::SeparatorText("Renderer");
                    const char* blends[] = { "Alpha", "Additive" };
                    int blend = static_cast<int>(p.BlendMode);
                    if (ImGui::Combo("Blend Mode##Particles", &blend, blends, 2))
                        p.BlendMode = static_cast<ParticleBlendMode>(blend);
                    ImGui::TextWrapped("Texture: %s", p.TexturePath.empty() ? "<none>" : p.TexturePath.c_str());
                    ImGui::Button(p.TexturePath.empty() ? "Drop Texture Here##Particles" : "Texture Assigned##Particles", ImVec2(-1.0f, 0.0f));
                    if (ImGui::BeginDragDropTarget())
                    {
                        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("NOJOB_TEXTURE_ASSET"))
                            p.TexturePath = static_cast<const char*>(payload->Data);
                        ImGui::EndDragDropTarget();
                    }
                    if (!p.TexturePath.empty())
                    {
                        if (ImGui::Button("Clear Texture##Particles")) p.TexturePath.clear();
                    }
                    ImGui::TextDisabled("Simulation runs in Play Mode. Rendering is instanced per emitter.");
                    if (ImGui::Button("Remove Particle System"))
                        m_SelectedEntity.RemoveComponent<ParticleSystemComponent>();
                }
            }

            ImGui::Separator();

            if (m_SelectedEntity.HasComponent<AudioSourceComponent>())
            {
                if (ImGui::CollapsingHeader("Audio Source", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    auto& audio = m_SelectedEntity.GetComponent<AudioSourceComponent>();
                    ImGui::TextWrapped("Clip: %s", audio.ClipPath.empty() ? "<none>" : audio.ClipPath.c_str());
                    ImGui::Button(audio.ClipPath.empty() ? "Drop Audio Clip Here" : "Audio Clip Assigned", ImVec2(-1.0f, 0.0f));
                    if (ImGui::BeginDragDropTarget())
                    {
                        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("NOJOB_AUDIO_ASSET"))
                        {
                            audio.ClipPath = static_cast<const char*>(payload->Data);
                            AudioEngine::Stop(m_SelectedEntity.GetHandle());
                        }
                        ImGui::EndDragDropTarget();
                    }
                    if (ImGui::Button("Load Audio Clip"))
                    {
                        const std::string path = OpenInspectorAudioFileDialog();
                        if (!path.empty())
                        {
                            std::filesystem::path selected(path);
                            std::error_code ec;
                            const auto relative = std::filesystem::relative(selected, m_ProjectPanel.GetCurrentDirectoryPath(), ec);
                            audio.ClipPath = (!ec && !relative.empty() && relative.generic_string().rfind("..", 0) != 0)
                                ? relative.generic_string() : selected.lexically_normal().string();
                        }
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Clear##AudioClip")) audio.ClipPath.clear();
                    ImGui::Checkbox("Play On Awake", &audio.PlayOnAwake);
                    ImGui::Checkbox("Loop##Audio", &audio.Loop);
                    ImGui::SliderFloat("Volume##Audio", &audio.Volume, 0.0f, 1.0f);
                    ImGui::SliderFloat("Pitch##Audio", &audio.Pitch, 0.1f, 3.0f);
                    ImGui::SeparatorText("Spatial Audio");
                    ImGui::SliderFloat("Spatial Blend##Audio", &audio.SpatialBlend, 0.0f, 1.0f, "%.2f");
                    ImGui::DragFloat("Min Distance##Audio", &audio.MinDistance, 0.1f, 0.01f, 10000.0f);
                    ImGui::DragFloat("Max Distance##Audio", &audio.MaxDistance, 0.25f, 0.02f, 100000.0f);
                    ImGui::SliderFloat("Doppler Factor##Audio", &audio.DopplerFactor, 0.0f, 5.0f);
                    audio.MinDistance = std::max(0.01f, audio.MinDistance);
                    audio.MaxDistance = std::max(audio.MinDistance + 0.01f, audio.MaxDistance);
                    audio.SpatialBlend = std::clamp(audio.SpatialBlend, 0.0f, 1.0f);
                    if (audio.SpatialBlend <= 0.001f)
                        ImGui::TextDisabled("2D: position and distance attenuation are disabled.");
                    else
                        ImGui::TextDisabled("3D: source follows the entity world Transform.");
                    if (ImGui::Button("Preview Play"))
                    {
                        const glm::mat4 world = m_Scene->GetWorldTransform(m_SelectedEntity);
                        AudioEngine::Play(m_SelectedEntity.GetHandle(), audio, glm::vec3(world[3]));
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Preview Stop")) AudioEngine::Stop(m_SelectedEntity.GetHandle());
                    if (!AudioEngine::GetLastError().empty())
                        ImGui::TextWrapped("Audio: %s", AudioEngine::GetLastError().c_str());
                    if (ImGui::Button("Remove Audio Source"))
                    {
                        AudioEngine::Stop(m_SelectedEntity.GetHandle());
                        m_SelectedEntity.RemoveComponent<AudioSourceComponent>();
                    }
                }
            }

            if (m_SelectedEntity.HasComponent<AudioListenerComponent>())
            {
                if (ImGui::CollapsingHeader("Audio Listener", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    auto& listener = m_SelectedEntity.GetComponent<AudioListenerComponent>();
                    ImGui::Checkbox("Enabled##AudioListener", &listener.Enabled);
                    ImGui::TextDisabled("Uses this entity's world position and orientation.");
                    ImGui::TextDisabled("The first enabled listener in the runtime scene is active.");
                    if (ImGui::Button("Remove Audio Listener"))
                        m_SelectedEntity.RemoveComponent<AudioListenerComponent>();
                }
            }

            if (m_SelectedEntity.HasComponent<CameraComponent>())
            {
                if (ImGui::CollapsingHeader(
                    "Camera",
                    ImGuiTreeNodeFlags_DefaultOpen))
                {
                    auto& camera =
                        m_SelectedEntity.GetComponent<CameraComponent>();

                    ImGui::Checkbox("Primary", &camera.Primary);

                    const char* projectionTypes[] =
                    { "Perspective", "Orthographic" };
                    int projectionType =
                        static_cast<int>(camera.ProjectionType);
                    if (ImGui::Combo(
                        "Projection",
                        &projectionType,
                        projectionTypes,
                        IM_ARRAYSIZE(projectionTypes)))
                    {
                        camera.ProjectionType =
                            static_cast<CameraProjectionType>(projectionType);
                    }

                    if (camera.ProjectionType ==
                        CameraProjectionType::Perspective)
                    {
                        ImGui::SliderFloat(
                            "Field of View",
                            &camera.PerspectiveFOV,
                            1.0f, 179.0f);
                        ImGui::DragFloat(
                            "Near Clip",
                            &camera.PerspectiveNear,
                            0.01f, 0.001f, 100.0f);
                        ImGui::DragFloat(
                            "Far Clip",
                            &camera.PerspectiveFar,
                            1.0f, 1.0f, 100000.0f);
                        camera.PerspectiveFar =
                            std::max(
                                camera.PerspectiveFar,
                                camera.PerspectiveNear + 0.01f);
                    }
                    else
                    {
                        ImGui::DragFloat(
                            "Size",
                            &camera.OrthographicSize,
                            0.1f, 0.01f, 10000.0f);
                        ImGui::DragFloat(
                            "Near Clip##Ortho",
                            &camera.OrthographicNear,
                            0.1f);
                        ImGui::DragFloat(
                            "Far Clip##Ortho",
                            &camera.OrthographicFar,
                            1.0f);
                    }

                    if (ImGui::Button("Remove Camera"))
                        m_SelectedEntity.RemoveComponent<CameraComponent>();
                }
            }


            if (m_SelectedEntity.HasComponent<DirectionalLightComponent>())
            {
                if (ImGui::CollapsingHeader(
                    "Directional Light",
                    ImGuiTreeNodeFlags_DefaultOpen))
                {
                    auto& light =
                        m_SelectedEntity.GetComponent<DirectionalLightComponent>();
                    ImGui::ColorEdit3("Color##Directional", &light.Color.x);
                    ImGui::DragFloat(
                        "Intensity##Directional",
                        &light.Intensity, 0.05f, 0.0f, 100.0f);
                    ImGui::Checkbox("Cast Shadows##Directional", &light.CastShadows);
                    ImGui::DragFloat("Shadow Bias##Directional", &light.ShadowBias, 0.0001f, 0.00001f, 0.05f, "%.5f");
                    if (ImGui::Button("Remove Directional Light"))
                        m_SelectedEntity.RemoveComponent<DirectionalLightComponent>();
                }
            }


            if (m_SelectedEntity.HasComponent<PointLightComponent>())
            {
                if (ImGui::CollapsingHeader(
                    "Point Light",
                    ImGuiTreeNodeFlags_DefaultOpen))
                {
                    auto& light =
                        m_SelectedEntity.GetComponent<PointLightComponent>();
                    ImGui::ColorEdit3("Color##Point", &light.Color.x);
                    ImGui::DragFloat(
                        "Intensity##Point",
                        &light.Intensity, 0.05f, 0.0f, 100.0f);
                    ImGui::DragFloat(
                        "Range##Point",
                        &light.Range, 0.1f, 0.01f, 10000.0f);
                    ImGui::Checkbox("Cast Shadows##Point", &light.CastShadows);
                    ImGui::DragFloat("Shadow Bias##Point", &light.ShadowBias, 0.001f, 0.001f, 0.25f, "%.4f");
                    if (ImGui::Button("Remove Point Light"))
                        m_SelectedEntity.RemoveComponent<PointLightComponent>();
                }
            }


            if (m_SelectedEntity.HasComponent<SpotLightComponent>())
            {
                if (ImGui::CollapsingHeader(
                    "Spot Light",
                    ImGuiTreeNodeFlags_DefaultOpen))
                {
                    auto& light =
                        m_SelectedEntity.GetComponent<SpotLightComponent>();
                    ImGui::ColorEdit3("Color##Spot", &light.Color.x);
                    ImGui::DragFloat(
                        "Intensity##Spot",
                        &light.Intensity, 0.05f, 0.0f, 100.0f);
                    ImGui::DragFloat(
                        "Range##Spot",
                        &light.Range, 0.1f, 0.01f, 10000.0f);
                    ImGui::SliderFloat(
                        "Inner Angle",
                        &light.InnerAngle, 0.1f, 89.0f);
                    ImGui::SliderFloat(
                        "Outer Angle",
                        &light.OuterAngle, 0.1f, 89.0f);
                    light.OuterAngle =
                        std::max(light.OuterAngle, light.InnerAngle);
                    ImGui::Checkbox("Cast Shadows##Spot", &light.CastShadows);
                    ImGui::DragFloat("Shadow Bias##Spot", &light.ShadowBias, 0.0001f, 0.00001f, 0.05f, "%.5f");
                    if (ImGui::Button("Remove Spot Light"))
                        m_SelectedEntity.RemoveComponent<SpotLightComponent>();
                }
            }


            if (m_SelectedEntity.HasComponent<AnimatorComponent>())
            {
                ImGui::Separator();
                if (ImGui::CollapsingHeader("Animator", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    auto& animator = m_SelectedEntity.GetComponent<AnimatorComponent>();
                    ImGui::Checkbox("Playing", &animator.Playing);
                    ImGui::SameLine(); ImGui::Checkbox("Loop", &animator.Loop);
                    ImGui::DragFloat("Speed", &animator.Speed, 0.05f, -4.0f, 4.0f);
                    if (animator.Animation && !animator.Animation->Clips().empty())
                    {
                        const auto& clips = animator.Animation->Clips();
                        animator.ClipIndex = std::clamp(animator.ClipIndex, 0, (int)clips.size() - 1);
                        if (ImGui::BeginCombo("Clip", clips[animator.ClipIndex].Name.c_str()))
                        {
                            for (int ci = 0;ci < (int)clips.size();++ci)
                                if (ImGui::Selectable(clips[ci].Name.c_str(), ci == animator.ClipIndex))
                                {
                                    animator.ClipIndex = ci; animator.TimeSeconds = 0.0f;
                                }
                            ImGui::EndCombo();
                        }
                        ImGui::Text("Skeleton bones: %zu", animator.Animation->Bones().size());
                        ImGui::Text("Time: %.2f / %.2f s", animator.TimeSeconds,
                            (float)clips[animator.ClipIndex].DurationSeconds());
                    }
                    else ImGui::TextDisabled("No animation asset loaded.");
                }
            }

            DrawComponentTools();

            ImGui::Separator();
            ImGui::Spacing();
            const float addWidth = std::min(260.0f, ImGui::GetContentRegionAvail().x);
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() +
                std::max(0.0f, (ImGui::GetContentRegionAvail().x - addWidth) * 0.5f));
            if (ImGui::Button("Add Component", ImVec2(addWidth, 0.0f)))
                ImGui::OpenPopup("AddComponentPopup");

            if (ImGui::BeginPopup("AddComponentPopup"))
            {
                static char componentSearch[96]{};
                ImGui::SetNextItemWidth(300.0f);
                ImGui::InputTextWithHint("##ComponentSearch", "Search components...",
                    componentSearch, sizeof(componentSearch));
                ImGui::Separator();

                std::string filter = componentSearch;
                std::transform(filter.begin(), filter.end(), filter.begin(),
                    [](unsigned char c) { return (char)std::tolower(c); });
                auto visible = [&](const char* name)
                    {
                        if (filter.empty()) return true;
                        std::string n = name;
                        std::transform(n.begin(), n.end(), n.begin(),
                            [](unsigned char c) { return (char)std::tolower(c); });
                        return n.find(filter) != std::string::npos;
                    };
                auto addItem = [&](const char* category, const char* name, bool enabled, auto add)
                    {
                        if (!visible(name)) return;
                        ImGui::TextDisabled("%s", category);
                        ImGui::SameLine(95.0f);
                        if (!enabled) ImGui::BeginDisabled();
                        if (ImGui::Selectable(name, false, enabled ? 0 : ImGuiSelectableFlags_Disabled))
                        {
                            CaptureUndoSnapshot();
                            add();
                            ImGui::CloseCurrentPopup();
                        }
                        if (!enabled) ImGui::EndDisabled();
                    };

                addItem("Physics", "Rigidbody",
                    !m_SelectedEntity.HasComponent<RigidbodyComponent>(),
                    [&] { m_SelectedEntity.AddComponent<RigidbodyComponent>(); });

                addItem("AI", "Nav Agent",
                    !m_SelectedEntity.HasComponent<NavAgentComponent>(),
                    [&]
                    {
                        m_SelectedEntity
                            .AddComponent<
                            NavAgentComponent>();
                    });

                addItem("AI", "Perception",
                    !m_SelectedEntity.HasComponent<PerceptionComponent>(),
                    [&] { m_SelectedEntity.AddComponent<PerceptionComponent>(); });

                const bool noCollider =
                    !m_SelectedEntity.HasComponent<BoxColliderComponent>() &&
                    !m_SelectedEntity.HasComponent<SphereColliderComponent>() &&
                    !m_SelectedEntity.HasComponent<CapsuleColliderComponent>();
                addItem("Physics", "Box Collider", noCollider,
                    [&] { m_SelectedEntity.AddComponent<BoxColliderComponent>(); });
                addItem("Physics", "Sphere Collider", noCollider,
                    [&] { m_SelectedEntity.AddComponent<SphereColliderComponent>(); });
                addItem("Physics", "Capsule Collider", noCollider,
                    [&] { m_SelectedEntity.AddComponent<CapsuleColliderComponent>(); });

                const bool noViewLight =
                    !m_SelectedEntity.HasComponent<CameraComponent>() &&
                    !m_SelectedEntity.HasComponent<DirectionalLightComponent>() &&
                    !m_SelectedEntity.HasComponent<PointLightComponent>() &&
                    !m_SelectedEntity.HasComponent<SpotLightComponent>();
                addItem("Rendering", "Camera", noViewLight,
                    [&] { m_SelectedEntity.AddComponent<CameraComponent>(); });
                addItem("Rendering", "Directional Light", noViewLight,
                    [&] { m_SelectedEntity.AddComponent<DirectionalLightComponent>(); });
                addItem("Rendering", "Point Light", noViewLight,
                    [&] { m_SelectedEntity.AddComponent<PointLightComponent>(); });
                addItem("Rendering", "Spot Light", noViewLight,
                    [&] { m_SelectedEntity.AddComponent<SpotLightComponent>(); });

                addItem("Effects", "Particle System",
                    !m_SelectedEntity.HasComponent<ParticleSystemComponent>(),
                    [&] { m_SelectedEntity.AddComponent<ParticleSystemComponent>(); });

                addItem("Audio", "Audio Source",
                    !m_SelectedEntity.HasComponent<AudioSourceComponent>(),
                    [&] { m_SelectedEntity.AddComponent<AudioSourceComponent>(); });
                addItem("Audio", "Audio Listener",
                    !m_SelectedEntity.HasComponent<AudioListenerComponent>(),
                    [&] { m_SelectedEntity.AddComponent<AudioListenerComponent>(); });

                addItem("Animation", "Animator",
                    !m_SelectedEntity.HasComponent<AnimatorComponent>(),
                    [&] { m_SelectedEntity.AddComponent<AnimatorComponent>(); });
                RegisterBuiltinScripts();
                for (const auto* definition : ScriptRegistry::All())
                {
                    const std::string label = definition->Name + " Script";
                    addItem("Scripting", label.c_str(),
                        !m_SelectedEntity.HasComponent<NativeScriptComponent>(),
                        [&, definition] {
                            NativeScriptComponent component;
                            component.ScriptName = definition->Name;
                            ScriptRegistry::ApplyDefaults(component);
                            m_SelectedEntity.AddComponent<NativeScriptComponent>(component);
                        });
                }

                ImGui::EndPopup();
            }

            if (m_SelectedEntity.HasComponent<PrefabInstanceComponent>())
            {
                ImGui::Separator();
                if (ImGui::CollapsingHeader("Prefab Instance", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    auto& pi = m_SelectedEntity.GetComponent<PrefabInstanceComponent>();
                    ImGui::TextWrapped("Source: %s", pi.SourcePath.c_str());
                    if (ImGui::Button("Apply to Prefab"))
                    {
                        CaptureUndoSnapshot();
                        PrefabSerializer::Apply(m_SelectedEntity, pi.SourcePath);
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Revert"))
                    {
                        CaptureUndoSnapshot();
                        Entity reverted = PrefabSerializer::Revert(
                            m_SelectedEntity, m_DefaultCubeMesh, m_DefaultCubeMaterial);
                        if (reverted) m_SelectedEntity = reverted;
                    }
                }
            }

            if (m_SelectedEntity.HasComponent<MeshRendererComponent>())
            {
                ImGui::Separator();

                if (ImGui::CollapsingHeader(
                    "Mesh Renderer",
                    ImGuiTreeNodeFlags_DefaultOpen))
                {
                    auto& renderer =
                        m_SelectedEntity.GetComponent<MeshRendererComponent>();

                    // Upgrade V1 renderers to the slot representation lazily.
                    if (renderer.Materials.empty() && renderer.MaterialAsset)
                        renderer.Materials.push_back(renderer.MaterialAsset);
                    else if (!renderer.Materials.empty() && !renderer.MaterialAsset)
                        renderer.MaterialAsset = renderer.Materials.front();

                    if (!renderer.Materials.empty())
                    {
                        if (m_SelectedMaterialSlot >= renderer.Materials.size())
                            m_SelectedMaterialSlot = 0;

                        ImGui::SeparatorText("Materials");
                        for (std::size_t slot = 0;
                            slot < renderer.Materials.size();
                            ++slot)
                        {
                            ImGui::PushID(static_cast<int>(slot));

                            const bool selected =
                                slot == m_SelectedMaterialSlot;
                            const char* state =
                                renderer.Materials[slot] ? "Assigned" : "None";

                            std::string label =
                                "Element " + std::to_string(slot) +
                                "  [" + state + "]";

                            if (ImGui::Selectable(label.c_str(), selected))
                                m_SelectedMaterialSlot = slot;

                            // Every element is a real material drop target.
                            if (ImGui::BeginDragDropTarget())
                            {
                                if (const ImGuiPayload* payload =
                                    ImGui::AcceptDragDropPayload(
                                        "NOJOB_MATERIAL_ASSET"))
                                {
                                    const char* relativePath =
                                        static_cast<const char*>(payload->Data);

                                    AssetRegistry registry(
                                        AssetManager::GetAssetsDirectory());
                                    registry.Load();

                                    std::shared_ptr<Shader> shader;
                                    if (renderer.Materials[slot])
                                        shader =
                                        renderer.Materials[slot]->GetShader();
                                    else if (renderer.MaterialAsset)
                                        shader =
                                        renderer.MaterialAsset->GetShader();

                                    auto loaded = MaterialSerializer::Load(
                                        AssetManager::GetProjectRoot() /
                                        relativePath,
                                        shader,
                                        registry);

                                    if (loaded)
                                    {
                                        renderer.Materials[slot] = loaded;
                                        if (slot == 0)
                                            renderer.MaterialAsset = loaded;
                                    }
                                }
                                ImGui::EndDragDropTarget();
                            }

                            ImGui::PopID();
                        }

                        auto& activeMaterial =
                            renderer.Materials[m_SelectedMaterialSlot];

                        if (!activeMaterial)
                        {
                            ImGui::TextDisabled(
                                "Selected material slot is empty.");
                        }
                        else
                        {
                            // Keep the legacy slot-0 alias synchronized.
                            if (m_SelectedMaterialSlot == 0)
                                renderer.MaterialAsset = activeMaterial;

                            ImGui::SeparatorText(
                                ("Element " +
                                    std::to_string(m_SelectedMaterialSlot))
                                .c_str());

                            const char* surfaceModes[] =
                            {
                                "Opaque",
                                "Alpha Clip",
                                "Transparent"
                            };

                            int surfaceMode =
                                static_cast<int>(
                                    activeMaterial->SurfaceMode());

                            if (ImGui::Combo(
                                "Rendering Mode",
                                &surfaceMode,
                                surfaceModes,
                                3))
                            {
                                activeMaterial->SurfaceMode() =
                                    static_cast<MaterialSurfaceMode>(
                                        surfaceMode);
                            }

                            if (activeMaterial->SurfaceMode() ==
                                MaterialSurfaceMode::AlphaClip)
                            {
                                ImGui::SliderFloat(
                                    "Alpha Cutoff",
                                    &activeMaterial->AlphaCutoff(),
                                    0.0f,
                                    1.0f);
                            }

                            ImGui::TextUnformatted("Material");
                            ImGui::SameLine();
                            ImGui::Button(
                                "Material Slot",
                                ImVec2(
                                    ImGui::GetContentRegionAvail().x,
                                    0.0f));

                            if (ImGui::BeginDragDropTarget())
                            {
                                if (const ImGuiPayload* materialPayload =
                                    ImGui::AcceptDragDropPayload(
                                        "NOJOB_MATERIAL_ASSET"))
                                {
                                    const char* relativePath =
                                        static_cast<const char*>(
                                            materialPayload->Data);

                                    AssetRegistry registry(
                                        AssetManager::GetAssetsDirectory());
                                    registry.Load();

                                    auto loaded = MaterialSerializer::Load(
                                        AssetManager::GetProjectRoot() /
                                        relativePath,
                                        activeMaterial->GetShader(),
                                        registry);

                                    if (loaded)
                                    {
                                        activeMaterial = loaded;
                                        if (m_SelectedMaterialSlot == 0)
                                            renderer.MaterialAsset = loaded;
                                    }
                                }

                                if (const ImGuiPayload* texturePayload =
                                    ImGui::AcceptDragDropPayload(
                                        "NOJOB_TEXTURE_ASSET"))
                                {
                                    const char* relativePath =
                                        static_cast<const char*>(
                                            texturePayload->Data);
                                    try
                                    {
                                        activeMaterial->SetTexture(
                                            AssetManager::LoadTexture(
                                                relativePath));
                                        activeMaterial->UseTexture() = true;
                                    }
                                    catch (const std::exception&) {}
                                }

                                ImGui::EndDragDropTarget();
                            }

                            ImGui::Separator();
                            auto& color = activeMaterial->GetColor();
                            ImGui::ColorEdit4(
                                "Material Color", &color.x);

                            ImGui::SeparatorText("PBR Surface");
                            ImGui::SliderFloat(
                                "Metallic",
                                &activeMaterial->Metallic(), 0.0f, 1.0f);
                            ImGui::SliderFloat(
                                "Roughness",
                                &activeMaterial->Roughness(), 0.04f, 1.0f);
                            ImGui::SliderFloat(
                                "Ambient Occlusion",
                                &activeMaterial->AmbientOcclusion(),
                                0.0f, 1.0f);
                            ImGui::SliderFloat(
                                "Normal Strength",
                                &activeMaterial->NormalStrength(),
                                0.0f, 2.0f);
                            ImGui::ColorEdit3(
                                "Emissive Color",
                                &activeMaterial->EmissiveColor().x);
                            ImGui::SliderFloat(
                                "Emissive Strength",
                                &activeMaterial->EmissiveStrength(),
                                0.0f, 20.0f);

                            ImGui::SeparatorText("PBR Texture Maps");
                            auto selectPBRMap =
                                [&](const char* label, auto setter)
                                {
                                    if (ImGui::Button(label))
                                    {
                                        const std::string path =
                                            OpenInspectorTextureFileDialog();
                                        if (!path.empty())
                                        {
                                            try
                                            {
                                                const auto importedPath =
                                                    AssetManager::ImportTexture(
                                                        path);
                                                setter(
                                                    AssetManager::LoadTexture(
                                                        importedPath));
                                            }
                                            catch (const std::exception&) {}
                                        }
                                    }
                                };

                            selectPBRMap(
                                "Normal Map...",
                                [&](std::shared_ptr<Texture2D> v)
                                {
                                    activeMaterial->SetNormalTexture(
                                        std::move(v));
                                });
                            ImGui::SameLine();
                            selectPBRMap(
                                "Metallic Map...",
                                [&](std::shared_ptr<Texture2D> v)
                                {
                                    activeMaterial->SetMetallicTexture(
                                        std::move(v));
                                });
                            selectPBRMap(
                                "Roughness Map...",
                                [&](std::shared_ptr<Texture2D> v)
                                {
                                    activeMaterial->SetRoughnessTexture(
                                        std::move(v));
                                });
                            ImGui::SameLine();
                            selectPBRMap(
                                "AO Map...",
                                [&](std::shared_ptr<Texture2D> v)
                                {
                                    activeMaterial->SetAOTexture(
                                        std::move(v));
                                });
                            selectPBRMap(
                                "Emissive Map...",
                                [&](std::shared_ptr<Texture2D> v)
                                {
                                    activeMaterial->SetEmissiveTexture(
                                        std::move(v));
                                });

                            ImGui::Checkbox(
                                "Use Texture",
                                &activeMaterial->UseTexture());

                            if (ImGui::Button("Select Texture..."))
                            {
                                const std::string path =
                                    OpenInspectorTextureFileDialog();
                                if (!path.empty())
                                {
                                    try
                                    {
                                        const auto importedPath =
                                            AssetManager::ImportTexture(path);
                                        activeMaterial->SetTexture(
                                            AssetManager::LoadTexture(
                                                importedPath));
                                        activeMaterial->UseTexture() = true;
                                    }
                                    catch (const std::exception&) {}
                                }
                            }

                            ImGui::SameLine();
                            if (ImGui::Button("Checkerboard"))
                            {
                                activeMaterial->SetTexture(
                                    Texture2D::CreateCheckerboard());
                                activeMaterial->UseTexture() = true;
                            }

                            if (ImGui::Button("Save Material Asset"))
                            {
                                const auto& entityTag =
                                    m_SelectedEntity
                                    .GetComponent<TagComponent>().Tag;

                                const auto materialPath =
                                    AssetManager::GetAssetsDirectory() /
                                    "Materials" /
                                    (entityTag + "_Element" +
                                        std::to_string(
                                            m_SelectedMaterialSlot) +
                                        ".nojobmat");

                                AssetRegistry registry(
                                    AssetManager::GetAssetsDirectory());
                                registry.Load();
                                MaterialSerializer::Save(
                                    *activeMaterial,
                                    materialPath,
                                    registry);
                                registry.Register(
                                    materialPath,
                                    AssetType::Material);
                                registry.Save();
                            }

                            ImGui::SameLine();
                            if (ImGui::Button("Create Prefab"))
                            {
                                ProjectAssetOperations::CreatePrefab(
                                    m_SelectedEntity,
                                    AssetManager::GetAssetsDirectory() / "Prefabs",
                                    [this](const std::string& message) { LogInspectorMessage(message); });
                            }

                            if (activeMaterial->GetTexture())
                            {
                                const auto& texture =
                                    activeMaterial->GetTexture();
                                const std::filesystem::path texturePath(
                                    texture->GetPath());

                                const std::string displayName =
                                    texture->GetPath() == "Checkerboard"
                                    ? std::string("Checkerboard")
                                    : texturePath.filename().string();

                                ImGui::TextDisabled(
                                    "Texture: %s",
                                    displayName.c_str());

                                ImGui::Image(
                                    static_cast<ImTextureID>(
                                        static_cast<intptr_t>(
                                            texture->GetRendererID())),
                                    ImVec2(96.0f, 96.0f));
                            }
                        }
                    }
                    else
                    {
                        ImGui::TextDisabled("No material assigned.");
                    }
                }
            }

            ImGui::Separator();

            const auto id =
                m_SelectedEntity.GetComponent<IDComponent>().ID;

            ImGui::Text(
                "Entity ID: %llu",
                static_cast<unsigned long long>(id));
        }
        else
        {
            ImGui::TextDisabled(
                "Select an entity in Hierarchy.");
        }

        ImGui::End();
    }

    void EditorLayer::DrawComponentTools()
    {
        if (!m_SelectedEntity) return;

        ImGui::SeparatorText("Component Actions");
        static int componentIndex = 0;

        struct Entry { const char* Name; int Id; };
        std::vector<Entry> entries;
        entries.push_back({ "Transform", 0 });
        if (m_SelectedEntity.HasComponent<NativeScriptComponent>()) entries.push_back({ "Native Script",1 });
        if (m_SelectedEntity.HasComponent<RigidbodyComponent>()) entries.push_back({ "Rigidbody",2 });
        if (m_SelectedEntity.HasComponent<BoxColliderComponent>()) entries.push_back({ "Box Collider",3 });
        if (m_SelectedEntity.HasComponent<SphereColliderComponent>()) entries.push_back({ "Sphere Collider",4 });
        if (m_SelectedEntity.HasComponent<CapsuleColliderComponent>()) entries.push_back({ "Capsule Collider",5 });
        if (m_SelectedEntity.HasComponent<CameraComponent>()) entries.push_back({ "Camera",6 });
        if (m_SelectedEntity.HasComponent<DirectionalLightComponent>()) entries.push_back({ "Directional Light",7 });
        if (m_SelectedEntity.HasComponent<PointLightComponent>()) entries.push_back({ "Point Light",8 });
        if (m_SelectedEntity.HasComponent<SpotLightComponent>()) entries.push_back({ "Spot Light",9 });
        if (m_SelectedEntity.HasComponent<AnimatorComponent>()) entries.push_back({ "Animator",10 });
        if (m_SelectedEntity.HasComponent<NavAgentComponent>())
            entries.push_back({ "Nav Agent",11 });
        if (m_SelectedEntity.HasComponent<PerceptionComponent>())
            entries.push_back({ "Perception", 12 });
        componentIndex = std::clamp(componentIndex, 0, (int)entries.size() - 1);
        if (ImGui::BeginCombo("Component", entries[componentIndex].Name))
        {
            for (int i = 0;i < (int)entries.size();++i)
                if (ImGui::Selectable(entries[i].Name, i == componentIndex))
                    componentIndex = i;
            ImGui::EndCombo();
        }

        const int id = entries[componentIndex].Id;
        auto copy = [&] {
            switch (id) {
            case 0:m_ComponentClipboard = m_SelectedEntity.GetComponent<TransformComponent>();break;
            case 1:m_ComponentClipboard = m_SelectedEntity.GetComponent<NativeScriptComponent>();break;
            case 2:m_ComponentClipboard = m_SelectedEntity.GetComponent<RigidbodyComponent>();break;
            case 3:m_ComponentClipboard = m_SelectedEntity.GetComponent<BoxColliderComponent>();break;
            case 4:m_ComponentClipboard = m_SelectedEntity.GetComponent<SphereColliderComponent>();break;
            case 5:m_ComponentClipboard = m_SelectedEntity.GetComponent<CapsuleColliderComponent>();break;
            case 6:m_ComponentClipboard = m_SelectedEntity.GetComponent<CameraComponent>();break;
            case 7:m_ComponentClipboard = m_SelectedEntity.GetComponent<DirectionalLightComponent>();break;
            case 8:m_ComponentClipboard = m_SelectedEntity.GetComponent<PointLightComponent>();break;
            case 9:m_ComponentClipboard = m_SelectedEntity.GetComponent<SpotLightComponent>();break;
            case 10:m_ComponentClipboard = m_SelectedEntity.GetComponent<AnimatorComponent>();break;
            case 11:m_ComponentClipboard = m_SelectedEntity.GetComponent<NavAgentComponent>();break;
            case 12:m_ComponentClipboard = m_SelectedEntity.GetComponent<PerceptionComponent>();break;
            }
            };
        auto reset = [&] {
            CaptureUndoSnapshot();
            switch (id) {
            case 0:m_SelectedEntity.GetComponent<TransformComponent>() = {};break;
            case 1:m_SelectedEntity.GetComponent<NativeScriptComponent>() = {};break;
            case 2:m_SelectedEntity.GetComponent<RigidbodyComponent>() = {};break;
            case 3:m_SelectedEntity.GetComponent<BoxColliderComponent>() = {};break;
            case 4:m_SelectedEntity.GetComponent<SphereColliderComponent>() = {};break;
            case 5:m_SelectedEntity.GetComponent<CapsuleColliderComponent>() = {};break;
            case 6:m_SelectedEntity.GetComponent<CameraComponent>() = {};break;
            case 7:m_SelectedEntity.GetComponent<DirectionalLightComponent>() = {};break;
            case 8:m_SelectedEntity.GetComponent<PointLightComponent>() = {};break;
            case 9:m_SelectedEntity.GetComponent<SpotLightComponent>() = {};break;
            case 10:m_SelectedEntity.GetComponent<AnimatorComponent>() = {};break;
            case 11:m_SelectedEntity.GetComponent<NavAgentComponent>() = {};  break;
            case 12:m_SelectedEntity.GetComponent<PerceptionComponent>() = {};break;
            }
            };
        auto paste = [&] {
            CaptureUndoSnapshot();
            switch (id) {
            case 0:if (auto p = std::get_if<TransformComponent>(&m_ComponentClipboard))m_SelectedEntity.GetComponent<TransformComponent>() = *p;break;
            case 1:if (auto p = std::get_if<NativeScriptComponent>(&m_ComponentClipboard))m_SelectedEntity.GetComponent<NativeScriptComponent>() = *p;break;
            case 2:if (auto p = std::get_if<RigidbodyComponent>(&m_ComponentClipboard))m_SelectedEntity.GetComponent<RigidbodyComponent>() = *p;break;
            case 3:if (auto p = std::get_if<BoxColliderComponent>(&m_ComponentClipboard))m_SelectedEntity.GetComponent<BoxColliderComponent>() = *p;break;
            case 4:if (auto p = std::get_if<SphereColliderComponent>(&m_ComponentClipboard))m_SelectedEntity.GetComponent<SphereColliderComponent>() = *p;break;
            case 5:if (auto p = std::get_if<CapsuleColliderComponent>(&m_ComponentClipboard))m_SelectedEntity.GetComponent<CapsuleColliderComponent>() = *p;break;
            case 6:if (auto p = std::get_if<CameraComponent>(&m_ComponentClipboard))m_SelectedEntity.GetComponent<CameraComponent>() = *p;break;
            case 7:if (auto p = std::get_if<DirectionalLightComponent>(&m_ComponentClipboard))m_SelectedEntity.GetComponent<DirectionalLightComponent>() = *p;break;
            case 8:if (auto p = std::get_if<PointLightComponent>(&m_ComponentClipboard))m_SelectedEntity.GetComponent<PointLightComponent>() = *p;break;
            case 9:if (auto p = std::get_if<SpotLightComponent>(&m_ComponentClipboard))m_SelectedEntity.GetComponent<SpotLightComponent>() = *p;break;
            case 10:if (auto p = std::get_if<AnimatorComponent>(&m_ComponentClipboard))m_SelectedEntity.GetComponent<AnimatorComponent>() = *p;break;
            case 11: if (auto p = std::get_if<NavAgentComponent>(&m_ComponentClipboard)) { m_SelectedEntity.GetComponent<NavAgentComponent>() = *p; } break;
            case 12:if (auto p = std::get_if<PerceptionComponent>(&m_ComponentClipboard))m_SelectedEntity.GetComponent<PerceptionComponent>() = *p;break;
            }
            };

        if (ImGui::SmallButton("Reset")) reset();
        ImGui::SameLine();
        if (ImGui::SmallButton("Copy")) copy();
        ImGui::SameLine();
        if (ImGui::SmallButton("Paste")) paste();
        ImGui::SameLine();

        const bool removable = id != 0;
        if (!removable) ImGui::BeginDisabled();
        if (ImGui::SmallButton("Remove") && removable)
        {
            CaptureUndoSnapshot();
            switch (id) {
            case 1:m_SelectedEntity.RemoveComponent<NativeScriptComponent>();break;
            case 2:m_SelectedEntity.RemoveComponent<RigidbodyComponent>();break;
            case 3:m_SelectedEntity.RemoveComponent<BoxColliderComponent>();break;
            case 4:m_SelectedEntity.RemoveComponent<SphereColliderComponent>();break;
            case 5:m_SelectedEntity.RemoveComponent<CapsuleColliderComponent>();break;
            case 6:m_SelectedEntity.RemoveComponent<CameraComponent>();break;
            case 7:m_SelectedEntity.RemoveComponent<DirectionalLightComponent>();break;
            case 8:m_SelectedEntity.RemoveComponent<PointLightComponent>();break;
            case 9:m_SelectedEntity.RemoveComponent<SpotLightComponent>();break;
            case 10:m_SelectedEntity.RemoveComponent<AnimatorComponent>();break;
            case 11:m_SelectedEntity.RemoveComponent<NavAgentComponent>();break;
            case 12:m_SelectedEntity.RemoveComponent<PerceptionComponent>();break;
            }
            componentIndex = 0;
        }
        if (!removable) ImGui::EndDisabled();
        ImGui::TextDisabled("F2 Rename | Ctrl+D Duplicate | Delete | Ctrl+Z/Y Undo/Redo");
    }
}
