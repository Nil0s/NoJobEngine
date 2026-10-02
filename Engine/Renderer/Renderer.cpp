#include "Engine/Renderer/Renderer.h"
#include "Engine/Renderer/RenderCommand.h"
#include "Engine/Renderer/Shader.h"
#include "Engine/Renderer/VertexArray.h"

namespace NoJob
{
    void Renderer::Init()
    {
        RenderCommand::Init();
    }

    void Renderer::Shutdown()
    {
    }

    void Renderer::BeginFrame()
    {
        RenderCommand::SetViewport(0, 0, 1600, 900);
        RenderCommand::SetClearColor(0.08f, 0.09f, 0.11f, 1.0f);
        RenderCommand::Clear();
    }

    void Renderer::EndFrame()
    {
    }

    void Renderer::Submit(
        const std::shared_ptr<VertexArray>& vertexArray,
        const std::shared_ptr<Shader>& shader)
    {
        shader->Bind();
        RenderCommand::DrawIndexed(*vertexArray);
    }
}
