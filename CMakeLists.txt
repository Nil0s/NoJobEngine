cmake_minimum_required(VERSION 3.25)
project(NoJobEngine VERSION 2.1.0 LANGUAGES C CXX)

# Keep Ninja header dependency parsing independent of the console code page.
# Detect the installed compiler language; English resources are not always installed.
if(MSVC AND CMAKE_GENERATOR MATCHES "Ninja")
    set(_nojob_msvc_launcher cmd /d /c call "${CMAKE_CURRENT_SOURCE_DIR}/Tools/Build/MSVCCompile.cmd")
    set(_nojob_include_probe "${CMAKE_BINARY_DIR}/CMakeFiles/NoJobShowIncludes")
    file(MAKE_DIRECTORY "${_nojob_include_probe}")
    file(WRITE "${_nojob_include_probe}/probe.h" "\n")
    file(WRITE "${_nojob_include_probe}/probe.c" "#include \"probe.h\"\nint nojob_probe(void) { return 0; }\n")
    execute_process(
        COMMAND ${_nojob_msvc_launcher} "${CMAKE_CXX_COMPILER}"
            /nologo /showIncludes /Z7 /c probe.c /Foprobe.obj
        WORKING_DIRECTORY "${_nojob_include_probe}"
        OUTPUT_VARIABLE _nojob_include_output
        ERROR_VARIABLE _nojob_include_error
        RESULT_VARIABLE _nojob_include_result
        ENCODING NONE)
    if(NOT _nojob_include_result EQUAL 0 OR NOT _nojob_include_output MATCHES "(^|\n)([^:\n]+:[^:\n]+: +)([A-Za-z]:|/)")
        message(FATAL_ERROR "Cannot detect MSVC header dependency prefix: ${_nojob_include_output} ${_nojob_include_error}")
    endif()
    set(CMAKE_CL_SHOWINCLUDES_PREFIX "${CMAKE_MATCH_2}")
    list(PREPEND CMAKE_C_COMPILER_LAUNCHER ${_nojob_msvc_launcher})
    list(PREPEND CMAKE_CXX_COMPILER_LAUNCHER ${_nojob_msvc_launcher})
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)

FetchContent_Declare(glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.4
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(glfw)

FetchContent_Declare(glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 1.0.1
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(glm)

# Assimp - production static-model importer (FBX/glTF/GLB/OBJ/DAE/STL/PLY...)
set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ASSIMP_INSTALL OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "" FORCE)
set(ASSIMP_WARNINGS_AS_ERRORS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(assimp
    GIT_REPOSITORY https://github.com/assimp/assimp.git
    GIT_TAG v5.4.3
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(assimp)

FetchContent_Declare(glad
    GIT_REPOSITORY https://github.com/Dav1dde/glad.git
    GIT_TAG v2.0.8
    GIT_SHALLOW TRUE)

FetchContent_GetProperties(glad)
if(NOT glad_POPULATED)
    FetchContent_Populate(glad)
endif()

add_subdirectory(
    "${glad_SOURCE_DIR}/cmake"
    "${glad_BINARY_DIR}/cmake"
)

glad_add_library(NoJobGLAD STATIC REPRODUCIBLE API gl:core=4.6)

# Dear ImGui docking branch.

FetchContent_Declare(stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    # Pinned for reproducible builds and license traceability.
    GIT_TAG 2c980bb59875b0d32144a71867fbdebb2f77cd20)
FetchContent_MakeAvailable(stb)

# miniaudio 0.11.25 - low-level audio backend used behind NoJobEngine's AudioEngine.
# Pinned release for reproducible builds.
FetchContent_Declare(miniaudio
    GIT_REPOSITORY https://github.com/mackron/miniaudio.git
    GIT_TAG 0.11.25
    GIT_SHALLOW TRUE)
FetchContent_GetProperties(miniaudio)
if(NOT miniaudio_POPULATED)
    FetchContent_Populate(miniaudio)
endif()

FetchContent_Declare(imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.92.5-docking
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(imgui)

FetchContent_Declare(imguizmo
    GIT_REPOSITORY https://github.com/CedricGuillemet/ImGuizmo.git
    # Pinned for reproducible builds and license traceability.
    GIT_TAG 18cef5e031d8c6973d80284c67f60549fafd78c1)

FetchContent_GetProperties(imguizmo)
if(NOT imguizmo_POPULATED)
    FetchContent_Populate(imguizmo)
endif()


# Jolt Physics 5.6.0 - runtime rigid body physics.
# Disable optional tools/compute backends: NoJobEngine only needs the core library.
set(JPH_MASTER_PROJECT OFF CACHE BOOL "" FORCE)
set(JPH_SAMPLES OFF CACHE BOOL "" FORCE)
set(JPH_TESTS OFF CACHE BOOL "" FORCE)
set(JPH_INSTALL OFF CACHE BOOL "" FORCE)
set(JPH_USE_DX12 OFF CACHE BOOL "" FORCE)
set(JPH_USE_VK OFF CACHE BOOL "" FORCE)
set(JPH_USE_MTL OFF CACHE BOOL "" FORCE)
set(JPH_USE_CPU_COMPUTE OFF CACHE BOOL "" FORCE)
set(OVERRIDE_CXX_FLAGS OFF CACHE BOOL "" FORCE)
# NoJobEngine uses the DLL MSVC runtime (/MD, /MDd). Jolt defaults to the
# static runtime (/MT, /MTd), which causes LNK2038/LNK2005 when linked
# into NoJobEditor. Force Jolt to use the same runtime as the engine.
set(USE_STATIC_MSVC_RUNTIME_LIBRARY OFF CACHE BOOL "" FORCE)
set(CPP_EXCEPTIONS_ENABLED ON CACHE BOOL "" FORCE)

FetchContent_Declare(JoltPhysics
    GIT_REPOSITORY https://github.com/jrouwe/JoltPhysics.git
    GIT_TAG v5.6.0
    GIT_SHALLOW TRUE
    SOURCE_SUBDIR Build)
FetchContent_MakeAvailable(JoltPhysics)

add_library(NoJobImGui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
    ${imguizmo_SOURCE_DIR}/src/ImGuizmo.cpp)

target_include_directories(NoJobImGui
    PUBLIC
        ${imgui_SOURCE_DIR}
        ${imgui_SOURCE_DIR}/backends
        ${imguizmo_SOURCE_DIR}/src
)

target_compile_definitions(NoJobImGui PRIVATE IMGUI_DEFINE_MATH_OPERATORS)

target_link_libraries(NoJobImGui PUBLIC glfw)

# Automatically discover all engine implementation files.
# CONFIGURE_DEPENDS makes CMake re-check the Engine tree when files are added or removed.
file(GLOB_RECURSE NOJOB_ENGINE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/*.c"
)

# Test translation units define their own main() and must not be part of the engine library.
list(FILTER NOJOB_ENGINE_SOURCES EXCLUDE REGEX "[/\\]Tests[/\\].*\\.cpp$")
list(FILTER NOJOB_ENGINE_SOURCES EXCLUDE REGEX "[/\\]NeuralTests\\.cpp$")

# Standalone RL tests and learning tools define main(); exclude them from the engine archive.
list(REMOVE_ITEM NOJOB_ENGINE_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/AI/Reinforcement/ReinforcementTests.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/AI/Reinforcement/TrackingLearningProof.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/AI/Reinforcement/TrackingEvaluationTests.cpp")

# SceneSerializer must be compiled into NoJobEngine for both Editor and Runtime.
set(NOJOB_SCENE_SERIALIZER "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Scene/SceneSerializer.cpp")
if(NOT EXISTS "${NOJOB_SCENE_SERIALIZER}")
    message(FATAL_ERROR "Missing Engine/Scene/SceneSerializer.cpp; restore it from the previous working Engine ZIP")
endif()
list(FIND NOJOB_ENGINE_SOURCES "${NOJOB_SCENE_SERIALIZER}" NOJOB_SERIALIZER_INDEX)
if(NOJOB_SERIALIZER_INDEX EQUAL -1)
    list(APPEND NOJOB_ENGINE_SOURCES "${NOJOB_SCENE_SERIALIZER}")
endif()

add_library(NoJobEngine STATIC
    ${NOJOB_ENGINE_SOURCES}
    ${miniaudio_SOURCE_DIR}/miniaudio.c
)

target_include_directories(NoJobEngine
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE
        ${stb_SOURCE_DIR}
        ${miniaudio_SOURCE_DIR})

target_compile_definitions(NoJobEngine PUBLIC
    GLFW_INCLUDE_NONE)

target_link_libraries(NoJobEngine PUBLIC
    glfw
    glm::glm
    NoJobGLAD
    Jolt::Jolt
    assimp::assimp)

# Project-native gameplay scripts are compiled into a reloadable DLL.
# The editor stays alive while this target is rebuilt.
file(GLOB_RECURSE NOJOB_PROJECT_SCRIPTS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/Assets/Scripts/*.cpp")

# Keep the target valid before the first user script exists.
set(NOJOB_PROJECT_SCRIPT_STUB
    "${CMAKE_CURRENT_BINARY_DIR}/NoJobProjectScriptsStub.cpp")
file(WRITE "${NOJOB_PROJECT_SCRIPT_STUB}"
    "extern \"C\" __declspec(dllexport) int NoJobProjectScriptsStub(){return 1;}\n")

add_library(NoJobProjectScripts SHARED
    ${NOJOB_PROJECT_SCRIPT_STUB}
    ${NOJOB_PROJECT_SCRIPTS})
target_include_directories(NoJobProjectScripts PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(NoJobProjectScripts PRIVATE NoJobEngine)
set_target_properties(NoJobProjectScripts PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/ProjectScripts"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/ProjectScripts"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/ProjectScripts")

# V1.4 native hot reload:
# version only the final linker artifacts. Object files keep stable paths so
# MSBuild can reuse every unchanged script .obj between hot reload generations.
set(NOJOB_HOT_RELOAD_GENERATION "0" CACHE STRING
    "Generation number for NoJobProjectScripts hot reload")

set_target_properties(NoJobProjectScripts PROPERTIES
    OUTPUT_NAME "NoJobProjectScripts_${NOJOB_HOT_RELOAD_GENERATION}"
    PDB_NAME "NoJobProjectScripts_${NOJOB_HOT_RELOAD_GENERATION}")

add_executable(NoJobEditor
    Editor/main.cpp
    Editor/EditorLayer.cpp
    Editor/Build/StandaloneBuilder.cpp
    Editor/Build/BuildSettings.cpp
    Editor/Scripting/VisualStudioIntegration.cpp
    Editor/Scripting/ProjectScriptManager.cpp
    Editor/Scripting/ProjectScriptBuildSystem.cpp
    Editor/Assets/ProjectAssetOperations.cpp
    Editor/Assets/ProjectPanel.cpp
    Editor/Panels/InspectorPanel.cpp
    Editor/Scene/SceneFileDialog.cpp
    Editor/EditorCamera.cpp
    Editor/Panels/NeuralEditorPanel.cpp
    Editor/Panels/ReinforcementPanel.cpp)

target_link_libraries(NoJobEditor PRIVATE
    NoJobEngine
    NoJobImGui)

if(MSVC)
    target_compile_options(NoJobEngine PRIVATE /W4 /permissive-)
    target_compile_options(NoJobEditor PRIVATE /W4 /permissive-)
    target_compile_options(NoJobProjectScripts PRIVATE /W4 /permissive-)

    set_property(
        DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        PROPERTY VS_STARTUP_PROJECT NoJobEditor)
endif()

set_target_properties(NoJobEditor PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

# V1.7 Standalone Build foundation: runtime-only executable.
# This target contains no Dear ImGui/editor code and boots the project's Start Scene.
add_executable(NoJobRuntime
    Runtime/main.cpp)
target_link_libraries(NoJobRuntime PRIVATE NoJobEngine)
set_target_properties(NoJobRuntime PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
if(MSVC)
    target_compile_options(NoJobRuntime PRIVATE /W4 /permissive-)
endif()
add_executable(BTSequenceTests
    Engine/AI/BehaviorTree/Tests/BTSequenceTests.cpp
)

target_include_directories(BTSequenceTests PRIVATE
    ${CMAKE_SOURCE_DIR}
)

target_compile_features(BTSequenceTests PRIVATE cxx_std_20)
add_executable(BTSelectorTests
    Engine/AI/BehaviorTree/Tests/BTSelectorTests.cpp
)

target_include_directories(BTSelectorTests PRIVATE
    ${CMAKE_SOURCE_DIR}
)

target_compile_features(BTSelectorTests PRIVATE cxx_std_20)
add_executable(BTBlackboardTests
    Engine/AI/BehaviorTree/Tests/BTBlackboardTests.cpp
)

target_compile_features(BTBlackboardTests PRIVATE cxx_std_20)

target_include_directories(BTBlackboardTests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)
add_executable(BTNativeTests
    Engine/AI/BehaviorTree/Tests/BTNativeTests.cpp
)

target_compile_features(BTNativeTests PRIVATE cxx_std_20)

target_include_directories(BTNativeTests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)
add_executable(BTAssetTests Engine/AI/BehaviorTree/Tests/BTAssetTests.cpp)
target_include_directories(BTAssetTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(BTAssetTests PRIVATE cxx_std_20)

add_executable(BTReactiveTests Engine/AI/BehaviorTree/Tests/BTReactiveTests.cpp)
target_include_directories(BTReactiveTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(BTReactiveTests PRIVATE cxx_std_20)

# Standalone neural tests; no graphics/physics/editor dependencies.
option(NOJOB_BUILD_NEURAL_TESTS "Build native neural network tests" OFF)
if(NOJOB_BUILD_NEURAL_TESTS)
    add_executable(NoJobNeuralTests
        Engine/AI/Neural/NeuralTests.cpp
        Engine/AI/Neural/NeuralNetwork.cpp)
    target_include_directories(NoJobNeuralTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/AI/Neural)
    enable_testing()
    add_test(NAME NoJobNeuralTests COMMAND NoJobNeuralTests)
endif()

# V2.1: isolated native reinforcement-learning tests (no editor or graphics).
option(NOJOB_BUILD_RL_TESTS "Build native reinforcement learning tests" OFF)
if(NOJOB_BUILD_RL_TESTS)
    add_executable(NoJobRLTests
        Engine/AI/Reinforcement/ReinforcementTests.cpp
        Engine/AI/Neural/NeuralNetwork.cpp)
    target_include_directories(NoJobRLTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_compile_features(NoJobRLTests PRIVATE cxx_std_20)
    enable_testing()
    add_test(NAME NoJobRLTests COMMAND NoJobRLTests)
endif()

# Headless bake/navigation regression; uses the unmodified testplane OBJ fixture.
option(NOJOB_BUILD_NAV_TRACKING_TESTS "Build baked navigation tracking regression" OFF)
if(NOJOB_BUILD_NAV_TRACKING_TESTS)
    add_executable(NoJobNavTrackingTests Engine/AI/Navigation/Tests/NavTrackingTests.cpp)
    target_link_libraries(NoJobNavTrackingTests PRIVATE NoJobEngine)
    enable_testing()
    add_test(NAME NoJobNavTrackingTests COMMAND NoJobNavTrackingTests)
    set_tests_properties(NoJobNavTrackingTests PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
endif()

option(NOJOB_BUILD_OBSTACLE_TESTS "Build learned obstacle avoidance tests and proof" OFF)
if(NOJOB_BUILD_OBSTACLE_TESTS)
    add_executable(NoJobObstacleTests Engine/AI/Reinforcement/Tests/ObstacleLearningTests.cpp Assets/Scripts/RLObstacleAgent.cpp Assets/Scripts/RLTargetPatrol.cpp)
    target_link_libraries(NoJobObstacleTests PRIVATE NoJobEngine)
    add_executable(NoJobObstacleLearningProof Engine/AI/Reinforcement/Tests/ObstacleLearningProof.cpp Engine/AI/Neural/NeuralNetwork.cpp)
    target_include_directories(NoJobObstacleLearningProof PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    target_compile_features(NoJobObstacleLearningProof PRIVATE cxx_std_20)
    enable_testing()
    add_test(NAME NoJobObstacleTests COMMAND NoJobObstacleTests "${CMAKE_CURRENT_BINARY_DIR}/obstacle_evaluation.csv")
    set_tests_properties(NoJobObstacleTests PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
endif()

# Standalone native trainer: no scene, renderer or editor runtime dependency.
option(NOJOB_BUILD_TRAINER "Build the independent visible RL trainer" ON)
if(NOJOB_BUILD_TRAINER)
    add_executable(NoJobTrainer WIN32 Tools/Trainer/main.cpp Engine/AI/Neural/NeuralNetwork.cpp)
    target_include_directories(NoJobTrainer PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    target_compile_features(NoJobTrainer PRIVATE cxx_std_20)
    target_compile_definitions(NoJobTrainer PRIVATE GLFW_INCLUDE_NONE)
    target_link_libraries(NoJobTrainer PRIVATE NoJobImGui NoJobGLAD)
    if(WIN32)
        target_link_libraries(NoJobTrainer PRIVATE shell32)
    endif()
    enable_testing()
    add_test(NAME NoJobTrainerSelfTest COMMAND NoJobTrainer --self-test --output-root "${CMAKE_CURRENT_BINARY_DIR}/trainer-self-test")
endif()

# Public Alpha branding is embedded in every user-facing executable.
option(NOJOB_DISTRIBUTION "Build portable GUI distribution executables" OFF)
if(WIN32)
    enable_language(RC)
    foreach(app NoJobEditor NoJobRuntime NoJobTrainer)
        if(TARGET ${app})
            target_sources(${app} PRIVATE Resources/Application.rc)
            target_include_directories(${app} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/Resources")
        endif()
    endforeach()
    if(NOJOB_DISTRIBUTION)
        set_target_properties(NoJobEditor PROPERTIES OUTPUT_NAME NJE)
        set_target_properties(NoJobRuntime PROPERTIES OUTPUT_NAME NJERuntime)
        if(TARGET NoJobTrainer)
            set_target_properties(NoJobTrainer PROPERTIES OUTPUT_NAME NJETrainer)
        endif()
    endif()
    if(NOJOB_DISTRIBUTION AND MSVC)
        foreach(app NoJobEditor NoJobRuntime)
            set_target_properties(${app} PROPERTIES WIN32_EXECUTABLE TRUE)
            target_link_options(${app} PRIVATE /ENTRY:mainCRTStartup)
        endforeach()
    endif()
endif()
option(NOJOB_BUILD_PATH_TESTS "Build portable project discovery regression tests" OFF)
if(NOJOB_BUILD_PATH_TESTS)
    add_executable(NoJobProjectPathTests Engine/Core/Tests/ProjectPathTests.cpp)
    target_include_directories(NoJobProjectPathTests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    enable_testing()
    add_test(NAME NoJobProjectPathTests COMMAND NoJobProjectPathTests "${CMAKE_CURRENT_BINARY_DIR}/path-test")
endif()

# Header-only neural/BT tools also use native executable-relative project discovery.
foreach(app BTSequenceTests BTSelectorTests BTBlackboardTests BTNativeTests BTAssetTests BTReactiveTests NoJobNeuralTests NoJobRLTests NoJobObstacleLearningProof NoJobProjectPathTests NoJobTrainer)
    if(TARGET ${app})
        target_sources(${app} PRIVATE Engine/Core/ProjectPaths.cpp)
    endif()
endforeach()

# WinSparkle is optional for development; installer builds use the pinned x64 SDK.
target_sources(NoJobEditor PRIVATE Editor/Distribution/UpdateService.cpp)
set(NJE_WINSPARKLE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/out/tooling/WinSparkle-0.9.4" CACHE PATH "WinSparkle 0.9.4 SDK directory")
if(WIN32 AND EXISTS "${NJE_WINSPARKLE_ROOT}/include/winsparkle.h")
    target_compile_definitions(NoJobEditor PRIVATE NJE_WITH_WINSPARKLE=1)
    target_include_directories(NoJobEditor PRIVATE "${NJE_WINSPARKLE_ROOT}/include")
    target_link_directories(NoJobEditor PRIVATE "${NJE_WINSPARKLE_ROOT}/x64/Release")
    target_link_libraries(NoJobEditor PRIVATE "${NJE_WINSPARKLE_ROOT}/x64/Release/WinSparkle.lib")
    add_custom_command(TARGET NoJobEditor POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${NJE_WINSPARKLE_ROOT}/x64/Release/WinSparkle.dll" "$<TARGET_FILE_DIR:NoJobEditor>/WinSparkle.dll")
endif()
