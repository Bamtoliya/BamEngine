#pragma once

#include "Base.h"
#include "RHIShader.h"

BEGIN(Engine)
class ENGINE_API Material : public Base
{
#pragma region Constructor&Destructor
	private:
	Material() {}
	virtual ~Material() = default;
	EResult Initialize(void* arg = nullptr);
public:
	static Material* Create(void* arg = nullptr);
	Material* Clone(void* arg = nullptr);
	virtual void Free() override;
#pragma endregion

#pragma region Variable
private:
	RHIShader* m_VertexShader = { nullptr };
	RHIShader* m_PixelShader = { nullptr };
#pragma endregion
};
END