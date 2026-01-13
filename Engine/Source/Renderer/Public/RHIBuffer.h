#pragma once

#include "RHIResource.h"

BEGIN(Engine)
class ENGINE_API Buffer : public RHIResource
{
protected:
	Buffer() {}
	virtual ~Buffer() = default;
}
END