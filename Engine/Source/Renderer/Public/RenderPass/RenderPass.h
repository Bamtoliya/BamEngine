#pragma once

#include "Base.h"
#include "RenderTypes.h"

BEGIN(Engine)
class ENGINE_API RenderPass : public Base
{
#pragma region Constructor&Destructor
private:
	RenderPass() {}
	virtual ~RenderPass() = default;
	EResult Initialize(void* arg = nullptr);
public:
	static RenderPass* Create(void* arg = nullptr);
	virtual void Free() override;
#pragma endregion

#pragma region Variable
private:
	RenderPassInfo m_Info;
#pragma endregion


};
END