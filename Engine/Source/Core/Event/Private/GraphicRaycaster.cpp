#pragma once
#include "GraphicRaycaster.h"
#include "ComponentRegistry.h"
#include "Structs.h"

REGISTER_COMPONENT(GraphicRaycaster);

EResult GraphicRaycaster::Initialize(void* arg)
{
	if (IsFailure(__super::Initialize(arg)))
		return EResult::Fail;
	return EResult();
}

GraphicRaycaster* GraphicRaycaster::Create(void* arg)
{
	GraphicRaycaster* instance = new GraphicRaycaster();
	if (IsFailure(instance->Initialize(arg)))
	{
		Safe_Release(instance);
		return nullptr;
	}
	return instance;
}

Component* GraphicRaycaster::Clone(GameObject* owner, void* arg)
{
	GraphicRaycaster* clone = Create(arg);
	if (clone) clone->SetOwner(owner);
	return clone;
}

void GraphicRaycaster::Free()
{
	__super::Free();
}

void GraphicRaycaster::Raycast(const PointerEventData& eventData, vector<RaycastResult>& outResults)
{
	if (!m_Owner)
		return;
	if (!m_Canvas)
	{
		m_Canvas = m_Owner->GetComponent<UICanvas>();
	}

	if (!m_Canvas)
		return;

	int32 canvasSortOrder = m_Owner->GetLayerIndex();
	vector<GameObject*> childs = m_Owner->GetAllChilds();
	cout << "GraphicRaycaster: " << eventData.Position.x << ", " << eventData.Position.y << endl;
	for (GameObject* child : childs)
	{
		RaycastRecursive(child, eventData, outResults, canvasSortOrder);
	}
}

void GraphicRaycaster::RaycastRecursive(GameObject* gameObject, const PointerEventData& eventData, vector<RaycastResult>& outResults, uint32 sortOrder)
{
	if (!gameObject || !gameObject->IsActive() || !gameObject->IsVisibleInHierarchy())
		return;

	RectTransform* rectTransform =	gameObject->GetComponent<RectTransform>();
	if (rectTransform)
	{
		if (HasFlag(rectTransform->GetFlags(), ERectTransformFlags::RaycastTarget))
		{
			vec2 pos = rectTransform->GetAbsolutePosition();
			vec2 size = rectTransform->GetAbsoluteSize();
			vec2 pivot = rectTransform->GetPivot();

			f32 left = pos.x - (size.x * pivot.x);
			f32 top = pos.y - (size.y * pivot.y);
			Rect bounds(left, top, size.x, size.y);

			if (bounds.Contains(eventData.Position))
			{
				RaycastResult result;
				result.Target = gameObject;
				result.Distance = 0.0f;

				result.SortOrder = sortOrder + gameObject->GetIndex();
				result.IsValid = true;

				outResults.push_back(result);
			}
		}
	}

	for (GameObject* child : gameObject->GetAllChilds())
	{
		RaycastRecursive(child, eventData, outResults, sortOrder);
	}
}
