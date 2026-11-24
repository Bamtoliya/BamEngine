#include "IBLManager.h"
#include "ResourceManager.h"
#include "Renderer.h"
#include "RHI.h"
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/packing.hpp>
#include <vector>
#include <cmath>
#include <cstring>
#include <limits>
#include <algorithm>
#include <thread>

namespace Engine
{
	namespace
	{
		float RadicalInverse_VdC(uint32 bits)
		{
			bits = (bits << 16u) | (bits >> 16u);
			bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
			bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
			bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
			bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
			return static_cast<float>(bits) * 2.3283064365386963e-10f; // / 0x100000000
		}

		vec2 Hammersley(uint32 i, uint32 N)
		{
			return vec2(static_cast<float>(i) / static_cast<float>(N), RadicalInverse_VdC(i));
		}

		vec3 ImportanceSampleGGX(vec2 Xi, vec3 N, float roughness)
		{
			float a = roughness * roughness;
			float phi = 2.0f * glm::pi<float>() * Xi.x;
			float cosTheta = std::sqrt((1.0f - Xi.y) / (1.0f + (a * a - 1.0f) * Xi.y));
			float sinTheta = std::sqrt(std::max(0.0f, 1.0f - cosTheta * cosTheta));

			vec3 H;
			H.x = std::cos(phi) * sinTheta;
			H.y = std::sin(phi) * sinTheta;
			H.z = cosTheta;

			vec3 up = std::abs(N.z) < 0.999f ? vec3(0.0f, 0.0f, 1.0f) : vec3(1.0f, 0.0f, 0.0f);
			vec3 tangent = glm::normalize(glm::cross(up, N));
			vec3 bitangent = glm::cross(N, tangent);

			vec3 sampleVec = tangent * H.x + bitangent * H.y + N * H.z;
			return glm::normalize(sampleVec);
		}

		float GeometrySchlickGGX(float NdotV, float roughness)
		{
			// IBL 기하 감쇄 k = alpha / 2 = roughness^2 / 2 (Karis 2013, Epic Games)
			float k = (roughness * roughness) / 2.0f;
			float nom = NdotV;
			float denom = NdotV * (1.0f - k) + k;
			return nom / std::max(denom, 0.0001f);
		}

		float GeometrySmith(float NdotV, float NdotL, float roughness)
		{
			float ggx2 = GeometrySchlickGGX(NdotV, roughness);
			float ggx1 = GeometrySchlickGGX(NdotL, roughness);
			return ggx2 * ggx1;
		}

		// DirectX LH 큐브맵 면(+X, -X, +Y, -Y, +Z, -Z)의 UV([-1, 1])에 대응하는 정규화 방향 벡터
		vec3 CubeFaceDirection(uint32 face, float u, float v)
		{
			vec3 dir(0.0f);
			switch (face)
			{
			case 0: dir = vec3( 1.0f,   -v,   -u); break; // +X
			case 1: dir = vec3(-1.0f,   -v,    u); break; // -X
			case 2: dir = vec3(    u, 1.0f,    v); break; // +Y
			case 3: dir = vec3(    u,-1.0f,   -v); break; // -Y
			case 4: dir = vec3(    u,   -v, 1.0f); break; // +Z
			case 5: dir = vec3(   -u,   -v,-1.0f); break; // -Z
			default: break;
			}
			return glm::normalize(dir);
		}

		vec3 SampleEquirectangularRGB(const float* srcRGB, uint32 srcWidth, uint32 srcHeight, const vec3& dir)
		{
			constexpr float kInvTwoPi = 0.15915494309189533577f;
			constexpr float kInvPi = 0.31830988618379067154f;

			const float phi = std::atan2(dir.z, dir.x);
			const float theta = std::asin(glm::clamp(dir.y, -1.0f, 1.0f));
			const float u = phi * kInvTwoPi + 0.5f;
			const float v = 0.5f - theta * kInvPi;

			const float fx = u * static_cast<float>(srcWidth) - 0.5f;
			const float fy = v * static_cast<float>(srcHeight) - 0.5f;

			const int32 x0 = static_cast<int32>(std::floor(fx));
			const int32 y0 = static_cast<int32>(std::floor(fy));
			const float tx = fx - static_cast<float>(x0);
			const float ty = fy - static_cast<float>(y0);

			const int32 w = static_cast<int32>(srcWidth);
			const int32 h = static_cast<int32>(srcHeight);
			const auto wrapX = [w](int32 x) -> uint32 { int32 r = x % w; return static_cast<uint32>(r < 0 ? r + w : r); };
			const auto clampY = [h](int32 y) -> uint32 { return static_cast<uint32>(std::clamp(y, 0, h - 1)); };

			const uint32 ux0 = wrapX(x0);
			const uint32 ux1 = wrapX(x0 + 1);
			const uint32 uy0 = clampY(y0);
			const uint32 uy1 = clampY(y0 + 1);

			const auto load = [srcRGB, srcWidth](uint32 x, uint32 y) -> vec3
			{
				const size_t idx = (static_cast<size_t>(y) * srcWidth + x) * 3;
				return vec3(srcRGB[idx], srcRGB[idx + 1], srcRGB[idx + 2]);
			};

			const vec3 c00 = load(ux0, uy0);
			const vec3 c10 = load(ux1, uy0);
			const vec3 c01 = load(ux0, uy1);
			const vec3 c11 = load(ux1, uy1);

			const vec3 c0 = glm::mix(c00, c10, tx);
			const vec3 c1 = glm::mix(c01, c11, tx);
			return glm::mix(c0, c1, ty);
		}

		void PackRGBA16F(const vec3& color, uint8* dst)
		{
			const auto sanitize = [](float v) -> float
			{
				if (!std::isfinite(v) || v <= 0.0f) return 0.0f;
				return (std::min)(v, 65504.0f);
			};
			const uint32 rg = glm::packHalf2x16(vec2(sanitize(color.r), sanitize(color.g)));
			const uint32 ba = glm::packHalf2x16(vec2(sanitize(color.b), 1.0f));
			std::memcpy(dst, &rg, sizeof(uint32));
			std::memcpy(dst + sizeof(uint32), &ba, sizeof(uint32));
		}
	}

	IBLManager& IBLManager::Get()
	{
		static IBLManager instance;
		return instance;
	}

	vec2 IBLManager::IntegrateBRDF(float NdotV, float roughness, uint32 numSamples)
	{
		NdotV = glm::clamp(NdotV, 0.0001f, 1.0f);
		roughness = glm::clamp(roughness, 0.0f, 1.0f);

		vec3 V;
		V.x = std::sqrt(std::max(0.0f, 1.0f - NdotV * NdotV));
		V.y = 0.0f;
		V.z = NdotV;

		float A = 0.0f;
		float B = 0.0f;

		const vec3 N = vec3(0.0f, 0.0f, 1.0f);

		for (uint32 i = 0; i < numSamples; ++i)
		{
			vec2 Xi = Hammersley(i, numSamples);
			vec3 H = ImportanceSampleGGX(Xi, N, roughness);
			vec3 L = glm::normalize(2.0f * glm::dot(V, H) * H - V);

			float NdotL = std::max(L.z, 0.0f);
			float NdotH = std::max(H.z, 0.0f);
			float VdotH = std::max(glm::dot(V, H), 0.0f);

			if (NdotL > 0.0f)
			{
				float G = GeometrySmith(NdotV, NdotL, roughness);
				float G_Vis = (G * VdotH) / (NdotH * NdotV);
				float Fc = std::pow(1.0f - VdotH, 5.0f);

				A += (1.0f - Fc) * G_Vis;
				B += Fc * G_Vis;
			}
		}

		return vec2(A / static_cast<float>(numSamples), B / static_cast<float>(numSamples));
	}

	bool IBLManager::ConvertEquirectangularToCubemap(const float* srcRGB, uint32 srcWidth, uint32 srcHeight,
		uint32 faceSize, bool generateMips, TextureBinaryHeader& outHeader, std::vector<uint8>& outPixels)
	{
		if (!srcRGB || srcWidth == 0 || srcHeight == 0 || faceSize == 0)
			return false;

		const uint32 mipLevels = (generateMips && faceSize > 1) ? Texture::CalculateMipLevels(faceSize, faceSize) : 1;
		constexpr uint32 bytesPerPixel = 8; // R16G16B16A16_FLOAT

		uint64 perFaceBytes = 0;
		for (uint32 m = 0; m < mipLevels; ++m)
		{
			const uint32 curSize = (std::max)(1u, faceSize >> m);
			perFaceBytes += static_cast<uint64>(curSize) * curSize * bytesPerPixel;
		}

		const uint64 totalBytes = perFaceBytes * 6;
		if (totalBytes == 0 || totalBytes > std::numeric_limits<uint32>::max())
			return false;

		outHeader = {};
		outHeader.width = faceSize;
		outHeader.height = faceSize;
		outHeader.depth = 1;
		outHeader.mipLevels = mipLevels;
		outHeader.arraySize = 6;
		outHeader.format = ETextureFormat::R16G16B16A16_FLOAT;
		outHeader.dimension = ETextureDimension::TextureCube;
		outHeader.dataSize = static_cast<uint32>(totalBytes);

		outPixels.resize(static_cast<size_t>(totalBytes));

		auto buildFace = [&](uint32 face)
		{
			std::vector<vec3> curMip(static_cast<size_t>(faceSize) * faceSize);
			const float invFaceSize = 1.0f / static_cast<float>(faceSize);
			for (uint32 y = 0; y < faceSize; ++y)
			{
				const float v = 2.0f * (static_cast<float>(y) + 0.5f) * invFaceSize - 1.0f;
				for (uint32 x = 0; x < faceSize; ++x)
				{
					const float u = 2.0f * (static_cast<float>(x) + 0.5f) * invFaceSize - 1.0f;
					const vec3 dir = CubeFaceDirection(face, u, v);
					curMip[static_cast<size_t>(y) * faceSize + x] = SampleEquirectangularRGB(srcRGB, srcWidth, srcHeight, dir);
				}
			}

			uint8* faceDst = outPixels.data() + static_cast<size_t>(face) * static_cast<size_t>(perFaceBytes);
			size_t mipOffset = 0;
			uint32 curSize = faceSize;

			for (uint32 m = 0; m < mipLevels; ++m)
			{
				uint8* mipDst = faceDst + mipOffset;
				const size_t pixelCount = static_cast<size_t>(curSize) * curSize;
				for (size_t i = 0; i < pixelCount; ++i)
					PackRGBA16F(curMip[i], mipDst + i * bytesPerPixel);

				mipOffset += pixelCount * bytesPerPixel;
				if (m + 1 >= mipLevels)
					break;

				const uint32 nextSize = (std::max)(1u, curSize >> 1);
				std::vector<vec3> nextMip(static_cast<size_t>(nextSize) * nextSize);
				for (uint32 y = 0; y < nextSize; ++y)
				{
					const uint32 sy0 = (std::min)(y * 2, curSize - 1);
					const uint32 sy1 = (std::min)(y * 2 + 1, curSize - 1);
					for (uint32 x = 0; x < nextSize; ++x)
					{
						const uint32 sx0 = (std::min)(x * 2, curSize - 1);
						const uint32 sx1 = (std::min)(x * 2 + 1, curSize - 1);
						const vec3 sum = curMip[static_cast<size_t>(sy0) * curSize + sx0]
							+ curMip[static_cast<size_t>(sy0) * curSize + sx1]
							+ curMip[static_cast<size_t>(sy1) * curSize + sx0]
							+ curMip[static_cast<size_t>(sy1) * curSize + sx1];
						nextMip[static_cast<size_t>(y) * nextSize + x] = sum * 0.25f;
					}
				}
				curMip = std::move(nextMip);
				curSize = nextSize;
			}
		};

		const uint32 numThreads = (std::min)(6u, (std::max)(1u, std::thread::hardware_concurrency()));
		if (numThreads <= 1 || faceSize < 64)
		{
			for (uint32 face = 0; face < 6; ++face)
				buildFace(face);
		}
		else
		{
			std::vector<std::thread> workers;
			workers.reserve(numThreads);
			for (uint32 t = 0; t < numThreads; ++t)
			{
				workers.emplace_back([&, t, numThreads]()
				{
					for (uint32 face = t; face < 6; face += numThreads)
						buildFace(face);
				});
			}
			for (auto& w : workers)
			{
				if (w.joinable()) w.join();
			}
		}

		return true;
	}

	Texture* IBLManager::GenerateBRDFLUT(uint32 size, RHI* rhi)
	{
		TextureBinaryHeader header{};
		header.width = size;
		header.height = size;
		header.depth = 1;
		header.mipLevels = 1;
		header.arraySize = 1;
		header.format = ETextureFormat::R16G16_FLOAT;
		header.dimension = ETextureDimension::Texture2D;
		header.dataSize = size * size * sizeof(uint32); // R16G16 = 4 bytes per pixel

		std::vector<uint8> pixelData(header.dataSize);
		uint32* dst = reinterpret_cast<uint32*>(pixelData.data());

		// CPU 멀티스레드 병렬 연산을 통한 256x256 Karis 적분 고속화 (가용 하드웨어 스레드 단위 행 분할)
		const uint32 numThreads = std::max(1u, std::thread::hardware_concurrency());
		std::vector<std::thread> workers;
		workers.reserve(numThreads);

		const uint32 rowsPerThread = (size + numThreads - 1) / numThreads;

		for (uint32 threadIdx = 0; threadIdx < numThreads; ++threadIdx)
		{
			const uint32 startY = threadIdx * rowsPerThread;
			const uint32 endY = std::min(startY + rowsPerThread, size);
			if (startY >= endY) break;

			workers.emplace_back([size, startY, endY, dst]()
			{
				for (uint32 y = startY; y < endY; ++y)
				{
					const float roughness = (static_cast<float>(y) + 0.5f) / static_cast<float>(size);
					for (uint32 x = 0; x < size; ++x)
					{
						const float NdotV = (static_cast<float>(x) + 0.5f) / static_cast<float>(size);
						vec2 integrated = IntegrateBRDF(NdotV, roughness, 1024);
						dst[y * size + x] = glm::packHalf2x16(integrated);
					}
				}
			});
		}

		for (auto& worker : workers)
		{
			if (worker.joinable())
				worker.join();
		}

		const std::wstring uri = L"Engine://Texture/DefaultBRDFLut";
		if (!rhi)
			return Texture::Create(header, pixelData, uri);

		RHITextureDesc desc{};
		desc.width = size;
		desc.height = size;
		desc.depth = 1;
		desc.mipLevels = 1;
		desc.arraySize = 1;
		desc.format = ETextureFormat::R16G16_FLOAT;
		desc.dimension = ETextureDimension::Texture2D;
		desc.usage = ETextureUsage::Sampler;
		desc.data = pixelData.data();
		desc.dataSize = header.dataSize;

		RHITexture* rhiTex = rhi->CreateTexture(desc);
		if (!rhiTex)
			return nullptr;

		Texture* tex = Texture::CreateFromRHITexture(rhiTex, uri);
		tex->SetTempData(pixelData);
		return tex;
	}

	Texture* IBLManager::GetDefaultBRDFLUT()
	{
		if (!Renderer::IsCreated())
		{
			if (!m_HeadlessBRDFLUT)
				m_HeadlessBRDFLUT.reset(GenerateBRDFLUT(256, nullptr));
			return m_HeadlessBRDFLUT.get();
		}

		RHI* rhi = Renderer::Get().GetRHI();
		if (!rhi || !ResourceManager::IsCreated())
			return nullptr;

		if (auto* cached = m_DefaultBRDFLUTHandle.Get(); cached && cached->GetRHITexture())
			return cached;

		const std::wstring uri = L"Engine://Texture/DefaultBRDFLut";
		auto& resources = ResourceManager::Get();
		auto handle = resources.GetResourceHandle<Texture>(uri);
		if (!handle.Get())
		{
			auto* generated = GenerateBRDFLUT(256, rhi);
			if (!generated)
				return nullptr;
			handle = resources.AddResource<Texture>(uri, generated);
		}

		auto* texture = handle.Get();
		if (!texture || !texture->GetRHITexture())
			return nullptr;

		m_DefaultBRDFLUTHandle = handle;
		return texture;
	}

	void IBLManager::Free()
	{
		m_DefaultBRDFLUTHandle.Reset();
		m_HeadlessBRDFLUT.reset();
	}
}
