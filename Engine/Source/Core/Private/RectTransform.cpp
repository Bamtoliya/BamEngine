#pragma once

#include "RectTransform.h"
#include "GameObject.h"
#include "Renderer.h"
#include "Transform.h"
#include "UICanvas.h"

REGISTER_COMPONENT(RectTransform)

namespace
{
	Engine::UICanvas* FindNearestCanvas(Engine::GameObject* from)
	{
		Engine::GameObject* cur = from;
		while (cur)
		{
			if (Engine::UICanvas* canvas = cur->GetComponent<UICanvas>())
				return canvas;
			cur = cur->GetParent();
		}
		return nullptr;
	}

	Engine::UICanvas* FindParentCanvas(Engine::UICanvas* selfCanvas)
	{
		if (!selfCanvas || !selfCanvas->GetOwner())
			return nullptr;

		Engine::GameObject* cur = selfCanvas->GetOwner()->GetParent();
		while (cur)
		{
			if (Engine::UICanvas* parentCanvas = cur->GetComponent<UICanvas>())
				return parentCanvas;
			cur = cur->GetParent();
		}
		return nullptr;
	}
}

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

	// 부모의 정보 가져오기
	mat4 parentWorldTransform = glm::identity<mat4>();
	vec2 parentSize = vec2(Renderer::Get().GetRHI()->GetSwapChainWidth(), Renderer::Get().GetRHI()->GetSwapChainHeight());
	vec2 parentPivot = vec2(0.0f, 0.0f);

	bool hasParentRect = false;
	GameObject* parent = (m_Owner) ? m_Owner->GetParent() : nullptr;
	if (parent)
	{
		RectTransform* parentRect = parent->GetComponent<RectTransform>();
		if (parentRect)
		{
			hasParentRect = true;
			parentRect->UpdateMatrix();

			parentSize = parentRect->GetAbsoluteSize();
			parentPivot = parentRect->GetPivot();

			// [핵심 변경] 부모의 렌더링 행렬이 아닌, '순수 WorldTransform'만 가져옵니다.
			parentWorldTransform = parentRect->m_WorldTransform;
		}
	}

	// 루트 캔버스 처리
	if (!hasParentRect)
	{
		if (UICanvas* canvas = FindNearestCanvas(m_Owner))
		{
			UICanvas* parentCanvas = FindParentCanvas(canvas);
			if (canvas->IsLayoutDirty())
			{
				const vec2 resolved = canvas->ResolveLayoutSize(parentCanvas, parentSize);
				canvas->ApplyResolvedLayoutSize(resolved);
			}
			parentSize = canvas->GetEffectiveLayoutSize();
		}
	}

	// 1. 레이아웃 픽셀 크기 계산 (Scale 미적용 순수 크기)
	vec2 anchorMinPx = parentSize * m_AnchorMin;
	vec2 anchorMaxPx = parentSize * m_AnchorMax;
	m_AbsoluteSize = (anchorMaxPx - anchorMinPx) + m_Size;

	// 2. 부모의 Pivot을 기준으로 한 나의 상대적 픽셀 위치
	vec2 anchorRef = anchorMinPx + (anchorMaxPx - anchorMinPx) * m_Pivot;
	vec2 posFromParentBottomLeft = anchorRef + m_AnchoredPosition;
	vec2 parentPivotPx = parentSize * parentPivot;
	vec2 myLocalPosPx = posFromParentBottomLeft - parentPivotPx;

	// 3. 나의 로컬 Transform 행렬 생성 (위치, 회전, '배율'만 포함)
	mat4 localTranslate = glm::translate(glm::identity<mat4>(), vec3(myLocalPosPx, 0.0f));
	mat4 localRotate = glm::rotate(glm::identity<mat4>(), glm::radians(m_Rotation), vec3(0.0f, 0.0f, 1.0f));
	mat4 localScale = glm::scale(glm::identity<mat4>(), vec3(m_Scale, 1.0f));

	mat4 localTransform = localTranslate * localRotate * localScale;

	// 4. 자식에게 물려줄 순수 World Transform 계산
	//    이 행렬에는 픽셀 크기(Size)나 내 Pivot 변환이 포함되지 않습니다!
	m_WorldTransform = parentWorldTransform * localTransform;

	// 5. 렌더링을 위한 최종 행렬 (m_Matrix) 생성
	//    1x1 쿼드를 그리기 위해 내 AbsoluteSize만큼 스케일을 키우고, 내 Pivot만큼 이동시킨 뒤 월드에 배치합니다.
	mat4 applySize = glm::scale(glm::identity<mat4>(), vec3(m_AbsoluteSize, 1.0f));
	mat4 applyPivot = glm::translate(glm::identity<mat4>(), vec3(-m_Pivot.x, -m_Pivot.y, 0.0f));

	m_Matrix = m_WorldTransform * applySize * applyPivot;

	// UI 이벤트/로직용 절대 좌표 (Size가 제외된 순수 위치)
	m_AbsolutePosition = vec2(m_WorldTransform[3][0], m_WorldTransform[3][1]);

	m_Dirty = false;
}
#pragma endregion

#pragma region Setter
void RectTransform::SetDirty(bool dirty)
{
	__super::SetDirty(dirty);
	if (dirty && m_Owner)
	{
		for (GameObject* child : m_Owner->GetAllChilds())
		{
			if (RectTransform* childRect = child->GetComponent<RectTransform>())
			{
				childRect->SetDirty(dirty);
				childRect->UpdateMatrix();
			}
			if (Transform* childTransform = child->GetComponent<Transform>())
			{
				childTransform->SetDirty(dirty);
			}
		}
	}
}

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
	m_AnchoredPosition = anchoredPosition;
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


