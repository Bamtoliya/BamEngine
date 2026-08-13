#pragma once

#include "Component.h"
#include "EventInterface.h"

BEGIN(Engine)

CLASS()
class ENGINE_API BaseRaycaster : public Component
{
	REFLECT_CLASS()
protected:
	BaseRaycaster() {}
public:
	virtual ~BaseRaycaster() = default;
	virtual EResult Initialize(void* arg = nullptr) override;
public:
	virtual void Free() override;
public:
	virtual void Raycast(const PointerEventData& eventData, vector<RaycastResult>& outResults) BAM_PURE;
};

END