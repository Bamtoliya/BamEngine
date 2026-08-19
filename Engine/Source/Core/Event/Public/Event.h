#pragma once

#include "Base.h"

BEGIN(Engine)

class GameObject;

struct PointerEventData
{
	vec2 Position = { 0.f, 0.f };
	vec2 Delta = { 0.f, 0.f };
	bool IsDragging = false;

	GameObject* PointerPress = nullptr;
	GameObject* PointerEnter = nullptr;
};

struct RaycastResult
{
	GameObject* Target = nullptr;
	f32 Distance = 0.0f;
	int32 SortOrder = 0; // UI의 경우 Z-Index 우선순위
	bool IsValid = false;
	// 우선순위 정렬: SortOrder가 높을수록 앞, 같다면 Distance가 짧을수록 앞
	bool operator<(const RaycastResult& other) const
	{
		if (SortOrder != other.SortOrder)
			return SortOrder > other.SortOrder;
		return Distance < other.Distance;
	}
};

class IEventSystemHandler
{
public:
	virtual ~IEventSystemHandler() = default;
};
class IPointerEnterHandler : public virtual IEventSystemHandler
{
public:
	virtual void OnPointerEnter(const PointerEventData& eventData) = 0;
};
class IPointerExitHandler : public virtual IEventSystemHandler
{
public:
	virtual void OnPointerExit(const PointerEventData& eventData) = 0;
};
class IPointerDownHandler : public virtual IEventSystemHandler
{
public:
	virtual void OnPointerDown(const PointerEventData& eventData) = 0;
};
class IPointerUpHandler : public virtual IEventSystemHandler
{
public:
	virtual void OnPointerUp(const PointerEventData& eventData) = 0;
};
class IPointerClickHandler : public virtual IEventSystemHandler
{
public:
	virtual void OnPointerClick(const PointerEventData& eventData) = 0;
};

END