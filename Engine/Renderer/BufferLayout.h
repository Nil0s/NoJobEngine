#pragma once

#include <cstdint>
#include <initializer_list>
#include <string>
#include <vector>

namespace NoJob
{
    enum class ShaderDataType
    {
        None = 0,
        Float,
        Float2,
        Float3,
        Float4
    };

    inline std::uint32_t ShaderDataTypeSize(ShaderDataType type)
    {
        switch (type)
        {
            case ShaderDataType::Float:  return 4;
            case ShaderDataType::Float2: return 4 * 2;
            case ShaderDataType::Float3: return 4 * 3;
            case ShaderDataType::Float4: return 4 * 4;
            default:                     return 0;
        }
    }

    struct BufferElement
    {
        std::string Name;
        ShaderDataType Type = ShaderDataType::None;
        std::uint32_t Size = 0;
        std::uint32_t Offset = 0;
        bool Normalized = false;

        BufferElement() = default;

        BufferElement(
            ShaderDataType type,
            std::string name,
            bool normalized = false)
            : Name(std::move(name)),
              Type(type),
              Size(ShaderDataTypeSize(type)),
              Normalized(normalized)
        {
        }

        std::uint32_t GetComponentCount() const
        {
            switch (Type)
            {
                case ShaderDataType::Float:  return 1;
                case ShaderDataType::Float2: return 2;
                case ShaderDataType::Float3: return 3;
                case ShaderDataType::Float4: return 4;
                default:                     return 0;
            }
        }
    };

    class BufferLayout
    {
    public:
        BufferLayout() = default;

        BufferLayout(std::initializer_list<BufferElement> elements)
            : m_Elements(elements)
        {
            CalculateOffsetsAndStride();
        }

        const std::vector<BufferElement>& GetElements() const
        {
            return m_Elements;
        }

        std::uint32_t GetStride() const
        {
            return m_Stride;
        }

    private:
        void CalculateOffsetsAndStride()
        {
            std::uint32_t offset = 0;
            m_Stride = 0;

            for (auto& element : m_Elements)
            {
                element.Offset = offset;
                offset += element.Size;
                m_Stride += element.Size;
            }
        }

        std::vector<BufferElement> m_Elements;
        std::uint32_t m_Stride = 0;
    };
}
