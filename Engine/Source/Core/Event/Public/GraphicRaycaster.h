#pragma once

#include "BaseRaycaster.h"
#include "EventInterface.h"
#include "UICanvas.h"

BEGIN(Engine)

CLASS()
class ENGINE_API GraphicRaycaster : public BaseRaycaster
{
	REFLECT_CLASS()
protected:
	GraphicRaycaster() {}
	virtual ~GraphicRaycaster() = default;
	EResult Initialize(void* arg) override;
public:
	static GraphicRaycaster* Create(void* arg = nullptr);
	virtual Component* Clone(GameObject* owner, void* arg = nullptr) override;
	virtual void Free() override;
public:
	virtual void Raycast(const PointerEventData& eventData, vector<RaycastResult>& outResults);
private:
	void RaycastRecursive(GameObject* gameObject, const PointerEventData& eventData, vector<RaycastResult>& outResults, uint32 sortOrder);
private:
	UICanvas* m_Canvas = { nullptr };
};

END