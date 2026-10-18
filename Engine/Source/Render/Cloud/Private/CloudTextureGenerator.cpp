#include "Render/Cloud/Public/CloudTextureGenerator.h"
#include <vector>
#include <cmath>
#include <algorithm>

namespace Engine
{
    static float Hash3D(int x, int y, int z)
    {
        int n = x * 374761393 + y * 668265263 + z * 3266489917;
        n = (n ^ (n >> 13)) * 1274126177;
        return static_cast<float>(n & 0x7fffffff) / static_cast<float>(0x7fffffff);
    }

    static float Worley3D(float x, float y, float z, int cellCount)
    {
        x *= cellCount; y *= cellCount; z *= cellCount;
        int ix = static_cast<int>(std::floor(x));
        int iy = static_cast<int>(std::floor(y));
        int iz = static_cast<int>(std::floor(z));
        float fx = x - ix; float fy = y - iy; float fz = z - iz;

        float minDist = 1.0f;
        for (int dz = -1; dz <= 1; ++dz)
        {
            for (int dy = -1; dy <= 1; ++dy)
            {
                for (int dx = -1; dx <= 1; ++dx)
                {
                    int cx = (ix + dx + cellCount * 10) % cellCount;
                    int cy = (iy + dy + cellCount * 10) % cellCount;
                    int cz = (iz + dz + cellCount * 10) % cellCount;

                    float px = Hash3D(cx, cy, cz);
                    float py = Hash3D(cx + 17, cy + 31, cz + 47);
                    float pz = Hash3D(cx + 53, cy + 71, cz + 89);

                    float diffX = (dx + px) - fx;
                    float diffY = (dy + py) - fy;
                    float diffZ = (dz + pz) - fz;
                    float dist = std::sqrt(diffX * diffX + diffY * diffY + diffZ * diffZ);
                    minDist = std::min(minDist, dist);
                }
            }
        }
        return 1.0f - std::min(minDist, 1.0f);
    }

    RHITexture* CloudTextureGenerator::CreateBaseShapeTexture(RHI* rhi, uint32 size)
    {
        if (!rhi) return nullptr;
        std::vector<uint8_t> pixels(size * size * size * 4);

        for (uint32 z = 0; z < size; ++z)
        {
            float fz = static_cast<float>(z) / size;
            for (uint32 y = 0; y < size; ++y)
            {
                float fy = static_cast<float>(y) / size;
                for (uint32 x = 0; x < size; ++x)
                {
                    float fx = static_cast<float>(x) / size;

                    float w0 = Worley3D(fx, fy, fz, 4);  // 거대한 구름 덩어리
                    float w1 = Worley3D(fx, fy, fz, 8);  // 중간 솜사탕 거품
                    float w2 = Worley3D(fx, fy, fz, 16); // 미세 솜털

                    float perlinWorley = std::clamp(w0 * 0.5f + w1 * 0.35f + w2 * 0.15f, 0.0f, 1.0f);

                    size_t idx = (z * size * size + y * size + x) * 4;
                    pixels[idx + 0] = static_cast<uint8_t>(perlinWorley * 255.0f); // R: Base Shape
                    pixels[idx + 1] = static_cast<uint8_t>(w0 * 255.0f);           // G: Worley 4
                    pixels[idx + 2] = static_cast<uint8_t>(w1 * 255.0f);           // B: Worley 8
                    pixels[idx + 3] = static_cast<uint8_t>(w2 * 255.0f);           // A: Worley 16
                }
            }
        }

        RHITextureDesc desc{};
        desc.width = size; desc.height = size; desc.depth = size;
        desc.mipLevels = 1; desc.arraySize = 1;
        desc.format = ETextureFormat::R8G8B8A8_UNORM;
        desc.dimension = ETextureDimension::Texture3D;
        desc.usage = ETextureUsage::Sampler;
        desc.data = pixels.data();
        desc.dataSize = static_cast<uint32>(pixels.size());

        return rhi->CreateTexture(desc);
    }

    RHITexture* CloudTextureGenerator::CreateWeatherMapTexture(RHI* rhi, uint32 size)
    {
        if (!rhi) return nullptr;
        std::vector<uint8_t> pixels(size * size * 4);

        for (uint32 y = 0; y < size; ++y)
        {
            float fy = static_cast<float>(y) / size;
            for (uint32 x = 0; x < size; ++x)
            {
                float fx = static_cast<float>(x) / size;

                // 2D 저주파 거시 노이즈 (구름 군집과 맑은 하늘의 자연스러운 경계)
                float c0 = Worley3D(fx, fy, 0.1f, 3);
                float c1 = Worley3D(fx, fy, 0.5f, 6);
                float coverage = std::clamp(c0 * 0.65f + c1 * 0.35f, 0.0f, 1.0f);

                size_t idx = (y * size + x) * 4;
                pixels[idx + 0] = static_cast<uint8_t>(coverage * 255.0f);      // R: Coverage
                pixels[idx + 1] = static_cast<uint8_t>((c0 * 0.8f + 0.2f) * 255.0f); // G: Height
                pixels[idx + 2] = static_cast<uint8_t>(255);                     // B: Density
                pixels[idx + 3] = 255;
            }
        }

        RHITextureDesc desc{};
        desc.width = size; desc.height = size; desc.depth = 1;
        desc.mipLevels = 1; desc.arraySize = 1;
        desc.format = ETextureFormat::R8G8B8A8_UNORM;
        desc.dimension = ETextureDimension::Texture2D;
        desc.usage = ETextureUsage::Sampler;
        desc.data = pixels.data();
        desc.dataSize = static_cast<uint32>(pixels.size());

        return rhi->CreateTexture(desc);
    }
}