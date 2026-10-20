#pragma once
#include <cstdint>
#include <memory>
#include <string>

namespace NoJob
{
    class Texture2D
    {
    public:
        virtual ~Texture2D() = default;

        virtual void Bind(std::uint32_t slot = 0) const = 0;
        virtual std::uint32_t GetRendererID() const = 0;
        virtual const std::string& GetPath() const = 0;

        static std::shared_ptr<Texture2D> CreateCheckerboard(
            std::uint32_t width = 256,
            std::uint32_t height = 256);

        static std::shared_ptr<Texture2D> Create(
            const std::string& path);
    };
}
