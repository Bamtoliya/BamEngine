#pragma once

#include "RHITexture.h"

BEGIN(Engine)
typedef struct tagTextureCreateInfo
{
	wstring FilePath;
} TEXTUREDESC;
class ENGINE_API Texture final : public Base
{
#pragma region Constructor&Destructor
private:
	Texture() {}
	virtual ~Texture() = default;
	EResult Initialize(void* arg = nullptr);
public:
	static Texture* Create(void* arg = nullptr);
	Texture* Clone(void* arg = nullptr);
	virtual void Free() override;
#pragma endregion

#pragma region Getter
	RHITexture* GetRHITexture() const { return m_RHITexture; }
#pragma endregion

#pragma region Variable
private:
	class RHITexture* m_RHITexture = { nullptr };
	wstring m_Path = L"";
#pragma endregion
};
END