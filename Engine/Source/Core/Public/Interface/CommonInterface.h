#pragma once

#include "Engine_Includes.h"

BEGIN(Engine)
class ENGINE_API ActiveInterface
{
public:
	virtual ~ActiveInterface() = default;
	virtual bool IsActive() const = 0;
	virtual void SetActive(bool active) = 0;
};

class ENGINE_API VisibleInterface
{
public:
	virtual ~VisibleInterface() = default;
	virtual bool IsVisible() const = 0;
	virtual void SetVisible(bool visible) = 0;
};
END