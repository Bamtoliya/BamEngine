#pragma once

#include "Engine_API.h"

namespace Engine
{
	class ENGINE_API IActive
	{
	public:
		virtual ~IActive() = default;
		virtual bool IsActive() const = 0;
		virtual void SetActive(bool active) = 0;
	};

	class ENGINE_API IVisible
	{
	public:
		virtual ~IVisible() = default;
		virtual bool IsVisible() const = 0;
		virtual void SetVisible(bool visible) = 0;
	};
}