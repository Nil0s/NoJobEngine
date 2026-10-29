/** @file DefaultScene.h @brief Shared empty-scene template for startup and File > New Scene. */
#pragma once
#include "Engine/Scene/Scene.h"
#include <glm/gtc/constants.hpp>
namespace NoJob {
/** @brief Replace scene contents with a primary camera and two lights, without geometry or scripts. */
inline void CreateDefaultEditorScene(Scene& scene) {
    Scene empty;
    scene.RestoreFrom(empty);
    auto camera = scene.CreateEntity("Main Camera");
    auto& transform = camera.GetComponent<TransformComponent>();
    transform.Position = {0.0f, 2.5f, 7.0f};
    transform.Rotation = {glm::radians(-15.0f), 0.0f, 0.0f};
    camera.AddComponent<CameraComponent>();
    auto sun = scene.CreateEntity("Directional Light");
    sun.GetComponent<TransformComponent>().Rotation = {glm::radians(-50.0f), glm::radians(-30.0f), 0.0f};
    sun.AddComponent<DirectionalLightComponent>().Intensity = 1.1f;
    auto fill = scene.CreateEntity("Point Light");
    fill.GetComponent<TransformComponent>().Position = {2.5f, 2.5f, 2.0f};
    auto& light = fill.AddComponent<PointLightComponent>();
    light.Intensity = 0.75f;
    light.Range = 8.0f;
}
}
