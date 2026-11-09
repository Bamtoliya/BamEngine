#pragma once

#include "LightManager.h"
#include "Light.h"
#include "RHIBuffer.h"
#include "RHI.h"
#include "SceneManager.h"
#include "Scene.h"
#include "LightComponent.h"
#include "ResourceManager.h"
#include "Shader.h"
#include "RHIPipeline.h"
#include "RHITexture.h"

IMPLEMENT_SINGLETON(LightManager)
#pragma region Constructor&Destructor
EResult LightManager::Initialize(void* arg)
{
	if (!arg) return EResult::InvalidArgument;
	CAST_DESC
	if (!desc->RHI) return EResult::InvalidArgument;
	m_RHI = desc->RHI;
	Safe_AddRef(m_RHI);

	if (IsFailure(EnsureBuffer()))
	{
		Free();
		return EResult::Fail;
	}		

	return EResult::Success;
}

void LightManager::Free()
{
	__super::Free();

	for (auto& buffer : m_LightBuffers)
		Safe_Release(buffer);

	Safe_Release(m_TileLightGridBuffer);
	Safe_Release(m_TileLightIndexBuffer);
	Safe_Release(m_TiledCullingPipeline);

	Safe_Release(m_RHI);
}
#pragma endregion

#pragma region Loop
void LightManager::Update(f32 dt)
{
	(void)dt;

	for (auto& dirty : m_BufferDirty)
		dirty = true;
}
#pragma endregion

#pragma region GPU
EResult LightManager::Bind(uint32 slot)
{
	if (IsFailure(EnsureBuffer())) return EResult::Fail;

	const uint32 frameIndex = m_RHI->GetFrameResourceIndex();
	if (m_BufferDirty[frameIndex] && IsFailure(UpdateLightBuffer()))
		return EResult::Fail;

	return m_RHI->BindStorageBuffer(m_LightBuffers[frameIndex], slot);
}

EResult LightManager::UpdateLightBuffer(Scene* targetScene)
{
	if (IsFailure(EnsureBuffer())) return EResult::Fail;
	const uint32 frameIndex = m_RHI->GetFrameResourceIndex();
	constexpr uint32 headerSize = sizeof(LightBufferHeader);
	constexpr uint32 totalSize = headerSize + sizeof(GPULight) * MAX_LIGHTS;
	std::array<uint8, totalSize> data{};
	auto* header = reinterpret_cast<LightBufferHeader*>(data.data());
	auto* gpuLights = reinterpret_cast<GPULight*>(data.data() + headerSize);
	uint32 currentLightIndex = 0;
	m_ShadowCameraBuffers.clear();
	m_ShadowData.clear();
	m_HasSpotShadow = false;
	m_HasPointShadow = false;
	m_SpotShadowParams = vec4(0.0f);
	m_PointShadowParams = vec4(0.0f);
	m_PointShadowPosRange = vec4(0.0f);
	std::vector<Scene*> scenesToQuery;
	if (targetScene)
	{
		scenesToQuery.push_back(targetScene);
	}
	else if (SceneManager::IsCreated())
	{
		scenesToQuery = SceneManager::Get().GetActiveScenes();
	}
	for (Scene* scene : scenesToQuery)
	{
		if (!scene) continue;
		auto view = scene->GetRegistry().view<LightComponent>();
		for (const auto entity : view)
		{
			if (currentLightIndex >= MAX_LIGHTS) break;
			const auto& light = view.get<LightComponent>(entity);
			const auto optGpuLight = light.GetGPULightDesc();
			if (optGpuLight.has_value())
			{
				gpuLights[currentLightIndex++] = optGpuLight.value();
			}
			if (light.LightType == ELightType::Directional)
			{
				if (auto optShadowData = light.GetShadowData())
				{
					std::array<CameraBuffer, MAX_SHADOW_CASCADES> cascadeCams{};
					for (uint32 c = 0; c < MAX_SHADOW_CASCADES; ++c)
					{
						if (auto optShadowCam = light.GetShadowCameraBuffer(c))
							cascadeCams[c] = optShadowCam.value();
					}
					m_ShadowCameraBuffers.push_back(cascadeCams);
					m_ShadowData.push_back(optShadowData.value());
				}
			}
			else if (light.LightType == ELightType::Spot && !m_HasSpotShadow && HasFlag(light.Flags, ELightFlags::CastShadows))
			{
				if (auto optCam = light.GetShadowCameraBuffer(0))
				{
					m_SpotShadowCameraBuffer = optCam.value();
					m_SpotShadowViewProj = optCam.value().viewProjMatrix;
					m_SpotShadowParams = light.GetShadowParams();
					m_HasSpotShadow = true;
				}
			}
			else if (light.LightType == ELightType::Point && !m_HasPointShadow && HasFlag(light.Flags, ELightFlags::CastShadows))
			{
				bool allFacesCollected = true;
				for (uint32 f = 0; f < 6; ++f)
				{
					if (auto optCam = light.GetPointShadowCameraBuffer(f))
					{
						m_PointShadowCameraBuffers[f] = optCam.value();
					}
					else
					{
						allFacesCollected = false;
						break;
					}
				}
				if (allFacesCollected)
				{
					if (optGpuLight.has_value())
						m_PointShadowPosRange = vec4(optGpuLight->Position, glm::max(optGpuLight->Range, 0.1f));
					m_PointShadowParams = vec4(light.ShadowBias, light.ShadowSlopeBias, light.ShadowNormalBias, 1.0f);
					m_HasPointShadow = true;
				}
			}
		}
	}
	header->NumLights = currentLightIndex;
	m_LightBuffers[frameIndex]->SetData(data.data(), totalSize);
	m_BufferDirty[frameIndex] = false;
	return EResult::Success;
}
EResult LightManager::EnsureBuffer()
{
	if (!m_RHI) return EResult::Fail;

	const uint32 frameCount = m_RHI->GetFrameResourceCount();
	const uint32 frameIndex = m_RHI->GetFrameResourceIndex();
	if (frameCount == 0 || frameCount > MAX_SWAPCHAIN_BUFFERS || frameIndex >= frameCount)
		return EResult::Fail;

	constexpr uint32 totalSize = sizeof(LightBufferHeader) + sizeof(GPULight) * MAX_LIGHTS;

	for (uint32 i = 0; i < frameCount; ++i)
	{
		if (m_LightBuffers[i]) continue;

		std::array<uint8, totalSize> zeros{};
		m_LightBuffers[i] = m_RHI->CreateBuffer(zeros.data(), totalSize, sizeof(GPULight), ERHIBufferType::Structured);
		if (!m_LightBuffers[i]) return EResult::Fail;

		m_BufferDirty[i] = true;
	}

	return EResult::Success;
}

CameraBuffer LightManager::GetShadowCameraBuffer(uint32 cascadeIndex, uint32 lightIndex) const
{
	if (lightIndex < m_ShadowCameraBuffers.size() && cascadeIndex < MAX_SHADOW_CASCADES)
		return m_ShadowCameraBuffers[lightIndex][cascadeIndex];
	return CameraBuffer{};
}

CameraBuffer LightManager::GetPointShadowCameraBuffer(uint32 faceIndex) const
{
	if (faceIndex < 6)
		return m_PointShadowCameraBuffers[faceIndex];
	return CameraBuffer{};
}

LightShadowData LightManager::GetShadowData(uint32 lightIndex) const
{
	LightShadowData data{};
	if (lightIndex < m_ShadowData.size()) data = m_ShadowData[lightIndex];

	if (m_HasSpotShadow)
	{
		data.SpotViewProjMatrix = m_SpotShadowViewProj;
		data.SpotShadowParams = m_SpotShadowParams;
	}

	if (m_HasPointShadow)
	{
		data.PointShadowParams = m_PointShadowParams;
		data.PointShadowPosRange = m_PointShadowPosRange;
	}

	return data;
}
#pragma endregion

#pragma region Tiled Lighting
EResult LightManager::EnsureTileBuffers(uint32 width, uint32 height)
{
	if (!m_RHI || width == 0 || height == 0) return EResult::InvalidArgument;

	const uint32 tilesX = (width + 15) / 16;
	const uint32 tilesY = (height + 15) / 16;

	if (m_TileLightGridBuffer && m_TileLightIndexBuffer && m_TileCountX >= tilesX && m_TileCountY >= tilesY)
		return EResult::Success;

	m_TileCountX = tilesX;
	m_TileCountY = tilesY;
	const uint32 totalTiles = tilesX * tilesY;

	Safe_Release(m_TileLightGridBuffer);
	Safe_Release(m_TileLightIndexBuffer);

	// Grid Buffer: 타일당 uint2 (8 bytes)
	const uint32 gridBufferSize = totalTiles * sizeof(uint32) * 2;
	m_TileLightGridBuffer = m_RHI->CreateBuffer(nullptr, gridBufferSize, sizeof(uint32) * 2, ERHIBufferType::Storage);

	// Index List Buffer: 타일당 MAX_LIGHTS_PER_TILE(256) * uint (4 bytes)
	const uint32 indexBufferSize = totalTiles * 256 * sizeof(uint32);
	m_TileLightIndexBuffer = m_RHI->CreateBuffer(nullptr, indexBufferSize, sizeof(uint32), ERHIBufferType::Storage);

	if (!m_TileLightGridBuffer || !m_TileLightIndexBuffer)
	{
		ENGINE_LOG_ERROR("Failed to create tile light buffers.");
		return EResult::Fail;
	}

	return EResult::Success;
}

EResult LightManager::EnsureTiledCullingPipeline()
{
	if (m_TiledCullingPipeline) return EResult::Success;

	auto shaderHandle = ResourceManager::Get().GetResourceHandle<Shader>(L"Resources/Shader/tiledLightCullingCS");
	Shader* csShader = shaderHandle.Get();
	if (!csShader || !csShader->GetRHIShader())
	{
		ENGINE_LOG_ERROR("tiledLightCullingCS shader is not loaded.");
		return EResult::Fail;
	}

	RHIPipelineDesc pipelineDesc{};
	pipelineDesc.pipelineType = EPipelineType::Compute;
	pipelineDesc.computeShader = csShader->GetRHIShader();

	m_TiledCullingPipeline = m_RHI->CreatePipeline(pipelineDesc);
	if (!m_TiledCullingPipeline)
	{
		ENGINE_LOG_ERROR("Failed to create tiled light culling compute pipeline.");
		return EResult::Fail;
	}

	return EResult::Success;
}

EResult LightManager::DispatchLightCulling(const CameraBuffer& cameraBuffer, RHITexture* positionTexture)
{
	if (!m_RHI || !positionTexture) return EResult::InvalidArgument;

	if (IsFailure(EnsureTiledCullingPipeline()))
		return EResult::Fail;

	const uint32 rtWidth = positionTexture->GetWidth();
	const uint32 rtHeight = positionTexture->GetHeight();
	if (IsFailure(EnsureTileBuffers(rtWidth, rtHeight)))
		return EResult::Fail;

	// 1. Compute 파이프라인 바인딩
	if (IsFailure(m_RHI->BindPipeline(m_TiledCullingPipeline)))
		return EResult::Fail;

	// 2. 카메라 상수 버퍼 바인딩 (b0, space0)
	if (IsFailure(m_RHI->BindConstantBuffer(&cameraBuffer, sizeof(CameraBuffer), 0)))
		return EResult::Fail;

	// 3. 텍스처(gPosition: register(t4, space3)) 및 광원 버퍼 바인딩
	if (IsFailure(m_RHI->BindTexture(positionTexture, MAX_MATERIAL_TEXTURE_SLOTS + 4)))
		return EResult::Fail;

	const uint32 frameIndex = m_RHI->GetFrameResourceIndex();
	if (m_BufferDirty[frameIndex] && IsFailure(UpdateLightBuffer()))
		return EResult::Fail;
	if (IsFailure(m_RHI->BindStorageBuffer(m_LightBuffers[frameIndex], 0)))
		return EResult::Fail;

	// 4. 출력 UAV 버퍼 바인딩 (u0, space3 및 u1, space3)
	if (IsFailure(m_RHI->BindComputeStorageBuffer(m_TileLightGridBuffer, 0)))
		return EResult::Fail;
	if (IsFailure(m_RHI->BindComputeStorageBuffer(m_TileLightIndexBuffer, 1)))
		return EResult::Fail;

	// 5. 디스패치 실행 (16x16 픽셀 타일 격자)
	// 셰이더의 16×16 타일 크기와 일치해야 합니다.
	constexpr uint32 tileSize = 16u;
	const uint32 dispatchTilesX = (rtWidth + tileSize - 1u) / tileSize;
	const uint32 dispatchTilesY = (rtHeight + tileSize - 1u) / tileSize;

	if (IsFailure(m_RHI->Dispatch(dispatchTilesX, dispatchTilesY, 1)))
		return EResult::Fail;

	// 6. UAV 배리어 동기화 (컴퓨트 쓰기 완료 후 픽셀 셰이더 읽기 전환)
	m_RHI->ResourceBarrierUAV(m_TileLightGridBuffer);
	m_RHI->ResourceBarrierUAV(m_TileLightIndexBuffer);

	// 7. 컴퓨트 파이프라인 바인딩 해제 (후속 그래픽스 패스 복원 보장)
	m_RHI->InvalidatePipelineBinding(m_TiledCullingPipeline);

	return EResult::Success;
}
#pragma endregion