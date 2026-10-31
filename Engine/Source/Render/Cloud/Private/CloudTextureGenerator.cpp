#include "Render/Cloud/Public/CloudTextureGenerator.h"
#include "Texture.h"

namespace Engine
{
    static float Hash3D(int x, int y, int z)
    {
        int n = x * 374761393 + y * 668265263 + z * 3266489917;
        n = (n ^ (n >> 13)) * 1274126177;
        return static_cast<float>(n & 0x7fffffff) / static_cast<float>(0x7fffffff);
    }

    static float Fade(float t) { return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f); }

    // 0~1 공간에서 완벽히 타일링(Wrap)되는 3D Perlin Gradient 노이즈
    static float Perlin3D(float x, float y, float z, int repeat)
    {
        x *= repeat; y *= repeat; z *= repeat;
        int xi = static_cast<int>(std::floor(x)) % repeat;
        int yi = static_cast<int>(std::floor(y)) % repeat;
        int zi = static_cast<int>(std::floor(z)) % repeat;
        if (xi < 0) xi += repeat; if (yi < 0) yi += repeat; if (zi < 0) zi += repeat;

        float xf = x - std::floor(x); float yf = y - std::floor(y); float zf = z - std::floor(z);
        float u = Fade(xf); float v = Fade(yf); float w = Fade(zf);

        auto grad = [](int h, float gx, float gy, float gz) {
            int n = h & 15;
            float a = n < 8 ? gx : gy;
            float b = n < 4 ? gy : (n == 12 || n == 14 ? gx : gz);
            return ((n & 1) == 0 ? a : -a) + ((n & 2) == 0 ? b : -b);
            };

        auto h = [repeat](int rx, int ry, int rz) {
            return static_cast<int>(Hash3D(rx % repeat, ry % repeat, rz % repeat) * 255.0f);
            };

        float x00 = std::lerp(grad(h(xi, yi, zi), xf, yf, zf), grad(h(xi + 1, yi, zi), xf - 1, yf, zf), u);
        float x10 = std::lerp(grad(h(xi, yi + 1, zi), xf, yf - 1, zf), grad(h(xi + 1, yi + 1, zi), xf - 1, yf - 1, zf), u);
        float x01 = std::lerp(grad(h(xi, yi, zi + 1), xf, yf, zf - 1), grad(h(xi + 1, yi, zi + 1), xf - 1, yf, zf - 1), u);
        float x11 = std::lerp(grad(h(xi, yi + 1, zi + 1), xf, yf - 1, zf - 1), grad(h(xi + 1, yi + 1, zi + 1), xf - 1, yf - 1, zf - 1), u);

        float y0 = std::lerp(x00, x10, v); float y1 = std::lerp(x01, x11, v);
        return (std::lerp(y0, y1, w) + 1.0f) * 0.5f; // 0.0 ~ 1.0 정규화
    }

    // 0~1 공간에서 완벽히 타일링되는 3D Worley(셀룰러) 노이즈
    static float Worley3D(float x, float y, float z, int cellCount, int seedOffset = 0)
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

                    float px = Hash3D(cx + seedOffset, cy, cz);
                    float py = Hash3D(cx + 17, cy + seedOffset + 31, cz + 47);
                    float pz = Hash3D(cx + 53, cy + 71, cz + seedOffset + 89);

                    float diffX = (dx + px) - fx;
                    float diffY = (dy + py) - fy;
                    float diffZ = (dz + pz) - fz;
                    minDist = std::min(minDist, std::sqrt(diffX * diffX + diffY * diffY + diffZ * diffZ));
                }
            }
        }
        return 1.0f - std::min(minDist, 1.0f);
    }

    Texture* CloudTextureGenerator::CreateBaseShapeTexture(RHI* rhi, const std::wstring& key, uint32 size)
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
                    float p0 = Perlin3D(fx, fy, fz, 4);
                    float p1 = Perlin3D(fx, fy, fz, 8);
                    float perlin = p0 * 0.7f + p1 * 0.3f;
                    float w0 = Worley3D(fx, fy, fz, 4, 0);
                    float perlinWorley = std::clamp(perlin * 0.60f + w0 * 0.40f, 0.0f, 1.0f);
                    float w1 = Worley3D(fx, fy, fz, 8, 101);
                    float w2 = Worley3D(fx, fy, fz, 16, 202);
                    float w3 = Worley3D(fx, fy, fz, 32, 303);
                    size_t idx = (z * size * size + y * size + x) * 4;
                    pixels[idx + 0] = static_cast<uint8_t>(perlinWorley * 255.0f);
                    pixels[idx + 1] = static_cast<uint8_t>(w1 * 255.0f);
                    pixels[idx + 2] = static_cast<uint8_t>(w2 * 255.0f);
                    pixels[idx + 3] = static_cast<uint8_t>(w3 * 255.0f);
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
        RHITexture* rhiTex = rhi->CreateTexture(desc);
        if (!rhiTex) return nullptr;
        Texture* tex = Texture::CreateFromRHITexture(rhiTex, key);
        if (tex)
            tex->SetTempData(pixels);
        return tex;
    }
    Texture* CloudTextureGenerator::CreateWeatherMapTexture(RHI* rhi, const std::wstring& key, uint32 size)
    {
        if (!rhi) return nullptr;
        std::vector<uint8_t> pixels(size * size * 4);
        for (uint32 y = 0; y < size; ++y)
        {
            float fy = static_cast<float>(y) / size;
            for (uint32 x = 0; x < size; ++x)
            {
                float fx = static_cast<float>(x) / size;
                float p0 = Perlin3D(fx, fy, 0.1f, 3);
                float p1 = Perlin3D(fx, fy, 0.3f, 6);
                float p2 = Perlin3D(fx, fy, 0.7f, 12);
                float coverage = std::clamp(p0 * 0.55f + p1 * 0.30f + p2 * 0.15f, 0.0f, 1.0f);
                float heightGrad = std::clamp(p0 * 0.7f + 0.3f, 0.4f, 1.0f);
                size_t idx = (y * size + x) * 4;
                pixels[idx + 0] = static_cast<uint8_t>(coverage * 255.0f);
                pixels[idx + 1] = static_cast<uint8_t>(heightGrad * 255.0f);
                pixels[idx + 2] = 255;
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
        RHITexture* rhiTex = rhi->CreateTexture(desc);
        if (!rhiTex) return nullptr;
        Texture* tex = Texture::CreateFromRHITexture(rhiTex, key);
        if (tex)
            tex->SetTempData(pixels);
        return tex;
    }
}