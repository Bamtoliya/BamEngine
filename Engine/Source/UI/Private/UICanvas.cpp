#pragma once

#include "UICanvas.h"
#include "ComponentRegistry.h"
#include "Math.h"

REGISTER_COMPONENT(UICanvas)

namespace
{
    inline Engine::vec2 SanitizeSize(const Engine::vec2& inSize, const Engine::vec2& fallback)
    {
        Engine::vec2 out = inSize;
        if (out.x <= 1.0f) out.x = fallback.x;
        if (out.y <= 1.0f) out.y = fallback.y;
        if (out.x <= 1.0f) out.x = 1.0f;
        if (out.y <= 1.0f) out.y = 1.0f;
        return out;
    }
}

EResult UICanvas::Initialize(void* arg)
{
    if (IsFailure(__super::Initialize(arg)))
        return EResult::Fail;

    m_RenderMode = ECanvasRenderMode::ScreenSpace;
    m_IsLayoutRoot = true;
    m_LayoutSource = ECanvasLayoutSource::ReferenceResolution;
    m_ReferenceResolution = vec2(1920.0f, 1080.0f);
    m_ScaleMode = EUIScaleMode::ScaleWithScreenSize;
    m_MatchWidthOrHeight = 0.5f;
    m_EffectiveLayoutSize = m_ReferenceResolution;
    m_ExternalLayoutSize = vec2(0.0f, 0.0f);
    m_ExternalOverrideEnabled = false;
    m_ExternalOwnerToken = 0;
    m_LayoutDirty = true;
    m_LayoutVersion = 0;

    return EResult::Success;
}

UICanvas* UICanvas::Create(void* arg)
{
    UICanvas* instance = new UICanvas();
    if (IsFailure(instance->Initialize(arg)))
    {
        Safe_Release(instance);
        return nullptr;
    }
    return instance;
}

Component* UICanvas::Clone(GameObject* owner, void* arg)
{
    UICanvas* instance = new UICanvas();
    if (!instance)
        return nullptr;

    if (IsFailure(instance->Initialize(arg)))
    {
        Safe_Release(instance);
        return nullptr;
    }

    instance->SetOwner(owner);
    instance->m_RenderMode = m_RenderMode;
    instance->m_IsLayoutRoot = m_IsLayoutRoot;
    instance->m_LayoutSource = m_LayoutSource;
    instance->m_ReferenceResolution = m_ReferenceResolution;
    instance->m_ScaleMode = m_ScaleMode;
    instance->m_MatchWidthOrHeight = m_MatchWidthOrHeight;
    instance->m_EffectiveLayoutSize = m_EffectiveLayoutSize;
    instance->m_ExternalLayoutSize = m_ExternalLayoutSize;
    instance->m_ExternalOverrideEnabled = m_ExternalOverrideEnabled;
    instance->m_ExternalOwnerToken = m_ExternalOwnerToken;
    instance->m_LayoutDirty = true;
    instance->m_LayoutVersion = 0;

    return instance;
}

void UICanvas::Free()
{
    __super::Free();
}

void UICanvas::SetLayoutSource(ECanvasLayoutSource source)
{
    if (m_LayoutSource == source)
        return;

    m_LayoutSource = source;
    MarkLayoutDirty();
}

void UICanvas::SetReferenceResolution(const vec2& res)
{
    vec2 safeRes = res;
    safeRes.x = glm::max(safeRes.x, 1.0f);
    safeRes.y = glm::max(safeRes.y, 1.0f);

    if (m_ReferenceResolution == safeRes)
        return;

    m_ReferenceResolution = safeRes;
    MarkLayoutDirty();
}

void UICanvas::SetExternalLayoutSize(const vec2& size, uint64 ownerToken)
{
    vec2 safeSize = vec2(glm::max(size.x, 1.0f), glm::max(size.y, 1.0f));

    // 이미 소유자가 있는 경우, 같은 토큰이 아니면 무시 (0 토큰은 강제 갱신 허용)
    if (m_ExternalOverrideEnabled && m_ExternalOwnerToken != 0 && ownerToken != 0 && m_ExternalOwnerToken != ownerToken)
        return;

    bool changed = (m_ExternalLayoutSize != safeSize) || (!m_ExternalOverrideEnabled) || (m_ExternalOwnerToken != ownerToken);

    m_ExternalLayoutSize = safeSize;
    m_ExternalOverrideEnabled = true;
    m_ExternalOwnerToken = ownerToken;

    if (changed)
        MarkLayoutDirty();
}

void UICanvas::ClearExternalLayoutSize(uint64 ownerToken)
{
    if (!m_ExternalOverrideEnabled)
        return;

    // 토큰이 설정된 경우 소유자만 해제 가능
    if (m_ExternalOwnerToken != 0 && ownerToken != 0 && m_ExternalOwnerToken != ownerToken)
        return;

    m_ExternalOverrideEnabled = false;
    m_ExternalLayoutSize = vec2(0.0f, 0.0f);
    m_ExternalOwnerToken = 0;
    MarkLayoutDirty();
}

vec2 UICanvas::ResolveLayoutSize(const UICanvas* parentCanvas, const vec2& fallbackSize) const
{
    const vec2 safeFallback = SanitizeSize(fallbackSize, vec2(1920.0f, 1080.0f));

    // 1) 외부 오버라이드 우선
    if (m_LayoutSource == ECanvasLayoutSource::ExternalOverride && m_ExternalOverrideEnabled)
        return SanitizeSize(m_ExternalLayoutSize, safeFallback);

    // 2) 부모 상속
    if (m_LayoutSource == ECanvasLayoutSource::InheritParent)
    {
        if (parentCanvas)
            return SanitizeSize(parentCanvas->GetEffectiveLayoutSize(), safeFallback);
        return safeFallback;
    }

    // 3) 레퍼런스 해상도
    if (m_LayoutSource == ECanvasLayoutSource::ReferenceResolution)
        return SanitizeSize(m_ReferenceResolution, safeFallback);

    return safeFallback;
}

void UICanvas::ApplyResolvedLayoutSize(const vec2& resolvedSize)
{
    vec2 safe = vec2(glm::max(resolvedSize.x, 1.0f), glm::max(resolvedSize.y, 1.0f));
    if (m_EffectiveLayoutSize == safe)
    {
        m_LayoutDirty = false;
        return;
    }

    m_EffectiveLayoutSize = safe;
    m_LayoutDirty = false;
    ++m_LayoutVersion;
}

void UICanvas::MarkLayoutDirty()
{
    m_LayoutDirty = true;
}

void UICanvas::SetMatchWidthOrHeight(f32 value)
{
    f32 clamped = glm::clamp(value, 0.0f, 1.0f);
    if (m_MatchWidthOrHeight == clamped)
        return;

    m_MatchWidthOrHeight = clamped;
    MarkLayoutDirty();
}