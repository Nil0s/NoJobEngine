cmake_minimum_required(VERSION 3.25)
project(NoJobEngine VERSION 1.0.0 LANGUAGES C CXX)

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
    GIT_TAG master
    GIT_SHALLOW TRUE)
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
    GIT_TAG master
    GIT_SHALLOW TRUE)

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
    Editor/EditorCamera.cpp)

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
