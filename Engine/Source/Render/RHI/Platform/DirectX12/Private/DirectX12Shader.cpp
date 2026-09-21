#include "DirectX12Shader.h"

EResult DirectX12Shader::Initialize(const DESC& desc)
{
	m_ShaderType = desc.shaderType;
	m_EntryPoint = desc.entryPoint;

	if (!desc.shaderBytecode.empty())
	{
		m_ShaderBytecode = desc.shaderBytecode;
	}
	else if (!desc.filePath.empty())
	{
		ifstream file(desc.filePath, ios::binary | ios::ate);
		if (!file.is_open())
		{
			fmt::print(stderr, "Shader open failed: {}\n", WStrToStr(desc.filePath));
			return EResult::FileNotFound;
		}

		size_t fileSize = static_cast<size_t>(file.tellg());
		m_ShaderBytecode.resize(fileSize);
		file.seekg(0, ios::beg);
		file.read((char*)m_ShaderBytecode.data(), fileSize);
		file.close();
		fmt::print(stderr, "DX12 shader load: {}, size={}\n", WStrToStr(desc.filePath), fileSize);
	}
	else
	{
		return EResult::InvalidArgument;
	}

	

    return EResult::Success;
}

DirectX12Shader* DirectX12Shader::Create(RHI* rhi, const DESC& desc)
{
	DirectX12Shader* shader = new DirectX12Shader(rhi);
	if (IsFailure(shader->Initialize(desc)))
	{
		Safe_Release(shader);
		return nullptr;
	}
    return shader;
}

void DirectX12Shader::Free()
{
	m_ShaderBytecode.clear();
	__super::Free();
}

D3D12_SHADER_BYTECODE DirectX12Shader::GetBytecode() const
{
	D3D12_SHADER_BYTECODE bytecode{};
	if (!m_ShaderBytecode.empty())
	{
		bytecode.pShaderBytecode = m_ShaderBytecode.data();
		bytecode.BytecodeLength = m_ShaderBytecode.size();
	}
	return bytecode;
}