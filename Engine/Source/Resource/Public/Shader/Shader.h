#pragma once 

#include "RHIShader.h"

BEGIN(Engine)
class ENGINE_API Shader final : public Base
{
#pragma region Struct
	typedef struct tagShaderInfo {
		EShaderType ShaderType;
		wstring filePath;
		string entryPoint;
	} SHADERDESC;
#pragma endregion

#pragma region Constructor&Destructor
private:
	Shader() {}
	virtual ~Shader() = default;
	EResult Initialize(void* arg = nullptr);
public:
	static Shader* Create(void* arg = nullptr);
	Shader* Clone(void* arg = nullptr);
	virtual void Free() override;
#pragma endregion

#pragma region Variable
private:
	class RHIShader* m_RHIShader = { nullptr };
#pragma endregion

};
END