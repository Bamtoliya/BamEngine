#pragma once

#include <cstdint>
#include <filesystem>
#include <vector>
#include <glm/glm.hpp>

namespace Tools::AssetWorker
{
    struct RasterVertex
    {
        glm::vec3 Position{ 0.0f };
        glm::vec3 Normal{ 0.0f, 1.0f, 0.0f };
        glm::vec2 TexCoord{ 0.0f };
        glm::vec4 Color{ 1.0f };
    };

    struct RasterBatch
    {
        std::vector<RasterVertex> Vertices;
        std::vector<uint32_t> Indices;
        glm::mat4 WorldMatrix{ 1.0f };
        glm::vec4 BaseColor{ 0.78f, 0.80f, 0.84f, 1.0f };
        float Metallic = 0.2f;
        float Roughness = 0.45f;
        std::vector<uint8_t> AlbedoPixels;
        int AlbedoWidth = 0;
        int AlbedoHeight = 0;
        bool DoubleSided = false;
    };

    class HeadlessThumbnailRasterizer
    {
    public:
        static bool BakeThumbnail(const std::filesystem::path& sourcePath, const std::filesystem::path& outputPngPath, int32_t resolution = 128);
    };
}
