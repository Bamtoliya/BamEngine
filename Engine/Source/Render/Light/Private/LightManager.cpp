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
}
#pragma endregion

#pragma region GPU
EResult LightManager::Bind(uint32 slot)
{
	if (!m_RHI) return EResult::Fail;
	if (IsFailure(EnsureBuffer())) return EResult::Fail;
	if (IsFailure(UpdateLightBuffer())) return EResult::Fail;
	if (!m_LightBuffer) return EResult::Fail;

	return m_RHI->BindStorageBuffer(m_LightBuffer, slot);
}

EResult LightManager::UpdateLightBuffer()
{
	if (IsFailure(EnsureBuffer())) return EResult::Fail;

	const uint32 headerSize = sizeof(LightBufferHeader);
	const uint32 totalSize = headerSize + sizeof(GPULight) * MAX_LIGHTS;

	vector<uint8> data(totalSize, 0);

	auto* header = reinterpret_cast<LightBufferHeader*>(data.data());
	auto* gpuLights = reinterpret_cast<GPULight*>(data.data() + headerSize);

	uint32 currentLightIndex = 0;
	m_ShadowCameraBuffers.clear();
	m_ShadowData.clear();

	// ECS Scene의 LightComponent 수집
	if (SceneManager::IsCreated())
	{
		for (Scene* scene : SceneManager::Get().GetActiveScenes())
		{
			if (!scene) continue;
			auto view = scene->GetRegistry().view<LightComponent>();
			for (auto entity : view)
			{
				if (currentLightIndex >= MAX_LIGHTS) break;
				const auto& light = view.get<LightComponent>(entity);
				if (auto optGpuLight = light.GetGPULightDesc())
				{
					gpuLights[currentLightIndex++] = optGpuLight.value();
				}
				if (auto optShadowCam = light.GetShadowCameraBuffer())
				{
					m_ShadowCameraBuffers.push_back(optShadowCam.value());
				}
				if (auto optShadowData = light.GetShadowData())
				{
					m_ShadowData.push_back(optShadowData.value());
				}
			}
		}
	}

	header->NumLights = currentLightIndex;
	m_LightBuffer->SetData(data.data(), totalSize);
	return EResult::Success;
}
EResult LightManager::EnsureBuffer()
{
	// 이미 할당된 크기가 충분하면 재생성 생략
	if (m_LightBuffer && m_LightCount >= MAX_LIGHTS)
		return EResult::Success;

	Safe_Release(m_LightBuffer);

	const uint32 totalSize = sizeof(LightBufferHeader) + sizeof(GPULight) * MAX_LIGHTS;
	m_LightBuffer = m_RHI->CreateBuffer(nullptr, totalSize, sizeof(GPULight), ERHIBufferType::Structured);
	if (!m_LightBuffer) return EResult::Fail;

	m_LightCount = MAX_LIGHTS;

	// 초기 데이터: NumLights = 0 으로 클리어
	vector<uint8> zeros(totalSize, 0);
	m_LightBuffer->SetData(zeros.data(), totalSize);

	return EResult::Success;
}
CameraBuffer LightManager::GetShadowCameraBuffer(uint32 shadowLightIndex) const
{
	if (shadowLightIndex < m_ShadowCameraBuffers.size())
	{
		return m_ShadowCameraBuffers[shadowLightIndex];
	}
}
LightShadowData LightManager::GetShadowData(uint32 shadowLightIndex) const
{
	if (shadowLightIndex < m_ShadowData.size())
	{
		return m_ShadowData[shadowLightIndex];
	}
}
#pragma endregion