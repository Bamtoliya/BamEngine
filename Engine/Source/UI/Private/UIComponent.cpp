#pragma once

#include "UIComponent.h"
#include "ComponentRegistry.h"
#include "UICanvas.h"

REGISTER_COMPONENT(UIComponent)

void UIComponent::EnsureRootCanvas()
{
	if (!m_Owner) return;
	GameObject* curr = m_Owner;
	bool hasCanvas = false;
	while (curr)
	{
		if (curr->GetComponent<UICanvas>())
		{
			hasCanvas = true;
			break;
		}
		curr = curr->GetParent();
	}
	if (!hasCanvas)
	{
		GameObject* root = m_Owner;
		while (root->GetParent())
		{
			root = root->GetParent();
		}

		if (!root->GetComponent<UICanvas>())
		{
			root->AddComponent<UICanvas>();
			ENGINE_LOG_INFO("UIComponent 첨부 감지: Root GameObject에 UICanvas를 자동 생성했습니다.");
		}
	}
}
