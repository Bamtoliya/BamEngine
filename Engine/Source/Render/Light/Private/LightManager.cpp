#pragma once

#include "LightManager.h"
#include "Light.h"
#include "RHIBuffer.h"

IMPLEMENT_SINGLETON(LightManager)
#pragma region Constructor&Destructor
EResult LightManager::Initialize(void* arg)
{
	if (!arg) return EResult::InvalidArgument;
	CAST_DESC
		m_RHI = desc->RHI;
	Safe_AddRef(m_RHI);

	if (IsFailure(EnsureBuffer()))
		return EResult::Fail;

	return EResult::Success;
}

void LightManager::Free()
{
	__super::Free();
	for (LightSource* lightSource : m_LightSources)
	{
		Safe_Release(lightSource);
	}
	m_LightSources.clear();
	Safe_Release(m_LightBuffer);
	Safe_Release(m_RHI);
}
#pragma endregion

#pragma region Loop
void LightManager::Update(f32 dt)
{
	ClearLightSources();
}
#pragma endregion

#pragma region Light Management
EResult LightManager::AddLightSource(LightSource* lightSource)
{
	if (!lightSource) return EResult::InvalidArgument;
	Safe_AddRef(lightSource);
	m_LightSources.push_back(lightSource);
	return EResult::Success;
}

EResult LightManager::RemoveLightSource(LightSource* lightSource)
{
	if (!lightSource) return EResult::InvalidArgument;
	auto it = std::find(m_LightSources.begin(), m_LightSources.end(), lightSource);
	if (it != m_LightSources.end())
	{
		Safe_Release(*it);
		m_LightSources.erase(it);
		return EResult::Success;
	}
	return EResult::Fail;
}

EResult LightManager::ClearLightSources()
{
	for (LightSource* lightSource : m_LightSources)
	{
		Safe_Release(lightSource);
	}
	m_LightSources.clear();
	return EResult::Success;
}
#pragma endregion

#pragma region GPU
EResult LightManager::Bind(uint32 slot)
{
	if (!m_RHI) return EResult::Fail;
	if (IsFailure(EnsureBuffer())) return EResult::Fail;
	if (IsFailure(UpdateLightBuffer())) return EResult::Fail;
	if (!m_LightBuffer) return EResult::Fail;

	return m_RHI->BindFragmentStorageBuffers(slot, &m_LightBuffer, 1);
}
EResult LightManager::UpdateLightBuffer()
{
	if (IsFailure(EnsureBuffer())) return EResult::Fail;

	const uint32 headerSize = sizeof(tagLightBufferHeader);
	const uint32 totalSize = headerSize + sizeof(tagGPULight) * MAX_LIGHTS;

	vector<uint8> data(totalSize, 0);

	auto* header = reinterpret_cast<tagLightBufferHeader*>(data.data());
	auto* gpuLights = reinterpret_cast<tagGPULight*>(data.data() + headerSize);

	// 0개여도 NumLights = 0으로 정상 빌드 → 셰이더 루프 0회
	const uint32 lightCount = static_cast<uint32>(
		glm::min(static_cast<int>(m_LightSources.size()), MAX_LIGHTS));
	header->NumLights = lightCount;

	for (uint32 i = 0; i < lightCount; ++i)
		gpuLights[i] = m_LightSources[i]->BuildGPULightDesc();

	m_LightBuffer->SetData(data.data(), totalSize);
	return EResult::Success;
}
EResult LightManager::EnsureBuffer()
{
	// 이미 할당된 크기가 충분하면 재생성 생략
	if (m_LightBuffer && m_LightCount >= MAX_LIGHTS)
		return EResult::Success;

	Safe_Release(m_LightBuffer);

	const uint32 totalSize = sizeof(tagLightBufferHeader) + sizeof(tagGPULight) * MAX_LIGHTS;
	m_LightBuffer = m_RHI->CreateBuffer(nullptr, totalSize, sizeof(tagGPULight), ERHIBufferType::Structured);
	if (!m_LightBuffer) return EResult::Fail;

	m_LightCount = MAX_LIGHTS;

	// 초기 데이터: NumLights = 0 으로 클리어
	vector<uint8> zeros(totalSize, 0);
	m_LightBuffer->SetData(zeros.data(), totalSize);

	return EResult::Success;
}
#pragma endregion