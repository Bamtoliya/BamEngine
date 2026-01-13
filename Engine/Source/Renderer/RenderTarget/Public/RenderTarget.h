#pragma once

#include "Base.h"

BEGIN(Engine)
class ENGINE_API RenderTarget final : public Base
{
private:
	EResult Initialize(void* arg = nullptr);
	virtual void Free() override;
public:
	static RenderTarget* Create(void* arg = nullptr);
};
END