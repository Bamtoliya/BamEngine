#pragma once

#include "Shader.h"
#include "Renderer.h"
#include "RHIShader.h"
#include "Logger.h"
#include "PipelineManager.h"

#pragma push_macro("new")
#undef new

#include <algorithm>
#include <cwctype>
#include <filesystem>

#pragma pop_macro("new")

#pragma region Constructor&Destructor
EResult Shader::Initialize(void* arg)
{
	if (IsFailure(__super::Initialize(arg))) return EResult::Fail;
	CAST_DESC
	
	m_ShaderType = desc->shaderType;
	m_EntryPoint = desc->entryPoint;
	m_DxilPath = desc->dxilPath;
	m_SpirvPath = desc->spirvPath;
	ResolveLegacyBinaryPaths();
	m_NumSamplers = desc->numSamplers;
	m_NumStorageTextures = desc->numStorageTextures;
	m_NumStorageBuffers = desc->numStorageBuffers;
	m_NumUniformBuffers = desc->numUniformBuffers;

	m_UniformBuffers = desc->uniformBuffers;
	m_StorageBuffers = desc->storageBuffers;
	m_Samplers = desc->samplers;
	m_StorageTextures = desc->storageTextures;
	m_StageInputs = desc->stageInputs;
	m_StageOutputs = desc->stageOutputs;


	RHI* rhi = Renderer::IsCreated() ? Renderer::Get().GetRHI() : nullptr;
	if (!rhi) return EResult::Success;
	RHIShaderDesc rhiShaderDesc = BuildRHIShaderDesc();
	m_RHIShader = rhi->CreateShader(rhiShaderDesc);

	return m_RHIShader ? EResult::Success : EResult::Fail;
}

Shader* Shader::Create(void* arg)
{
	Shader* instance = new Shader();
	if(IsFailure(instance->Initialize(arg)))
	{
		delete instance;
		return nullptr;
	}
	return instance;
}

void Shader::Free()
{
	Safe_Release(m_RHIShader);
}
#pragma endregion

#pragma region RHI

EResult Shader::UpdateBytecode(const vector<uint8>& newBytecode)
{
	if (newBytecode.empty() || !m_RHIShader) return EResult::InvalidArgument;

	// 1. 기존 RHIShader의 포인터를 유지한 채 바이트코드만 갱신
	m_RHIShader->UpdateBytecode(newBytecode);

	// 2. 이 RHIShader를 참조하는 모든 파이프라인의 내부 D3D12 PSO만 실시간 Rebuild
	PipelineManager::Get().RebuildPipelinesWithShader(m_RHIShader);

	IncreaseVersion();
	return EResult::Success;
}
#pragma endregion

#pragma region Getter

RHIShaderDesc Shader::BuildRHIShaderDesc() const
{
	RHIShaderDesc desc;
	desc.shaderType = m_ShaderType;
	desc.entryPoint = m_EntryPoint;

	const bool useDxil = Renderer::IsCreated() && Renderer::Get().GetRHIType() == ERHIType::DirectX12;
	desc.filePath = useDxil ? m_DxilPath : m_SpirvPath;

	desc.numSamplers = m_NumSamplers;
	desc.numStorageTextures = m_NumStorageTextures;
	desc.numStorageBuffers = m_NumStorageBuffers;
	desc.numUniformBuffers = m_NumUniformBuffers;
	return desc;
}

EResult Shader::OnPostLoad()
{
	Safe_Release(m_RHIShader);
	ResolveLegacyBinaryPaths();

	RHI* rhi = Renderer::IsCreated() ? Renderer::Get().GetRHI() : nullptr;
	if (!rhi)
		return EResult::Success;

	m_RHIShader = rhi->CreateShader(BuildRHIShaderDesc());
	return m_RHIShader ? EResult::Success : EResult::Fail;
}

#pragma endregion

#pragma region MyRegion
void Shader::ResolveLegacyBinaryPaths()
{
	std::wstring extension = std::filesystem::path(m_Path).extension().wstring();
	std::transform(extension.begin(), extension.end(), extension.begin(),
		[](wchar_t value) { return static_cast<wchar_t>(std::towlower(value)); });

	if (m_DxilPath.empty() && (extension == L".cso" || extension == L".dxil"))
		m_DxilPath = m_Path;

	if (m_SpirvPath.empty() && extension == L".spv")
		m_SpirvPath = m_Path;
}
#pragma endregion