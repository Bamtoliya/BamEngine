#pragma once 

#include "Resource.h"
#include "RHIShader.h"

struct tagShaderDesc {
	EShaderType ShaderType;
	wstring FilePath;
	string EntryPoint;
};

BEGIN(Engine)
CLASS()
class ENGINE_API Shader final : public Resource
{
	REFLECT_CLASS()
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

#pragma region Save&Load
public:
	virtual void Deserialize(Archive& ar);
#pragma endregion


#pragma region Variable
private:
	PROPERTY()
	RHIShader* m_RHIShader = { nullptr };
#pragma endregion

};
END