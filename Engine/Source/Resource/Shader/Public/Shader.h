#pragma once 

#include "Resource.h"
#include "RHIShader.h"

struct tagShaderDesc : public tagResourceCreateDesc {
	EShaderType ShaderType = { EShaderType::Unknown };
	string EntryPoint = { "main" };
	wstring SpirvPath = {};

	uint32 NumSamplers = { 0 };
	uint32 NumStorageTextures = { 0 };
	uint32 NumStorageBuffers = { 0 };
	uint32 NumUniformBuffers = { 0 };
};

BEGIN(Engine)
CLASS()
class ENGINE_API Shader final : public Resource
{
	REFLECT_CLASS()
	DECLARE_RESOURCE(Shader)
	using DESC = tagShaderDesc;
#pragma region Constructor&Destructor
private:
	Shader() : Resource(EResourceType::Shader) {}
	virtual ~Shader() = default;
	EResult Initialize(void* arg = nullptr);
public:
	static Shader* Create(void* arg = nullptr);
	virtual void Free() override;
#pragma endregion

#pragma region RHI
public:
	RHIShader* GetRHIShader() const { return m_RHIShader; }
#pragma endregion


#pragma region Bind
public:
	EResult Bind(uint32 slot) override { return EResult::NotImplemented; }
#pragma endregion

#pragma region Getter
public:
	EShaderType GetShaderType() const { return m_ShaderType; }
	const string& GetEntryPoint() const { return m_EntryPoint; }
	const wstring& GetSpirvPath() const { return m_SpirvPath; }

	uint32 GetNumSamplers() const { return m_NumSamplers; }
	uint32 GetNumStorageTextures() const { return m_NumStorageTextures; }
	uint32 GetNumStorageBuffers() const { return m_NumStorageBuffers; }
	uint32 GetNumUniformBuffers() const { return m_NumUniformBuffers; }

	tagRHIShaderDesc BuildRHIShaderDesc() const;
#pragma endregion

#pragma region Save&Load
public:
	void Serialize(Archive& ar) override;
	void Deserialize(Archive& ar) override;
#pragma endregion


#pragma region Member Variables
private:

	PROPERTY()
	EShaderType m_ShaderType = { EShaderType::Unknown };
	PROPERTY()
	wstring m_SpirvPath = {};
	PROPERTY()
	string m_EntryPoint = "main";

	PROPERTY()
	uint32 m_NumSamplers = 0;
	PROPERTY()
	uint32 m_NumStorageTextures = 0;
	PROPERTY()
	uint32 m_NumStorageBuffers = 0;
	PROPERTY()
	uint32 m_NumUniformBuffers = 0;

	PROPERTY(NOSERIALIZE)
	RHIShader* m_RHIShader = { nullptr };
#pragma endregion

};
END