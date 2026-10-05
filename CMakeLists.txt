cmake_minimum_required(VERSION 3.25)
project(NoJobEngine VERSION 0.3.0 LANGUAGES C CXX)

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

add_library(NoJobEngine STATIC
    Engine/Core/Window.cpp
    Engine/Asset/AssetManager.cpp
    Engine/Physics/PhysicsSystem.cpp
    Engine/Scene/Scene.cpp
    Engine/Scene/SceneRenderer.cpp

    Engine/Renderer/Renderer.cpp
    Engine/Renderer/RendererAPI.cpp
    Engine/Renderer/RenderCommand.cpp
    Engine/Renderer/Buffer.cpp
    Engine/Renderer/VertexArray.cpp
    Engine/Renderer/Shader.cpp
    Engine/Renderer/Framebuffer.cpp
    Engine/Renderer/Mesh.cpp
    Engine/Renderer/Material.cpp
    Engine/Renderer/Texture.cpp
    Engine/Renderer/StbImage.cpp

    Engine/Platform/OpenGL/OpenGLContext.cpp
    Engine/Platform/OpenGL/OpenGLRendererAPI.cpp
    Engine/Platform/OpenGL/OpenGLBuffer.cpp
    Engine/Platform/OpenGL/OpenGLVertexArray.cpp
    Engine/Platform/OpenGL/OpenGLShader.cpp
    Engine/Platform/OpenGL/OpenGLFramebuffer.cpp
    Engine/Platform/OpenGL/OpenGLTexture.cpp
)

target_include_directories(NoJobEngine
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE
        ${stb_SOURCE_DIR})

target_compile_definitions(NoJobEngine PUBLIC
    GLFW_INCLUDE_NONE)

target_link_libraries(NoJobEngine PUBLIC
    glfw
    glm::glm
    NoJobGLAD
    Jolt::Jolt)

add_executable(NoJobEditor
    Editor/main.cpp
    Editor/EditorLayer.cpp
    Editor/EditorCamera.cpp)

target_link_libraries(NoJobEditor PRIVATE
    NoJobEngine
    NoJobImGui)

if(MSVC)
    target_compile_options(NoJobEngine PRIVATE /W4 /permissive-)
    target_compile_options(NoJobEditor PRIVATE /W4 /permissive-)

    set_property(
        DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        PROPERTY VS_STARTUP_PROJECT NoJobEditor)
endif()

set_target_properties(NoJobEditor PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
