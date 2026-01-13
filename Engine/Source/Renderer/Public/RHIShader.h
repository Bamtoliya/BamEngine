#pragma once

#include "RHIResouce.h"
BEGIN(Engine)
class ENGINE_API Shader : public RHIResource
{
protected:
	Shader() {}
	virtual ~Shader() = default;
}
END