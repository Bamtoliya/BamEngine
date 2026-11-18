#include "IBLManager.h"
#include "ResourceManager.h"
#include "Renderer.h"
#include "RHI.h"
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/packing.hpp>
#include <vector>
#include <cmath>
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

	Texture* IBLManager::GenerateBRDFLUT(uint32 size)
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
		RHI* rhi = Renderer::IsCreated() ? Renderer::Get().GetRHI() : nullptr;
		if (rhi)
		{
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
			if (rhiTex)
			{
				Texture* tex = Texture::CreateFromRHITexture(rhiTex, uri);
				if (tex)
				{
					tex->SetTempData(pixelData);
					return tex;
				}
			}
		}

		Texture* tex = Texture::Create(header, pixelData, uri);
		return tex;
	}

	Texture* IBLManager::GetDefaultBRDFLUT()
	{
		if (m_DefaultBRDFLUT)
			return m_DefaultBRDFLUT;

		const std::wstring uri = L"Engine://Texture/DefaultBRDFLut";

		if (ResourceManager::IsCreated())
		{
			auto& rm = ResourceManager::Get();
			auto handle = rm.GetResourceHandle<Texture>(uri);
			if (handle.IsValid() && handle.Get())
			{
				m_DefaultBRDFLUTHandle = handle;
				m_DefaultBRDFLUT = handle.Get();
				return m_DefaultBRDFLUT;
			}
		}

		Texture* created = GenerateBRDFLUT(256);
		if (created)
		{
			m_DefaultBRDFLUT = created;
			if (ResourceManager::IsCreated())
			{
				m_DefaultBRDFLUTHandle = ResourceManager::Get().AddResource<Texture>(uri, created);
			}
		}
		return m_DefaultBRDFLUT;
	}

	void IBLManager::Free()
	{
		m_DefaultBRDFLUTHandle = {};
		if (m_DefaultBRDFLUT)
		{
			if (!ResourceManager::IsCreated())
			{
				m_DefaultBRDFLUT->Free();
				delete m_DefaultBRDFLUT;
			}
			m_DefaultBRDFLUT = nullptr;
		}
	}
}
