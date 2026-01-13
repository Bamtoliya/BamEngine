#pragma once

#include "RHIResource.h"

class ENGINE_API Texture : public RHIResource
{
private:

public:
	virtual Texture* Create(void* arg) PURE;

}