#pragma once

#include "RHIPipeline.h"

BEGIN(Engine)
class ENGINE_API Pipeline final : public Base
{
#pragma region Constructor&Destructor
private:
	Pipeline() {}
	virtual ~Pipeline() = default;
	EResult Initialize(void* arg = nullptr);
public:
	static Pipeline* Create(void* arg = nullptr);
	Pipeline* Clone(void* arg = nullptr);
	virtual void Free() override;
#pragma endregion

#pragma region Getter
	RHIPipeline* GetRHIPipeline() const { return m_RHIPipeline; }
#pragma endregion

#pragma region Variable
private:
	class RHIPipeline* m_RHIPipeline = { nullptr };
#pragma endregion
};
END