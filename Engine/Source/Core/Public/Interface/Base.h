#pragma once

#include "Engine_API.h"
#include "Macro.h"
#include "Result.h"
#include <cstdint>

BEGIN(Engine)
class ENGINE_API Base
{
protected:
	Base() {}
	virtual ~Base() = default;
public:
	virtual void Free() {}
	int32 AddRef()
	{
		return ++m_RefCount;
	}
	int32 Release()
	{
		int32 refCount = --m_RefCount;
		if (refCount == 0)
		{
			Free();
			delete this;
		}
		return refCount;
	}
protected:
	int32 m_RefCount = { 1 };
};
END