#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include <Windows.h>
#include <DirectXTex.h>

#include "HeadlessThumbnailRasterizer.h"
#include "ComponentEnable.h"

#include "BoundingBox.h"
#include "Engine_Includes.h"
#include "Functions.h"
#include "MaterialInterface.h"
#include "MaterialInstance.h"
#include "Mesh.h"
#include "Model.h"
#include "ModelConverter.h"
#include "PlatformPath.h"
#include "ResourceManager.h"
#include "Scene.h"
#include "SkinnedMeshRendererComponent.h"
#include "Sprite.h"
#include "SpriteRendererComponent.h"
#include "StaticMeshRendererComponent.h"
#include "Texture.h"
#include "TextureBinaryHeader.h"
#include "TransformComponent.h"
#include "TransformSystem.h"
#include "Vertex.h"
#include "WorldTransformComponent.h"

#include <archive/BeveArchive.h>
#include <archive/JsonArchive.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <fstream>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

namespace Tools::AssetWorker
{
    namespace
    {
        struct MaterialInfo
        {
            glm::vec4 BaseColor{ 0.78f, 0.80f, 0.84f, 1.0f };
            float Metallic = 0.2f;
            float Roughness = 0.45f;
            bool DoubleSided = false;
            std::vector<uint8_t> AlbedoPixels;
            int AlbedoWidth = 0;
            int AlbedoHeight = 0;
        };

        struct DecodedImage
        {
            std::vector<uint8_t> Pixels;
            int Width = 0;
            int Height = 0;
        };

        struct SceneDeleter
        {
            void operator()(Engine::Scene* scene) const { if (scene) scene->Release(); }
        };

        struct ResourceRefInfo
        {
            bool Null = true;
            std::wstring AssetKey;
            std::wstring AssetPath;
        };

        struct WorkerContext
        {
            std::unordered_map<std::wstring, MaterialInfo> MaterialCache;
            std::unordered_map<std::wstring, DecodedImage> TextureCache;
        };

        std::string ToLowerAscii(std::string value)
        {
            for (char& ch : value) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
            return value;
        }

        std::string PathExtLower(const std::filesystem::path& path)
        {
            return ToLowerAscii(Engine::Unicode::WideToUtf8(path.extension().wstring()));
        }

        std::string PathNameLower(const std::filesystem::path& path)
        {
            return ToLowerAscii(Engine::Unicode::WideToUtf8(path.filename().wstring()));
        }

        bool IsJsonArchivePath(const std::filesystem::path& path)
        {
            const auto ext = PathExtLower(path);
            return ext == ".json" || ext == ".prefab";
        }

        std::unique_ptr<archive::IArchive> OpenArchiveFromFile(const std::filesystem::path& filePath)
        {
            std::error_code ec;
            if (!std::filesystem::is_regular_file(filePath, ec)) return nullptr;

            std::unique_ptr<archive::IArchive> ar;
            if (IsJsonArchivePath(filePath)) ar = std::make_unique<archive::JsonArchive>();
            else ar = std::make_unique<archive::BeveArchive>();

            if (!ar->LoadFromFile(filePath)) return nullptr;
            return ar;
        }

        std::filesystem::path ResolveReferencePath(const std::wstring& assetKey, const std::wstring& assetPath, const std::filesystem::path& ownerDir)
        {
            std::error_code ec;
            const auto tryCandidate = [&](const std::wstring& raw) -> std::filesystem::path
                {
                    if (raw.empty()) return {};
                    auto resolved = Engine::PlatformPath::ResolveUri(raw);
                    if (std::filesystem::is_regular_file(resolved, ec)) return resolved;

                    // 레거시 Resources/ -> Assets/ 치환 폴백
                    std::wstring replaced = raw;
                    auto replaceSeg = [&](const std::wstring& from, const std::wstring& to) -> bool
                        {
                            const size_t pos = replaced.find(from);
                            if (pos == std::wstring::npos) return false;
                            replaced.replace(pos, from.size(), to);
                            return true;
                        };
                    bool modified = replaceSeg(L"/Resources/", L"/Assets/") || replaceSeg(L"\\Resources\\", L"\\Assets\\");
                    if (!modified && replaced.rfind(L"Resources/", 0) == 0)
                    {
                        replaced.replace(0, 10, L"Assets/");
                        modified = true;
                    }
                    if (modified)
                    {
                        auto alt = Engine::PlatformPath::ResolveUri(replaced);
                        if (std::filesystem::is_regular_file(alt, ec)) return alt;
                    }

                    // 소유 에셋과 동일 폴더 내 파일명 폴백
                    if (!ownerDir.empty())
                    {
                        const auto filename = std::filesystem::path(raw).filename();
                        if (!filename.empty())
                        {
                            const auto local = ownerDir / filename;
                            if (std::filesystem::is_regular_file(local, ec)) return local;
                        }
                    }
                    return {};
                };

            if (auto found = tryCandidate(assetPath); !found.empty()) return found;
            if (auto found = tryCandidate(assetKey); !found.empty()) return found;
            return {};
        }

        archive::ArchiveResult ReadResourceRefObject(archive::IArchive& object, ResourceRefInfo& outRef)
        {
            outRef = {};
            bool hasNull = false;
            auto r = object.HasField("Null", hasNull);
            if (!r) return r;
            if (hasNull)
            {
                r = object.Read("Null", outRef.Null);
                if (!r) return r;
                if (outRef.Null) return {};
            }

            bool hasKey = false, hasPath = false;
            r = object.HasField("AssetKey", hasKey);
            if (r && hasKey) r = object.Read("AssetKey", outRef.AssetKey);
            if (r) r = object.HasField("AssetPath", hasPath);
            if (r && hasPath) r = object.Read("AssetPath", outRef.AssetPath);
            if (!r) return r;

            if (!outRef.AssetKey.empty() || !outRef.AssetPath.empty()) outRef.Null = false;
            return {};
        }

        std::vector<uint8_t> DownsampleRGBA(const uint8_t* src, int srcW, int srcH, int dstW, int dstH)
        {
            std::vector<uint8_t> dst(static_cast<size_t>(dstW) * dstH * 4);
            const float xRatio = static_cast<float>(srcW) / static_cast<float>(dstW);
            const float yRatio = static_cast<float>(srcH) / static_cast<float>(dstH);

            for (int y = 0; y < dstH; ++y)
            {
                const float srcY = (static_cast<float>(y) + 0.5f) * yRatio - 0.5f;
                const int y0 = std::clamp(static_cast<int>(srcY), 0, srcH - 1);
                const int y1 = std::clamp(y0 + 1, 0, srcH - 1);
                const float yW = srcY - static_cast<float>(y0);

                for (int x = 0; x < dstW; ++x)
                {
                    const float srcX = (static_cast<float>(x) + 0.5f) * xRatio - 0.5f;
                    const int x0 = std::clamp(static_cast<int>(srcX), 0, srcW - 1);
                    const int x1 = std::clamp(x0 + 1, 0, srcW - 1);
                    const float xW = srcX - static_cast<float>(x0);

                    const size_t dstIdx = (static_cast<size_t>(y) * dstW + x) * 4;
                    const size_t i00 = (static_cast<size_t>(y0) * srcW + x0) * 4;
                    const size_t i10 = (static_cast<size_t>(y0) * srcW + x1) * 4;
                    const size_t i01 = (static_cast<size_t>(y1) * srcW + x0) * 4;
                    const size_t i11 = (static_cast<size_t>(y1) * srcW + x1) * 4;

                    for (int c = 0; c < 4; ++c)
                    {
                        const float top = src[i00 + c] * (1.0f - xW) + src[i10 + c] * xW;
                        const float bot = src[i01 + c] * (1.0f - xW) + src[i11 + c] * xW;
                        const float val = top * (1.0f - yW) + bot * yW;
                        dst[dstIdx + c] = static_cast<uint8_t>(std::clamp(val, 0.0f, 255.0f));
                    }
                }
            }
            return dst;
        }

        DecodedImage ShrinkToMaxDim(const uint8_t* rgba, int w, int h, int maxDim = 128)
        {
            DecodedImage img;
            if (!rgba || w <= 0 || h <= 0) return img;
            if (w <= maxDim && h <= maxDim)
            {
                img.Width = w;
                img.Height = h;
                img.Pixels.assign(rgba, rgba + static_cast<size_t>(w) * h * 4);
                return img;
            }
            const float aspect = static_cast<float>(w) / static_cast<float>(h);
            int dstW = maxDim;
            int dstH = maxDim;
            if (aspect >= 1.0f) dstH = (std::max)(1, static_cast<int>(maxDim / aspect));
            else dstW = (std::max)(1, static_cast<int>(maxDim * aspect));
            img.Width = dstW;
            img.Height = dstH;
            img.Pixels = DownsampleRGBA(rgba, w, h, dstW, dstH);
            return img;
        }

        DXGI_FORMAT ToDxgiFormat(Engine::ETextureFormat fmt)
        {
            using F = Engine::ETextureFormat;
            switch (fmt)
            {
            case F::R8G8B8A8_UNORM: return DXGI_FORMAT_R8G8B8A8_UNORM;
            case F::R8G8B8A8_UNORM_SRGB: return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
            case F::B8G8R8A8_UNORM: return DXGI_FORMAT_B8G8R8A8_UNORM;
            case F::B8G8R8A8_UNORM_SRGB: return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
            case F::BC1_RGBA_UNORM: return DXGI_FORMAT_BC1_UNORM;
            case F::BC1_RGBA_UNORM_SRGB: return DXGI_FORMAT_BC1_UNORM_SRGB;
            case F::BC2_RGBA_UNORM: return DXGI_FORMAT_BC2_UNORM;
            case F::BC2_RGBA_UNORM_SRGB: return DXGI_FORMAT_BC2_UNORM_SRGB;
            case F::BC3_RGBA_UNORM: return DXGI_FORMAT_BC3_UNORM;
            case F::BC3_RGBA_UNORM_SRGB: return DXGI_FORMAT_BC3_UNORM_SRGB;
            case F::BC4_R_UNORM: return DXGI_FORMAT_BC4_UNORM;
            case F::BC5_RG_UNORM: return DXGI_FORMAT_BC5_UNORM;
            case F::BC7_RGBA_UNORM: return DXGI_FORMAT_BC7_UNORM;
            case F::BC7_RGBA_UNORM_SRGB: return DXGI_FORMAT_BC7_UNORM_SRGB;
            default: return DXGI_FORMAT_UNKNOWN;
            }
        }

        DecodedImage DecodeTextureFromMemory(const uint8_t* encoded, size_t size)
        {
            if (!encoded || size == 0 || size > static_cast<size_t>((std::numeric_limits<int>::max)())) return {};
            int w = 0, h = 0, c = 0;
            std::unique_ptr<stbi_uc, decltype(&stbi_image_free)> pixels(
                stbi_load_from_memory(encoded, static_cast<int>(size), &w, &h, &c, STBI_rgb_alpha), stbi_image_free);
            if (!pixels || w <= 0 || h <= 0) return {};
            return ShrinkToMaxDim(pixels.get(), w, h, 128);
        }

        DecodedImage LoadTextureCached(const std::filesystem::path& texPath, WorkerContext& ctx)
        {
            if (texPath.empty()) return {};
            const std::wstring cacheKey = Engine::PlatformPath::Normalize(texPath).wstring();
            if (auto it = ctx.TextureCache.find(cacheKey); it != ctx.TextureCache.end()) return it->second;

            DecodedImage result;
            const auto ext = PathExtLower(texPath);
            const auto name = PathNameLower(texPath);

            if (ext == ".bamtex" || name.ends_with(".bamtex.json"))
            {
                if (auto ar = OpenArchiveFromFile(texPath))
                {
                    Engine::TextureBinaryHeader header{};
                    std::vector<uint8_t> pixelData;
                    bool ok = false;
                    (void)ar->ReadObject("Texture", header, [&](archive::IArchive& src, Engine::TextureBinaryHeader& hdr) -> archive::ArchiveResult
                        {
                            auto r = src.ReadObject("TextureHeader", hdr, [](archive::IArchive& fields, Engine::TextureBinaryHeader& h) -> archive::ArchiveResult
                                {
                                    auto fr = fields.Read("width", h.width);
                                    if (fr) fr = fields.Read("height", h.height);
                                    if (fr) fr = fields.Read("format", h.format);
                                    return fr;
                                });
                            if (!r) return r;
                            r = src.ReadArray("PixelData", pixelData);
                            ok = static_cast<bool>(r);
                            return r;
                        });
                    if (ok && header.width > 0 && header.height > 0 && !pixelData.empty())
                    {
                        const DXGI_FORMAT dxgiFmt = ToDxgiFormat(header.format);
                        if (dxgiFmt != DXGI_FORMAT_UNKNOWN)
                        {
                            size_t rowPitch = 0, slicePitch = 0;
                            if (SUCCEEDED(DirectX::ComputePitch(dxgiFmt, header.width, header.height, rowPitch, slicePitch)) &&
                                pixelData.size() >= slicePitch)
                            {
                                DirectX::Image img{};
                                img.width = header.width;
                                img.height = header.height;
                                img.format = dxgiFmt;
                                img.rowPitch = rowPitch;
                                img.slicePitch = slicePitch;
                                img.pixels = pixelData.data();

                                DirectX::ScratchImage converted;
                                HRESULT hr = DirectX::IsCompressed(dxgiFmt)
                                    ? DirectX::Decompress(img, DXGI_FORMAT_R8G8B8A8_UNORM, converted)
                                    : (dxgiFmt != DXGI_FORMAT_R8G8B8A8_UNORM
                                        ? DirectX::Convert(img, DXGI_FORMAT_R8G8B8A8_UNORM, DirectX::TEX_FILTER_DEFAULT, 0.5f, converted)
                                        : E_FAIL);

                                if (dxgiFmt == DXGI_FORMAT_R8G8B8A8_UNORM)
                                    result = ShrinkToMaxDim(pixelData.data(), static_cast<int>(header.width), static_cast<int>(header.height), 128);
                                else if (SUCCEEDED(hr) && converted.GetImage(0, 0, 0))
                                    result = ShrinkToMaxDim(converted.GetImage(0, 0, 0)->pixels, static_cast<int>(header.width), static_cast<int>(header.height), 128);
                            }
                        }
                    }
                }
            }
            else if (ext == ".dds")
            {
                DirectX::ScratchImage ddsImg;
                if (SUCCEEDED(DirectX::LoadFromDDSFile(texPath.wstring().c_str(), DirectX::DDS_FLAGS_NONE, nullptr, ddsImg)))
                {
                    const DirectX::ScratchImage* finalImg = &ddsImg;
                    DirectX::ScratchImage conv;
                    if (DirectX::IsCompressed(ddsImg.GetMetadata().format))
                    {
                        if (SUCCEEDED(DirectX::Decompress(*ddsImg.GetImage(0, 0, 0), DXGI_FORMAT_R8G8B8A8_UNORM, conv))) finalImg = &conv;
                        else finalImg = nullptr;
                    }
                    else if (ddsImg.GetMetadata().format != DXGI_FORMAT_R8G8B8A8_UNORM)
                    {
                        if (SUCCEEDED(DirectX::Convert(*ddsImg.GetImage(0, 0, 0), DXGI_FORMAT_R8G8B8A8_UNORM, DirectX::TEX_FILTER_DEFAULT, 0.5f, conv))) finalImg = &conv;
                        else finalImg = nullptr;
                    }
                    if (finalImg && finalImg->GetImage(0, 0, 0))
                    {
                        const auto* im = finalImg->GetImage(0, 0, 0);
                        result = ShrinkToMaxDim(im->pixels, static_cast<int>(im->width), static_cast<int>(im->height), 128);
                    }
                }
            }
            else
            {
                std::ifstream file(texPath, std::ios::binary | std::ios::ate);
                if (file.is_open())
                {
                    const auto sz = file.tellg();
                    if (sz > 0 && sz <= 32 * 1024 * 1024)
                    {
                        file.seekg(0, std::ios::beg);
                        std::vector<uint8_t> bytes(static_cast<size_t>(sz));
                        if (file.read(reinterpret_cast<char*>(bytes.data()), sz))
                            result = DecodeTextureFromMemory(bytes.data(), bytes.size());
                    }
                }
            }

            ctx.TextureCache.emplace(cacheKey, result);
            return result;
        }

        MaterialInfo LoadMaterialInfoCached(const std::filesystem::path& matPath, WorkerContext& ctx)
        {
            MaterialInfo info;
            if (matPath.empty()) return info;

            const std::wstring cacheKey = Engine::PlatformPath::Normalize(matPath).wstring();
            if (auto it = ctx.MaterialCache.find(cacheKey); it != ctx.MaterialCache.end()) return it->second;

            if (auto ar = OpenArchiveFromFile(matPath))
            {
                bool hasField = false;
                if (ar->HasField("m_CullMode", hasField) && hasField)
                {
                    Engine::ECullMode cull = Engine::ECullMode::Back;
                    if (ar->Read("m_CullMode", cull) && cull == Engine::ECullMode::None) info.DoubleSided = true;
                }

                struct RawMatParam
                {
                    std::vector<uint8_t> Data;
                };
                if (ar->HasField("m_Parameters", hasField) && hasField)
                {
                    std::unordered_map<std::string, RawMatParam> params;
                    auto mr = ar->ReadObjectMap("m_Parameters", params, [](archive::IArchive& obj, RawMatParam& val) -> archive::ArchiveResult
                        {
                            bool hasData = false;
                            auto r = obj.HasField("data", hasData);
                            if (r && hasData) r = obj.ReadArray("data", val.Data);
                            return r;
                        });
                    if (mr)
                    {
                        if (auto it = params.find("BaseColor"); it != params.end() && it->second.Data.size() >= sizeof(glm::vec4))
                            std::memcpy(&info.BaseColor, it->second.Data.data(), sizeof(glm::vec4));
                        if (auto it = params.find("Metallic"); it != params.end() && it->second.Data.size() >= sizeof(float))
                            std::memcpy(&info.Metallic, it->second.Data.data(), sizeof(float));
                        if (auto it = params.find("Roughness"); it != params.end() && it->second.Data.size() >= sizeof(float))
                            std::memcpy(&info.Roughness, it->second.Data.data(), sizeof(float));
                    }
                }

                if (ar->HasField("m_TextureBindings", hasField) && hasField)
                {
                    std::filesystem::path albedoPath;
                    (void)ar->ReadObjectArray("m_TextureBindings", [&](archive::IArchive& obj, std::size_t) -> archive::ArchiveResult
                        {
                            uint32_t slot = 999;
                            bool hasSlot = false;
                            auto r = obj.HasField("slot", hasSlot);
                            if (r && hasSlot) r = obj.Read("slot", slot);
                            if (!r || slot != 0) return {};

                            bool hasTex = false;
                            r = obj.HasField("texture", hasTex);
                            if (r && hasTex)
                            {
                                ResourceRefInfo texRef;
                                r = obj.ReadObject("texture", texRef, [](archive::IArchive& tObj, ResourceRefInfo& ref) { return ReadResourceRefObject(tObj, ref); });
                                if (r && !texRef.Null) albedoPath = ResolveReferencePath(texRef.AssetKey, texRef.AssetPath, matPath.parent_path());
                            }
                            return {};
                        });

                    if (!albedoPath.empty())
                    {
                        auto img = LoadTextureCached(albedoPath, ctx);
                        if (!img.Pixels.empty())
                        {
                            info.AlbedoPixels = std::move(img.Pixels);
                            info.AlbedoWidth = img.Width;
                            info.AlbedoHeight = img.Height;
                        }
                    }
                }
            }

            ctx.MaterialCache.emplace(cacheKey, info);
            return info;
        }

        MaterialInfo MaterialInfoFromInterface(Engine::MaterialInterface* mat, const std::filesystem::path& ownerDir, WorkerContext& ctx)
        {
            MaterialInfo info;
            if (!mat) return info;

            if (mat->HasParameter("BaseColor")) info.BaseColor = mat->GetParameter<glm::vec4>("BaseColor", info.BaseColor);
            if (mat->HasParameter("Metallic")) info.Metallic = mat->GetParameter<float>("Metallic", info.Metallic);
            if (mat->HasParameter("Roughness")) info.Roughness = mat->GetParameter<float>("Roughness", info.Roughness);
            info.DoubleSided = (mat->GetCullMode() == Engine::ECullMode::None);

            for (const auto& binding : mat->GetTextureBindings())
            {
                if (binding.slot != 0) continue;
                const auto* tex = binding.texture.Get();
                if (!tex) continue;
                const auto resolved = ResolveReferencePath(tex->GetKey(), tex->GetPath(), ownerDir);
                if (!resolved.empty())
                {
                    auto img = LoadTextureCached(resolved, ctx);
                    info.AlbedoPixels = std::move(img.Pixels);
                    info.AlbedoWidth = img.Width;
                    info.AlbedoHeight = img.Height;
                }
                break;
            }
            return info;
        }

        void ApplyMaterialToBatch(const MaterialInfo& mat, RasterBatch& batch)
        {
            batch.BaseColor = mat.BaseColor;
            batch.Metallic = mat.Metallic;
            batch.Roughness = mat.Roughness;
            batch.DoubleSided = mat.DoubleSided;
            batch.AlbedoPixels = mat.AlbedoPixels;
            batch.AlbedoWidth = mat.AlbedoWidth;
            batch.AlbedoHeight = mat.AlbedoHeight;
        }

        bool ExtractMeshToBatch(const Engine::Mesh& mesh, RasterBatch& batch)
        {
            const uint32_t vCount = mesh.GetStreamCount(Engine::EMeshStream::Position);
            const uint32_t posStride = mesh.GetStreamStride(Engine::EMeshStream::Position);
            const auto& posRaw = mesh.GetRawStreamData(Engine::EMeshStream::Position);
            if (vCount == 0 || posStride < sizeof(glm::vec3) || posRaw.size() < static_cast<size_t>(vCount) * posStride) return false;

            const auto& matRaw = mesh.GetRawStreamData(Engine::EMeshStream::Material);
            const uint32_t matStride = mesh.GetStreamStride(Engine::EMeshStream::Material);
            const bool hasMat = (matStride >= sizeof(Engine::VertexMaterial) && matRaw.size() >= static_cast<size_t>(vCount) * matStride);

            batch.Vertices.resize(vCount);
            for (uint32_t i = 0; i < vCount; ++i)
            {
                RasterVertex rv{};
                std::memcpy(&rv.Position, posRaw.data() + static_cast<size_t>(i) * posStride, sizeof(glm::vec3));
                if (hasMat)
                {
                    Engine::VertexMaterial vm{};
                    std::memcpy(&vm, matRaw.data() + static_cast<size_t>(i) * matStride, sizeof(Engine::VertexMaterial));
                    rv.Normal = vm.normal;
                    rv.TexCoord = vm.texCoord;
                    rv.Color = vm.color;
                }
                batch.Vertices[i] = rv;
            }

            const auto& idxRaw = mesh.GetRawIndexData();
            const uint32_t idxCount = mesh.GetIndexCount();
            const uint32_t idxStride = mesh.GetIndexStride();
            const bool hasIdx = (idxCount >= 3 && (idxStride == 2 || idxStride == 4) && idxRaw.size() >= static_cast<size_t>(idxCount) * idxStride);

            const size_t totalTris = hasIdx ? (idxCount / 3) : (vCount / 3);
            if (totalTris == 0) return false;

            const size_t maxTrisPerMesh = 80000;
            const size_t triStep = (std::max<size_t>)(1, totalTris / maxTrisPerMesh);
            batch.Indices.reserve((std::min<size_t>)(totalTris, maxTrisPerMesh) * 3);

            auto readIdx = [&](size_t idx) -> uint32_t
                {
                    if (!hasIdx) return static_cast<uint32_t>(idx);
                    if (idxStride == 4)
                    {
                        uint32_t v = 0;
                        std::memcpy(&v, idxRaw.data() + idx * 4, 4);
                        return v;
                    }
                    uint16_t v = 0;
                    std::memcpy(&v, idxRaw.data() + idx * 2, 2);
                    return v;
                };

            for (size_t t = 0; t < totalTris; t += triStep)
            {
                const uint32_t i0 = readIdx(t * 3 + 0);
                const uint32_t i1 = readIdx(t * 3 + 1);
                const uint32_t i2 = readIdx(t * 3 + 2);
                if (i0 < vCount && i1 < vCount && i2 < vCount)
                {
                    batch.Indices.push_back(i0);
                    batch.Indices.push_back(i1);
                    batch.Indices.push_back(i2);
                }
            }

            if (!hasMat)
            {
                for (size_t i = 0; i + 2 < batch.Indices.size(); i += 3)
                {
                    auto& a = batch.Vertices[batch.Indices[i + 0]];
                    auto& b = batch.Vertices[batch.Indices[i + 1]];
                    auto& c = batch.Vertices[batch.Indices[i + 2]];
                    glm::vec3 n = glm::cross(b.Position - a.Position, c.Position - a.Position);
                    const float len = glm::length(n);
                    if (len > 1e-6f) n /= len;
                    else n = glm::vec3(0.0f, 1.0f, 0.0f);
                    a.Normal = b.Normal = c.Normal = n;
                }
            }
            return !batch.Indices.empty();
        }

        bool LoadBamMeshArchive(const std::filesystem::path& meshPath, RasterBatch& outBatch)
        {
            auto ar = OpenArchiveFromFile(meshPath);
            if (!ar) return false;

            std::unique_ptr<Engine::Mesh> mesh(Engine::Mesh::CreateEmpty());
            if (!mesh || !mesh->ReadArchive(*ar)) return false;

            const bool ok = ExtractMeshToBatch(*mesh, outBatch);
            mesh->Free();
            return ok;
        }

        bool LoadBamModelAsset(const std::filesystem::path& modelPath, std::vector<RasterBatch>& outBatches, WorkerContext& ctx)
        {
            auto ar = OpenArchiveFromFile(modelPath);
            if (!ar) return false;

            struct RawNode
            {
                int32_t ParentIndex = -1;
                glm::mat4 LocalTransform{ 1.0f };
                std::vector<uint32_t> MeshIndices;
            };
            struct RawMeshLOD
            {
                ResourceRefInfo LOD1;
                ResourceRefInfo LOD2;
            };

            glm::vec3 placementEuler{ 0.0f };
            bool hasField = false;
            if (ar->HasField("m_PlacementEulerDegrees", hasField) && hasField)
            {
                std::array<float, 3> deg{};
                if (ar->ReadArray("m_PlacementEulerDegrees", deg))
                    placementEuler = glm::vec3(deg[0], deg[1], deg[2]);
            }

            std::vector<RawNode> nodes;
            if (ar->HasField("m_Nodes", hasField) && hasField)
            {
                (void)ar->ReadObjectArray("m_Nodes", nodes, [](archive::IArchive& obj, RawNode& node) -> archive::ArchiveResult
                    {
                        bool f = false;
                        if (obj.HasField("ParentIndex", f) && f) (void)obj.Read("ParentIndex", node.ParentIndex);
                        if (obj.HasField("LocalTransform", f) && f)
                        {
                            std::array<float, 16> m{};
                            if (obj.ReadArray("LocalTransform", m)) std::memcpy(&node.LocalTransform, m.data(), sizeof(m));
                        }
                        if (obj.HasField("MeshIndices", f) && f) (void)obj.ReadArray("MeshIndices", node.MeshIndices);
                        return {};
                    });
            }

            std::vector<ResourceRefInfo> meshRefs;
            if (ar->HasField("m_Meshes", hasField) && hasField)
            {
                (void)ar->ReadObjectArray("m_Meshes", meshRefs, [](archive::IArchive& obj, ResourceRefInfo& ref) { return ReadResourceRefObject(obj, ref); });
            }

            std::vector<RawMeshLOD> meshLods;
            if (ar->HasField("m_MeshLODs", hasField) && hasField)
            {
                (void)ar->ReadObjectArray("m_MeshLODs", meshLods, [](archive::IArchive& obj, RawMeshLOD& lod) -> archive::ArchiveResult
                    {
                        bool f = false;
                        if (obj.HasField("LOD1Mesh", f) && f)
                            (void)obj.ReadObject("LOD1Mesh", lod.LOD1, [](archive::IArchive& o, ResourceRefInfo& r) { return ReadResourceRefObject(o, r); });
                        if (obj.HasField("LOD2Mesh", f) && f)
                            (void)obj.ReadObject("LOD2Mesh", lod.LOD2, [](archive::IArchive& o, ResourceRefInfo& r) { return ReadResourceRefObject(o, r); });
                        return {};
                    });
            }

            std::vector<ResourceRefInfo> matRefs;
            if (ar->HasField("m_Materials", hasField) && hasField)
            {
                (void)ar->ReadObjectArray("m_Materials", matRefs, [](archive::IArchive& obj, ResourceRefInfo& ref) { return ReadResourceRefObject(obj, ref); });
            }

            std::vector<uint32_t> meshMatIndices;
            if (ar->HasField("m_MeshMaterialIndices", hasField) && hasField)
                (void)ar->ReadArray("m_MeshMaterialIndices", meshMatIndices);

            if (meshRefs.empty()) return false;

            Engine::TransformComponent rootTr;
            (void)rootTr.SetEulerRotation(placementEuler);
            const glm::mat4 placementMatrix = rootTr.GetLocalMatrix();

            std::vector<glm::mat4> worldTransforms(nodes.size(), placementMatrix);
            for (size_t i = 0; i < nodes.size(); ++i)
            {
                const int32_t p = nodes[i].ParentIndex;
                worldTransforms[i] = (p >= 0 && static_cast<size_t>(p) < i)
                    ? worldTransforms[p] * nodes[i].LocalTransform : placementMatrix * nodes[i].LocalTransform;
            }

            const std::filesystem::path modelDir = modelPath.parent_path();
            const bool preferLowestLod = (meshRefs.size() > 32);

            std::vector<std::optional<RasterBatch>> decodedMeshes(meshRefs.size());
            auto getMeshBatch = [&](uint32_t meshIdx) -> const RasterBatch*
                {
                    if (meshIdx >= meshRefs.size()) return nullptr;
                    if (!decodedMeshes[meshIdx].has_value())
                    {
                        std::filesystem::path chosenPath;
                        if (preferLowestLod && meshIdx < meshLods.size())
                        {
                            if (!meshLods[meshIdx].LOD2.Null)
                                chosenPath = ResolveReferencePath(meshLods[meshIdx].LOD2.AssetKey, meshLods[meshIdx].LOD2.AssetPath, modelDir);
                            if (chosenPath.empty() && !meshLods[meshIdx].LOD1.Null)
                                chosenPath = ResolveReferencePath(meshLods[meshIdx].LOD1.AssetKey, meshLods[meshIdx].LOD1.AssetPath, modelDir);
                        }
                        if (chosenPath.empty() && !meshRefs[meshIdx].Null)
                            chosenPath = ResolveReferencePath(meshRefs[meshIdx].AssetKey, meshRefs[meshIdx].AssetPath, modelDir);

                        RasterBatch b;
                        if (!chosenPath.empty() && LoadBamMeshArchive(chosenPath, b))
                        {
                            if (meshIdx < meshMatIndices.size())
                            {
                                const uint32_t matIdx = meshMatIndices[meshIdx];
                                if (matIdx < matRefs.size() && !matRefs[matIdx].Null)
                                {
                                    const auto matPath = ResolveReferencePath(matRefs[matIdx].AssetKey, matRefs[matIdx].AssetPath, modelDir);
                                    if (!matPath.empty()) ApplyMaterialToBatch(LoadMaterialInfoCached(matPath, ctx), b);
                                }
                            }
                            decodedMeshes[meshIdx] = std::move(b);
                        }
                        else
                        {
                            decodedMeshes[meshIdx] = RasterBatch{};
                        }
                    }
                    return decodedMeshes[meshIdx]->Indices.empty() ? nullptr : &(*decodedMeshes[meshIdx]);
                };

            if (!nodes.empty())
            {
                for (size_t i = 0; i < nodes.size(); ++i)
                {
                    for (uint32_t meshIdx : nodes[i].MeshIndices)
                    {
                        if (const auto* proto = getMeshBatch(meshIdx))
                        {
                            RasterBatch inst = *proto;
                            inst.WorldMatrix = worldTransforms[i];
                            outBatches.push_back(std::move(inst));
                        }
                    }
                }
            }
            else
            {
                for (uint32_t meshIdx = 0; meshIdx < static_cast<uint32_t>(meshRefs.size()); ++meshIdx)
                {
                    if (const auto* proto = getMeshBatch(meshIdx))
                    {
                        RasterBatch inst = *proto;
                        inst.WorldMatrix = placementMatrix;
                        outBatches.push_back(std::move(inst));
                    }
                }
            }
            return !outBatches.empty();
        }

        bool LoadRawModelAsset(const std::filesystem::path& sourcePath, std::vector<RasterBatch>& outBatches, WorkerContext& ctx)
        {
            std::error_code ec;
            const auto parentDir = sourcePath.parent_path();
            const auto stem = sourcePath.stem().wstring();
            const auto candidate1 = parentDir / (stem + L".bammodel");
            const auto candidate2 = parentDir / stem / (stem + L".bammodel");

            if (std::filesystem::is_regular_file(candidate1, ec) && LoadBamModelAsset(candidate1, outBatches, ctx)) return true;
            if (std::filesystem::is_regular_file(candidate2, ec) && LoadBamModelAsset(candidate2, outBatches, ctx)) return true;

            Tools::ModelConversion::Options opt{};
            opt.FlipUVs = true;
            opt.GenerateLODs = false;
            const auto result = Tools::ModelConversion::ConvertStaticModel(sourcePath, opt);
            if (!result || !result.Model.has_value() || result.Model->Meshes.empty()) return false;

            const auto& modelData = *result.Model;
            std::vector<MaterialInfo> matInfos(modelData.Materials.size());
            for (size_t m = 0; m < modelData.Materials.size(); ++m)
            {
                const auto& sm = modelData.Materials[m];
                matInfos[m].BaseColor = glm::vec4(sm.BaseColorFactor[0], sm.BaseColorFactor[1], sm.BaseColorFactor[2], sm.BaseColorFactor[3]);
                matInfos[m].Metallic = sm.MetallicFactor;
                matInfos[m].Roughness = sm.RoughnessFactor;
                if (sm.BaseColorTexture.IsEmbedded && !sm.BaseColorTexture.EmbeddedData.empty())
                {
                    auto img = DecodeTextureFromMemory(sm.BaseColorTexture.EmbeddedData.data(), sm.BaseColorTexture.EmbeddedData.size());
                    matInfos[m].AlbedoPixels = std::move(img.Pixels);
                    matInfos[m].AlbedoWidth = img.Width;
                    matInfos[m].AlbedoHeight = img.Height;
                }
                else if (!sm.BaseColorTexture.Path.empty())
                {
                    const auto texRel = Engine::Utf8ToPath(sm.BaseColorTexture.Path);
                    const auto resolved = std::filesystem::is_regular_file(parentDir / texRel, ec)
                        ? (parentDir / texRel) : ResolveReferencePath(texRel.wstring(), texRel.wstring(), parentDir);
                    if (!resolved.empty())
                    {
                        auto img = LoadTextureCached(resolved, ctx);
                        matInfos[m].AlbedoPixels = std::move(img.Pixels);
                        matInfos[m].AlbedoWidth = img.Width;
                        matInfos[m].AlbedoHeight = img.Height;
                    }
                }
            }

            std::vector<RasterBatch> protoMeshes(modelData.Meshes.size());
            for (size_t mi = 0; mi < modelData.Meshes.size(); ++mi)
            {
                const auto& srcMesh = modelData.Meshes[mi];
                RasterBatch& b = protoMeshes[mi];
                b.Vertices.resize(srcMesh.Vertices.size());
                for (size_t vi = 0; vi < srcMesh.Vertices.size(); ++vi)
                {
                    const auto& sv = srcMesh.Vertices[vi];
                    b.Vertices[vi] = RasterVertex{
                        glm::vec3(sv.Position[0], sv.Position[1], sv.Position[2]),
                        glm::vec3(sv.Normal[0], sv.Normal[1], sv.Normal[2]),
                        glm::vec2(sv.TexCoord[0], sv.TexCoord[1]),
                        glm::vec4(sv.Color[0], sv.Color[1], sv.Color[2], sv.Color[3])
                    };
                }
                const size_t totalTris = srcMesh.Indices.size() / 3;
                const size_t triStep = (std::max<size_t>)(1, totalTris / 80000);
                for (size_t t = 0; t < totalTris; t += triStep)
                {
                    b.Indices.push_back(srcMesh.Indices[t * 3 + 0]);
                    b.Indices.push_back(srcMesh.Indices[t * 3 + 1]);
                    b.Indices.push_back(srcMesh.Indices[t * 3 + 2]);
                }
                if (srcMesh.MaterialIndex < matInfos.size()) ApplyMaterialToBatch(matInfos[srcMesh.MaterialIndex], b);
            }

            if (!modelData.Nodes.empty())
            {
                std::vector<glm::mat4> worldTransforms(modelData.Nodes.size(), glm::mat4(1.0f));
                for (size_t i = 0; i < modelData.Nodes.size(); ++i)
                {
                    glm::mat4 local(1.0f);
                    std::memcpy(&local, modelData.Nodes[i].LocalTransform.data(), sizeof(float) * 16);
                    const int32_t p = modelData.Nodes[i].ParentIndex;
                    worldTransforms[i] = (p >= 0 && static_cast<size_t>(p) < i) ? (worldTransforms[p] * local) : local;
                    for (uint32_t meshIdx : modelData.Nodes[i].MeshIndices)
                    {
                        if (meshIdx < protoMeshes.size() && !protoMeshes[meshIdx].Indices.empty())
                        {
                            RasterBatch inst = protoMeshes[meshIdx];
                            inst.WorldMatrix = worldTransforms[i];
                            outBatches.push_back(std::move(inst));
                        }
                    }
                }
            }
            if (outBatches.empty())
            {
                for (auto& b : protoMeshes)
                    if (!b.Indices.empty()) outBatches.push_back(std::move(b));
            }
            return !outBatches.empty();
        }

        bool LoadPrefabAssetFallback(const std::filesystem::path& prefabPath, std::vector<RasterBatch>& outBatches, WorkerContext& ctx)
        {
            auto ar = OpenArchiveFromFile(prefabPath);
            if (!ar) return false;

            const std::filesystem::path prefabDir = prefabPath.parent_path();
            bool dummy = false;
            (void)ar->ReadObject("Prefab", dummy, [&](archive::IArchive& prefabObj, bool&) -> archive::ArchiveResult
                {
                    bool hasEntities = false;
                    if (!prefabObj.HasField("Entities", hasEntities) || !hasEntities) return {};

                    return prefabObj.ReadObjectArray("Entities", [&](archive::IArchive& entObj, std::size_t) -> archive::ArchiveResult
                        {
                            bool hasComps = false;
                            if (!entObj.HasField("Components", hasComps) || !hasComps) return {};

                            glm::mat4 worldMat(1.0f);
                            ResourceRefInfo meshRef, matRef;
                            bool hasRenderer = false;

                            (void)entObj.ReadObjectArray("Components", [&](archive::IArchive& compObj, std::size_t) -> archive::ArchiveResult
                                {
                                    std::string typeName;
                                    bool hasType = false, hasData = false;
                                    if (!compObj.HasField("Type", hasType) || !hasType || !compObj.Read("Type", typeName)) return {};
                                    if (!compObj.HasField("Data", hasData) || !hasData) return {};
                                    bool hasEnabled = false;
                                    bool enabled = true;
                                    if (!compObj.HasField("Enabled", hasEnabled) ||
                                        (hasEnabled && (!compObj.Read("Enabled", enabled) || !enabled))) return {};

                                    bool dDummy = false;
                                    (void)compObj.ReadObject("Data", dDummy, [&](archive::IArchive& dataObj, bool&) -> archive::ArchiveResult
                                        {
                                            if (typeName == "Engine::TransformComponent")
                                            {
                                                Engine::TransformComponent tr;
                                                bool f = false;
                                                std::array<float, 3> v3{};
                                                std::array<float, 4> v4{ 0, 0, 0, 1 };
                                                if (dataObj.HasField("position", f) && f && dataObj.ReadArray("position", v3))
                                                    tr.position = glm::vec3(v3[0], v3[1], v3[2]);
                                                if (dataObj.HasField("rotation", f) && f && dataObj.ReadArray("rotation", v4))
                                                    tr.rotation = glm::quat(v4[3], v4[0], v4[1], v4[2]);
                                                if (dataObj.HasField("scale", f) && f && dataObj.ReadArray("scale", v3))
                                                    tr.scale = glm::vec3(v3[0], v3[1], v3[2]);
                                                worldMat = tr.GetLocalMatrix();
                                            }
                                            else if (typeName == "Engine::StaticMeshRendererComponent" || typeName == "Engine::SkinnedMeshRendererComponent")
                                            {
                                                bool f = false;
                                                if (dataObj.HasField("meshHandle", f) && f)
                                                    (void)dataObj.ReadObject("meshHandle", meshRef, [](archive::IArchive& o, ResourceRefInfo& r) { return ReadResourceRefObject(o, r); });
                                                if (dataObj.HasField("materialHandle", f) && f)
                                                    (void)dataObj.ReadObject("materialHandle", matRef, [](archive::IArchive& o, ResourceRefInfo& r) { return ReadResourceRefObject(o, r); });
                                                hasRenderer = true;
                                            }
                                            return {};
                                        });
                                    return {};
                                });

                            if (hasRenderer && !meshRef.Null)
                            {
                                RasterBatch b;
                                bool meshOk = false;
                                if (auto* builtin = Engine::ResourceManager::Get().GetResourceHandle<Engine::Mesh>(meshRef.AssetKey).Get())
                                {
                                    meshOk = ExtractMeshToBatch(*builtin, b);
                                }
                                if (!meshOk)
                                {
                                    const auto meshPath = ResolveReferencePath(meshRef.AssetKey, meshRef.AssetPath, prefabDir);
                                    if (!meshPath.empty()) meshOk = LoadBamMeshArchive(meshPath, b);
                                }
                                if (meshOk)
                                {
                                    b.WorldMatrix = worldMat;
                                    if (!matRef.Null)
                                    {
                                        const auto matPath = ResolveReferencePath(matRef.AssetKey, matRef.AssetPath, prefabDir);
                                        if (!matPath.empty()) ApplyMaterialToBatch(LoadMaterialInfoCached(matPath, ctx), b);
                                    }
                                    outBatches.push_back(std::move(b));
                                }
                            }
                            return {};
                        });
                });

            return !outBatches.empty();
        }

        bool LoadPrefabAsset(const std::filesystem::path& prefabPath, std::vector<RasterBatch>& outBatches, WorkerContext& ctx)
        {
            auto ar = OpenArchiveFromFile(prefabPath);
            if (!ar) return false;

            std::unique_ptr<Engine::Scene, SceneDeleter> scene(Engine::Scene::Create());
            if (scene)
            {
                Engine::Entity* root = nullptr;
                const auto res = scene->InstantiatePrefabArchive(*ar, root, nullptr);
                if (res && root && Engine::TransformSystem::UpdateHierarchy(scene->GetRegistry()))
                {
                    auto& registry = scene->GetRegistry();
                    const auto dir = prefabPath.parent_path();

                    for (auto e : registry.view<Engine::StaticMeshRendererComponent, Engine::WorldTransformComponent>())
                    {
                        if (!Engine::IsComponentEnabled<Engine::StaticMeshRendererComponent>(registry, e)) continue;
                        const auto& mr = registry.get<Engine::StaticMeshRendererComponent>(e);
                        const auto& wt = registry.get<Engine::WorldTransformComponent>(e);
                        const auto* mesh = mr.meshHandle.Get();
                        if (!mesh) continue;

                        RasterBatch b;
                        if (ExtractMeshToBatch(*mesh, b))
                        {
                            b.WorldMatrix = wt.worldMatrix;
                            if (auto* mat = mr.materialHandle.Get()) ApplyMaterialToBatch(MaterialInfoFromInterface(mat, dir, ctx), b);
                            outBatches.push_back(std::move(b));
                        }
                    }

                    for (auto e : registry.view<Engine::SkinnedMeshRendererComponent, Engine::WorldTransformComponent>())
                    {
                        if (!Engine::IsComponentEnabled<Engine::SkinnedMeshRendererComponent>(registry, e)) continue;
                        const auto& smr = registry.get<Engine::SkinnedMeshRendererComponent>(e);
                        const auto& wt = registry.get<Engine::WorldTransformComponent>(e);
                        const auto* mesh = smr.meshHandle.Get();
                        if (!mesh) continue;

                        RasterBatch b;
                        if (ExtractMeshToBatch(*mesh, b))
                        {
                            b.WorldMatrix = wt.worldMatrix;
                            if (auto* mat = smr.materialHandle.Get()) ApplyMaterialToBatch(MaterialInfoFromInterface(mat, dir, ctx), b);
                            outBatches.push_back(std::move(b));
                        }
                    }

                    for (auto e : registry.view<Engine::SpriteRendererComponent, Engine::WorldTransformComponent>())
                    {
                        if (!Engine::IsComponentEnabled<Engine::SpriteRendererComponent>(registry, e)) continue;
                        const auto& sr = registry.get<Engine::SpriteRendererComponent>(e);
                        const auto& wt = registry.get<Engine::WorldTransformComponent>(e);
                        auto* mesh = sr.meshHandle.Get();
                        if (!mesh) mesh = Engine::ResourceManager::Get().GetResourceHandle<Engine::Mesh>(L"QuadMesh").Get();
                        if (!mesh) continue;

                        RasterBatch b;
                        if (ExtractMeshToBatch(*mesh, b))
                        {
                            b.WorldMatrix = wt.worldMatrix;
                            b.BaseColor = sr.color;
                            b.DoubleSided = true;
                            if (auto* sprite = sr.spriteHandle.Get())
                            {
                                if (auto* tex = sprite->GetTexture())
                                {
                                    const auto resolved = ResolveReferencePath(tex->GetKey(), tex->GetPath(), dir);
                                    if (!resolved.empty())
                                    {
                                        auto img = LoadTextureCached(resolved, ctx);
                                        b.AlbedoPixels = std::move(img.Pixels);
                                        b.AlbedoWidth = img.Width;
                                        b.AlbedoHeight = img.Height;
                                    }
                                }
                            }
                            outBatches.push_back(std::move(b));
                        }
                    }

                    if (!outBatches.empty()) return true;
                }
            }

            return LoadPrefabAssetFallback(prefabPath, outBatches, ctx);
        }

        glm::vec4 SampleTexture(const RasterBatch& batch, glm::vec2 uv)
        {
            if (batch.AlbedoPixels.empty() || batch.AlbedoWidth <= 0 || batch.AlbedoHeight <= 0)
                return glm::vec4(1.0f);

            const float u = uv.x - std::floor(uv.x);
            const float v = uv.y - std::floor(uv.y);
            const int x = std::clamp(static_cast<int>(u * batch.AlbedoWidth), 0, batch.AlbedoWidth - 1);
            const int y = std::clamp(static_cast<int>(v * batch.AlbedoHeight), 0, batch.AlbedoHeight - 1);
            const size_t idx = (static_cast<size_t>(y) * batch.AlbedoWidth + x) * 4;
            return glm::vec4(
                std::pow(batch.AlbedoPixels[idx + 0] / 255.0f, 2.2f),
                std::pow(batch.AlbedoPixels[idx + 1] / 255.0f, 2.2f),
                std::pow(batch.AlbedoPixels[idx + 2] / 255.0f, 2.2f),
                batch.AlbedoPixels[idx + 3] / 255.0f);
        }

        glm::vec3 AcesToneMap(const glm::vec3& x)
        {
            const float a = 2.51f, b = 0.03f, c = 2.43f, d = 0.59f, e = 0.14f;
            return glm::clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0f, 1.0f);
        }

        bool RenderBatchesToPng(const std::vector<RasterBatch>& batches, const std::filesystem::path& outputPngPath, int32_t resolution)
        {
            if (batches.empty() || resolution <= 0) return false;

            Engine::BoundingBox totalBounds;
            bool hasBounds = false;
            for (const auto& batch : batches)
            {
                for (const auto& v : batch.Vertices)
                {
                    const glm::vec3 wp = glm::vec3(batch.WorldMatrix * glm::vec4(v.Position, 1.0f));
                    if (!std::isfinite(wp.x) || !std::isfinite(wp.y) || !std::isfinite(wp.z)) continue;
                    totalBounds.min = hasBounds ? glm::min(totalBounds.min, wp) : wp;
                    totalBounds.max = hasBounds ? glm::max(totalBounds.max, wp) : wp;
                    hasBounds = true;
                }
            }
            if (!hasBounds || !totalBounds.IsValid()) return false;

            constexpr float fovDeg = 45.0f;
            constexpr float pitchDeg = 15.0f;
            constexpr float yawDeg = 160.0f;
            constexpr double margin = 1.05;

            const glm::dvec3 lower(totalBounds.min);
            const glm::dvec3 upper(totalBounds.max);
            const double radius = (std::max)(glm::length((upper - lower) * 0.5), 0.0001);
            const double halfFov = glm::radians(static_cast<double>(fovDeg) * 0.5);
            const float distance = static_cast<float>(radius * margin / std::sin(halfFov));
            const float nearPlane = (std::max)(distance * 0.01f, 0.001f);
            const float farPlane = distance * 15.0f;
            if (!std::isfinite(farPlane)) return false;

            const glm::vec3 center = glm::vec3((lower + upper) * 0.5);
            const glm::vec3 forward = glm::quat(glm::radians(glm::vec3(pitchDeg, yawDeg, 0.0f))) * glm::vec3(0.0f, 0.0f, 1.0f);
            const glm::vec3 eye = center - forward * distance;

            const glm::mat4 view = glm::lookAtLH(eye, center, glm::vec3(0.0f, 1.0f, 0.0f));
            const glm::mat4 proj = glm::perspectiveLH_ZO(glm::radians(fovDeg), 1.0f, nearPlane, farPlane);
            const glm::mat4 vp = proj * view;

            const int ssW = resolution * 2;
            const int ssH = resolution * 2;
            std::vector<glm::vec4> colorBuf(static_cast<size_t>(ssW) * ssH);
            std::vector<float> depthBuf(static_cast<size_t>(ssW) * ssH, 1.0f);

            for (int y = 0; y < ssH; ++y)
            {
                const float t = static_cast<float>(y) / static_cast<float>(ssH - 1);
                const glm::vec3 bg = glm::mix(glm::vec3(0.14f, 0.155f, 0.18f), glm::vec3(0.075f, 0.085f, 0.10f), t);
                for (int x = 0; x < ssW; ++x)
                    colorBuf[static_cast<size_t>(y) * ssW + x] = glm::vec4(bg, 1.0f);
            }

            // 스튜디오 3점 조명 (ThumbnailRenderer와 동일 방향성 구성)
            const glm::vec3 keyDir = glm::normalize(glm::vec3(0.45f, -0.65f, 0.60f));
            const glm::vec3 fillDir = glm::normalize(glm::vec3(-0.75f, -0.35f, 0.55f));
            const glm::vec3 rimDir = glm::normalize(glm::vec3(-0.20f, -0.50f, -0.84f));

            struct ProjVert
            {
                glm::vec4 Clip{ 0.0f };
                glm::vec3 Screen{ 0.0f };
                glm::vec3 WorldPos{ 0.0f };
                glm::vec3 WorldNorm{ 0.0f, 1.0f, 0.0f };
                glm::vec2 UV{ 0.0f };
                glm::vec4 Color{ 1.0f };
            };

            auto edgeFunc = [](const glm::vec2& a, const glm::vec2& b, const glm::vec2& c) -> float
                {
                    return (c.x - a.x) * (b.y - a.y) - (c.y - a.y) * (b.x - a.x);
                };

            for (const auto& batch : batches)
            {
                if (batch.Vertices.empty() || batch.Indices.size() < 3) continue;

                const glm::mat3 normalMat = glm::mat3(glm::transpose(glm::inverse(batch.WorldMatrix)));
                std::vector<ProjVert> pv(batch.Vertices.size());

                for (size_t i = 0; i < batch.Vertices.size(); ++i)
                {
                    const auto& src = batch.Vertices[i];
                    const glm::vec4 wp = batch.WorldMatrix * glm::vec4(src.Position, 1.0f);
                    const glm::vec4 clip = vp * wp;
                    pv[i].Clip = clip;
                    pv[i].WorldPos = glm::vec3(wp);
                    const glm::vec3 wn = normalMat * src.Normal;
                    const float nLen = glm::length(wn);
                    pv[i].WorldNorm = nLen > 1e-6f ? (wn / nLen) : glm::vec3(0.0f, 1.0f, 0.0f);
                    pv[i].UV = src.TexCoord;
                    pv[i].Color = src.Color;
                    if (clip.w > 1e-5f)
                    {
                        const float invW = 1.0f / clip.w;
                        pv[i].Screen.x = (clip.x * invW * 0.5f + 0.5f) * static_cast<float>(ssW);
                        pv[i].Screen.y = (0.5f - clip.y * invW * 0.5f) * static_cast<float>(ssH);
                        pv[i].Screen.z = clip.z * invW;
                    }
                }

                for (size_t t = 0; t + 2 < batch.Indices.size(); t += 3)
                {
                    const uint32_t i0 = batch.Indices[t + 0];
                    const uint32_t i1 = batch.Indices[t + 1];
                    const uint32_t i2 = batch.Indices[t + 2];
                    if (i0 >= pv.size() || i1 >= pv.size() || i2 >= pv.size()) continue;

                    const auto& v0 = pv[i0];
                    const auto& v1 = pv[i1];
                    const auto& v2 = pv[i2];
                    if (v0.Clip.w <= 1e-5f || v1.Clip.w <= 1e-5f || v2.Clip.w <= 1e-5f) continue;

                    const glm::vec2 p0(v0.Screen), p1(v1.Screen), p2(v2.Screen);
                    const float area = edgeFunc(p0, p1, p2);
                    if (std::abs(area) < 1e-4f) continue;
                    const float invArea = 1.0f / area;

                    const int minX = (std::max)(0, static_cast<int>(std::floor((std::min)({ p0.x, p1.x, p2.x }))));
                    const int maxX = (std::min)(ssW - 1, static_cast<int>(std::ceil((std::max)({ p0.x, p1.x, p2.x }))));
                    const int minY = (std::max)(0, static_cast<int>(std::floor((std::min)({ p0.y, p1.y, p2.y }))));
                    const int maxY = (std::min)(ssH - 1, static_cast<int>(std::ceil((std::max)({ p0.y, p1.y, p2.y }))));
                    if (minX > maxX || minY > maxY) continue;

                    const float w0Inv = 1.0f / v0.Clip.w;
                    const float w1Inv = 1.0f / v1.Clip.w;
                    const float w2Inv = 1.0f / v2.Clip.w;

                    for (int y = minY; y <= maxY; ++y)
                    {
                        for (int x = minX; x <= maxX; ++x)
                        {
                            const glm::vec2 p(static_cast<float>(x) + 0.5f, static_cast<float>(y) + 0.5f);
                            const float e0 = edgeFunc(p1, p2, p) * invArea;
                            const float e1 = edgeFunc(p2, p0, p) * invArea;
                            const float e2 = edgeFunc(p0, p1, p) * invArea;
                            if (e0 < 0.0f || e1 < 0.0f || e2 < 0.0f) continue;

                            const float z = e0 * v0.Screen.z + e1 * v1.Screen.z + e2 * v2.Screen.z;
                            const size_t pixIdx = static_cast<size_t>(y) * ssW + x;
                            if (z < 0.0f || z >= depthBuf[pixIdx]) continue;
                            depthBuf[pixIdx] = z;

                            const float pw = 1.0f / (e0 * w0Inv + e1 * w1Inv + e2 * w2Inv);
                            const float b0 = (e0 * w0Inv) * pw;
                            const float b1 = (e1 * w1Inv) * pw;
                            const float b2 = (e2 * w2Inv) * pw;

                            const glm::vec3 worldPos = v0.WorldPos * b0 + v1.WorldPos * b1 + v2.WorldPos * b2;
                            glm::vec3 N = glm::normalize(v0.WorldNorm * b0 + v1.WorldNorm * b1 + v2.WorldNorm * b2);
                            const glm::vec3 V = glm::normalize(eye - worldPos);
                            if (glm::dot(N, V) < 0.0f) N = -N;

                            const glm::vec2 uv = v0.UV * b0 + v1.UV * b1 + v2.UV * b2;
                            const glm::vec4 vCol = v0.Color * b0 + v1.Color * b1 + v2.Color * b2;
                            const glm::vec4 texCol = SampleTexture(batch, uv);

                            const glm::vec3 linearBase(
                                std::pow(std::clamp(batch.BaseColor.r, 0.0f, 1.0f), 2.2f),
                                std::pow(std::clamp(batch.BaseColor.g, 0.0f, 1.0f), 2.2f),
                                std::pow(std::clamp(batch.BaseColor.b, 0.0f, 1.0f), 2.2f));
                            const glm::vec3 albedo = linearBase * glm::vec3(vCol) * glm::vec3(texCol);

                            const float shininess = glm::mix(64.0f, 8.0f, std::clamp(batch.Roughness, 0.05f, 1.0f));
                            const float specStrength = glm::mix(0.25f, 0.85f, batch.Metallic) * (1.0f - batch.Roughness * 0.6f);

                            auto calcLight = [&](const glm::vec3& dir, const glm::vec3& col, bool spec) -> glm::vec3
                                {
                                    const glm::vec3 L = -dir;
                                    const float ndl = (std::max)(glm::dot(N, L), 0.0f);
                                    glm::vec3 lit = albedo * col * ndl;
                                    if (spec && ndl > 0.0f)
                                    {
                                        const glm::vec3 H = glm::normalize(L + V);
                                        const float s = std::pow((std::max)(glm::dot(N, H), 0.0f), shininess);
                                        lit += col * (specStrength * s);
                                    }
                                    return lit;
                                };

                            const glm::vec3 hemi = albedo * glm::mix(glm::vec3(0.08f, 0.09f, 0.11f), glm::vec3(0.22f, 0.25f, 0.30f), N.y * 0.5f + 0.5f);
                            const glm::vec3 total = hemi
                                + calcLight(keyDir, glm::vec3(1.85f, 1.80f, 1.72f), true)
                                + calcLight(fillDir, glm::vec3(0.70f, 0.78f, 0.92f), false)
                                + calcLight(rimDir, glm::vec3(0.85f, 0.90f, 1.05f), true);

                            const glm::vec3 mapped = AcesToneMap(total);
                            const glm::vec3 srgb(std::pow(mapped.r, 1.0f / 2.2f), std::pow(mapped.g, 1.0f / 2.2f), std::pow(mapped.b, 1.0f / 2.2f));
                            colorBuf[pixIdx] = glm::vec4(srgb, 1.0f);
                        }
                    }
                }
            }

            std::vector<uint8_t> outPixels(static_cast<size_t>(resolution) * resolution * 4);
            for (int y = 0; y < resolution; ++y)
            {
                for (int x = 0; x < resolution; ++x)
                {
                    const int sx = x * 2;
                    const int sy = y * 2;
                    const glm::vec4 c00 = colorBuf[static_cast<size_t>(sy) * ssW + sx];
                    const glm::vec4 c10 = colorBuf[static_cast<size_t>(sy) * ssW + sx + 1];
                    const glm::vec4 c01 = colorBuf[static_cast<size_t>(sy + 1) * ssW + sx];
                    const glm::vec4 c11 = colorBuf[static_cast<size_t>(sy + 1) * ssW + sx + 1];
                    const glm::vec4 avg = (c00 + c10 + c01 + c11) * 0.25f;

                    const size_t idx = (static_cast<size_t>(y) * resolution + x) * 4;
                    outPixels[idx + 0] = static_cast<uint8_t>(std::clamp(avg.r * 255.0f, 0.0f, 255.0f));
                    outPixels[idx + 1] = static_cast<uint8_t>(std::clamp(avg.g * 255.0f, 0.0f, 255.0f));
                    outPixels[idx + 2] = static_cast<uint8_t>(std::clamp(avg.b * 255.0f, 0.0f, 255.0f));
                    outPixels[idx + 3] = 255;
                }
            }

            std::error_code ec;
            const auto outDir = outputPngPath.parent_path();
            if (!outDir.empty() && !std::filesystem::exists(outDir, ec))
            {
                if (!std::filesystem::create_directories(outDir, ec)) return false;
            }

            const auto tid = std::hash<std::thread::id>{}(std::this_thread::get_id());
            const std::filesystem::path tmpPath = outputPngPath.native() + L".tmp_" +
                std::to_wstring(GetCurrentProcessId()) + L"_" + std::to_wstring(tid);

            std::vector<uint8_t> pngBytes;
            const auto writeCallback = [](void* context, void* data, int size)
                {
                    if (!context || !data || size <= 0) return;
                    auto* buf = static_cast<std::vector<uint8_t>*>(context);
                    const auto* src = static_cast<const uint8_t*>(data);
                    buf->insert(buf->end(), src, src + size);
                };

            if (stbi_write_png_to_func(writeCallback, &pngBytes, resolution, resolution, 4, outPixels.data(), resolution * 4) == 0 ||
                pngBytes.empty())
            {
                return false;
            }

            {
                std::ofstream outFile(tmpPath, std::ios::binary | std::ios::trunc);
                if (!outFile.is_open() || !outFile.write(reinterpret_cast<const char*>(pngBytes.data()), pngBytes.size()))
                {
                    std::filesystem::remove(tmpPath, ec);
                    return false;
                }
            }

            std::filesystem::rename(tmpPath, outputPngPath, ec);
            if (ec)
            {
                std::filesystem::remove(outputPngPath, ec);
                ec.clear();
                std::filesystem::rename(tmpPath, outputPngPath, ec);
                if (ec)
                {
                    std::filesystem::remove(tmpPath, ec);
                    return false;
                }
            }
            return true;
        }
    }

    bool HeadlessThumbnailRasterizer::BakeThumbnail(const std::filesystem::path& sourcePath, const std::filesystem::path& outputPngPath, int32_t resolution)
    {
        std::error_code ec;
        if (sourcePath.empty() || !std::filesystem::is_regular_file(sourcePath, ec)) return false;

        const std::string ext = PathExtLower(sourcePath);
        const std::string name = PathNameLower(sourcePath);

        WorkerContext ctx;
        std::vector<RasterBatch> batches;
        bool loaded = false;

        if (ext == ".bammesh" || name.ends_with(".bammesh.json"))
        {
            RasterBatch b;
            if (LoadBamMeshArchive(sourcePath, b))
            {
                batches.push_back(std::move(b));
                loaded = true;
            }
        }
        else if (ext == ".bammodel" || name.ends_with(".bammodel.json"))
        {
            loaded = LoadBamModelAsset(sourcePath, batches, ctx);
        }
        else if (ext == ".fbx" || ext == ".obj" || ext == ".gltf" || ext == ".glb")
        {
            loaded = LoadRawModelAsset(sourcePath, batches, ctx);
        }
        else if (ext == ".bamprefab" || ext == ".prefab" || name.ends_with(".bamprefab.json") || name.ends_with(".prefab.json"))
        {
            loaded = LoadPrefabAsset(sourcePath, batches, ctx);
        }

        if (!loaded || batches.empty()) return false;
        return RenderBatchesToPng(batches, outputPngPath, resolution);
    }
}
