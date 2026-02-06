#pragma once

#include "RenderComponent.h"


void RenderComponent::LateUpdate(f32 dt)
{
	__super::LateUpdate(dt);
	if (m_Active && m_Owner->IsActive() && m_Owner->IsVisible())
	{
		Renderer::Get().Submit(this, m_RenderPassID);
	}
}