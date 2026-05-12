#pragma once

#include "RectTransform.h"
#include "GameObject.h"
#include "Renderer.h"

REGISTER_COMPONENT(RectTransform)

#pragma region Constructor&Destructor
EResult RectTransform::Initialize(void* arg)
{
	if (IsFailure(__super::Initialize(arg))) return EResult::Fail;
	CAST_DESC
	m_Size = desc ? desc->Size : vec2(100.0f, 100.0f);
	m_AnchorMin = desc ? desc->AnchorMin : vec2(0.5f, 0.5f);
	m_AnchorMax = desc ? desc->AnchorMax : vec2(0.5f, 0.5f);
	m_AnchoredPosition = desc ? desc->AnchoredPosition : vec2(0.5f, 0.5f);
	m_Pivot = desc ? desc->Pivot : vec2(0.5f, 0.5f);
	m_Scale = desc ? desc->Scale : vec2(1.0f, 1.0f);
	m_Rotation = desc ? desc->Rotation : 0.0f;
	m_Flags = desc ? desc->Flags : ERectTransformFlags::Default;
	SetDirty();
	UpdateMatrix();
	return EResult::Success;
}

Component* RectTransform::Create(void* arg)
{
	RectTransform* instance = new RectTransform();
	if (IsFailure(instance->Initialize(arg)))
	{
		Safe_Release(instance);
		return nullptr;
	}
	return instance;
}
Component* RectTransform::Clone(GameObject* owner, void* arg)
{
	RectTransform* instance = new RectTransform();
	if (arg)
	{
		if (IsFailure(instance->Initialize(&arg)))
		{
			Safe_Release(instance);
			return nullptr;
		}
	}
	else
	{
		DESC transformDesc;
		transformDesc.Size = this->m_Size;
		transformDesc.AnchorMin = this->m_AnchorMin;
		transformDesc.AnchorMax = this->m_AnchorMax;
		transformDesc.AnchoredPosition = this->m_AnchoredPosition;
		transformDesc.Pivot = this->m_Pivot;
		transformDesc.Scale = this->m_Scale;
		transformDesc.Rotation = this->m_Rotation;
		transformDesc.Flags = this->m_Flags;
		transformDesc.Owner = owner;
		transformDesc.Active = this->m_Active;
		transformDesc.Tag = this->m_Tag;
		if (IsFailure(instance->Initialize(&transformDesc)))
		{
			Safe_Release(instance);
			return nullptr;
		}
	}
	return instance;
}

void RectTransform::Free()
{
	__super::Free();
}
void RectTransform::Update(f32 dt)
{
	UpdateMatrix();
}
void RectTransform::UpdateMatrix()
{
	if (!m_Dirty) return;

	vec2 parentPos = vec2(0.0f);
	vec2 parentSize = vec2(Renderer::Get().GetRHI()->GetSwapChainWidth(), Renderer::Get().GetRHI()->GetSwapChainHeight());

	GameObject* parent = (m_Owner) ? m_Owner->GetParent() : nullptr;
	if (parent)
	{
		RectTransform* parentRect = parent->GetComponent<RectTransform>();
		if (parentRect)
		{
			parentRect->UpdateMatrix();

			parentPos = parentRect->GetAbsolutePosition();
			parentSize = parentRect->GetAbsoluteSize();
		}
	}

	vec2 anchorMinPx = parentSize * m_AnchorMin;
	vec2 anchorMaxPx = parentSize * m_AnchorMax;

	m_AbsoluteSize = (anchorMaxPx - anchorMinPx) + m_Size;

	vec2 anchorRef = anchorMinPx + (anchorMaxPx - anchorMinPx) * m_Pivot;
	vec2 myPivotAbsPos = parentPos + anchorRef + m_AnchoredPosition;

	vec2 pivotOffset = m_AbsoluteSize * m_Pivot;
	m_AbsolutePosition = myPivotAbsPos - pivotOffset;

	mat4 pivotTranslation = glm::translate(glm::identity<mat4>(), vec3(-m_Pivot.x, -m_Pivot.y, 0.0f));
	mat4 scale = glm::scale(glm::identity<mat4>(), vec3(m_AbsoluteSize * m_Scale, 1.0f));
	mat4 rotation = glm::rotate(glm::identity<mat4>(), glm::radians(m_Rotation), vec3(0.0f, 0.0f, 1.0f));
	mat4 translation = glm::translate(glm::identity<mat4>(), vec3(myPivotAbsPos, 0.0f));

	m_Matrix = translation * rotation * scale * pivotTranslation;

	m_Dirty = false;
}
#pragma endregion

#pragma region Setter
void RectTransform::SetSize(const vec2& size)
{
	m_Size = size;
	SetDirty();
	UpdateMatrix();
}

void RectTransform::SetAnchorMin(const vec2& anchorMin)
{
	m_AnchorMin = glm::clamp(anchorMin, vec2(0.0f), vec2(1.0f));
	SetDirty();
	UpdateMatrix();
}

void RectTransform::SetAnchorMax(const vec2& anchorMax)
{
	m_AnchorMax = glm::clamp(anchorMax, vec2(0.0f), vec2(1.0f));
	SetDirty();
	UpdateMatrix();
}

void RectTransform::SetAnchoredPosition(const vec2& anchoredPosition)
{
	m_AbsolutePosition = anchoredPosition;
	SetDirty();
	UpdateMatrix();
}

void RectTransform::SetPivot(const vec2& pivot)
{
	m_Pivot = glm::clamp(pivot, vec2(0.0f), vec2(1.0f));
	SetDirty();
	UpdateMatrix();
}

void RectTransform::SetScale(const vec2& scale)
{
	m_Scale = scale;
	SetDirty();
	UpdateMatrix();
}

void RectTransform::SetRotation(const f32& rotation)
{
	m_Rotation = rotation;
	SetDirty();
	UpdateMatrix();
}

void RectTransform::SetFlags(const ERectTransformFlags& flags)
{
	m_Flags = flags;
	SetDirty();
	UpdateMatrix();
}
#pragma endregion


#pragma region Presets
void RectTransform::SetAnchorPreset(ERectAnchor preset)
{
	switch (preset)
	{
	case Engine::ERectAnchor::TopLeft:
		break;
	case Engine::ERectAnchor::TopCenter:
		break;
	case Engine::ERectAnchor::TopRight:
		break;
	case Engine::ERectAnchor::MiddleLeft:
		break;
	case Engine::ERectAnchor::MiddleCenter:
		break;
	case Engine::ERectAnchor::MiddleRight:
		break;
	case Engine::ERectAnchor::BottomLeft:
		break;
	case Engine::ERectAnchor::BottomCenter:
		break;
	case Engine::ERectAnchor::BottomRight:
		break;
	case Engine::ERectAnchor::StretchTop:
		break;
	case Engine::ERectAnchor::StretchMiddle:
		break;
	case Engine::ERectAnchor::StretchBottom:
		break;
	case Engine::ERectAnchor::StretchLeft:
		break;
	case Engine::ERectAnchor::StretchCenter:
		break;
	case Engine::ERectAnchor::StretchRight:
		break;
	case Engine::ERectAnchor::StretchFull:
		break;
	case Engine::ERectAnchor::Custorm:
	default:
		break;
	}
}
void RectTransform::SetPivotPreset(ERectPivot preset)
{
	switch (preset)
	{
	case Engine::ERectPivot::Custorm:
		break;
	case Engine::ERectPivot::TopLeft:
		break;
	case Engine::ERectPivot::TopCenter:
		break;
	case Engine::ERectPivot::TopRight:
		break;
	case Engine::ERectPivot::MiddleLeft:
		break;
	case Engine::ERectPivot::MiddleCenter:
		break;
	case Engine::ERectPivot::MiddleRight:
		break;
	case Engine::ERectPivot::BottomLeft:
		break;
	case Engine::ERectPivot::BottomCenter:
		break;
	case Engine::ERectPivot::BottomRight:
		break;
	default:
		break;
	}
}
#pragma endregion


