#pragma once

#include "LightManager.h"
#include "Light.h"
#include "RHIBuffer.h"
#include "RHI.h"
#include "SceneManager.h"
#include "Scene.h"
#include "Components/Render/LightComponent.h"

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
	Safe_Release(m_LightBuffer);
	Safe_Release(m_RHI);
}
#pragma endregion

#pragma region Loop
void LightManager::Update(f32 dt)
{
	(void)dt;
	m_BufferDirty = true;
}
#pragma endregion

#pragma region GPU
EResult LightManager::Bind(uint32 slot)
{
	if (!m_RHI) return EResult::Fail;
	if (IsFailure(EnsureBuffer())) return EResult::Fail;
	if (m_BufferDirty && IsFailure(UpdateLightBuffer())) return EResult::Fail;
	if (!m_LightBuffer) return EResult::Fail;

	return m_RHI->BindStorageBuffer(m_LightBuffer, slot);
}

EResult LightManager::UpdateLightBuffer()
{
	if (IsFailure(EnsureBuffer())) return EResult::Fail;

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

	// ECS Scene의 LightComponent 수집
	if (SceneManager::IsCreated())
	{
		for (Scene* scene : SceneManager::Get().GetActiveScenes())
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
	}

	header->NumLights = currentLightIndex;
	m_LightBuffer->SetData(data.data(), totalSize);
	m_BufferDirty = false;
	return EResult::Success;
}
EResult LightManager::EnsureBuffer()
{
	// 이미 할당된 크기가 충분하면 재생성 생략
	if (m_LightBuffer && m_LightCount >= MAX_LIGHTS)
		return EResult::Success;

	Safe_Release(m_LightBuffer);

	constexpr uint32 totalSize = sizeof(LightBufferHeader) + sizeof(GPULight) * MAX_LIGHTS;
	m_LightBuffer = m_RHI->CreateBuffer(nullptr, totalSize, sizeof(GPULight), ERHIBufferType::Structured);
	if (!m_LightBuffer) return EResult::Fail;

	m_LightCount = MAX_LIGHTS;

	// 초기 데이터: NumLights = 0 으로 클리어
	std::array<uint8, totalSize> zeros{};
	m_LightBuffer->SetData(zeros.data(), totalSize);
	m_BufferDirty = true;

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