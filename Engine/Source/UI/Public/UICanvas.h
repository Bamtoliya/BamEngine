#pragma once

#include "UIComponent.h"

BEGIN(Engine)

ENUM()
enum class ECanvasRenderMode : uint8
{
    ScreenSpace,
    WorldSpace,
    ScreenSpaceCamera,
};

ENUM()
enum class ECanvasLayoutSource : uint8
{
    InheritParent,
    ReferenceResolution,
    ExternalOverride,
};

ENUM()
enum class EUIScaleMode : uint8
{
    ConstantPixelSize,
    ScaleWithScreenSize,
    ConstantPhysicalSize,
};

CLASS()
class ENGINE_API UICanvas : public UIComponent
{
    REFLECT_CLASS()
#pragma region Constructor&Destructor
protected:
protected:
    UICanvas() {}
    virtual EResult Initialize(void* arg = nullptr) override;
public:
    virtual ~UICanvas() = default;
    static UICanvas* Create(void* arg = nullptr);
    virtual Component* Clone(GameObject* owner, void* arg = nullptr) override;
    virtual void Free() override;
#pragma endregion

#pragma region RenderMode
public:
    ECanvasRenderMode GetRenderMode() const { return m_RenderMode; }
    void SetRenderMode(ECanvasRenderMode renderMode) { m_RenderMode = renderMode; MarkLayoutDirty(); }
#pragma endregion

#pragma region Layout Policy
public:
    bool IsLayoutRoot() const { return m_IsLayoutRoot; }
    void SetLayoutRoot(bool isRoot) { m_IsLayoutRoot = isRoot; MarkLayoutDirty(); }

    ECanvasLayoutSource GetLayoutSource() const { return m_LayoutSource; }
    void SetLayoutSource(ECanvasLayoutSource source);

    const vec2& GetReferenceResolution() const { return m_ReferenceResolution; }
    void SetReferenceResolution(const vec2& res);

    const vec2& GetExternalLayoutSize() const { return m_ExternalLayoutSize; }
    void SetExternalLayoutSize(const vec2& size, uint64 ownerToken = 0);
    void ClearExternalLayoutSize(uint64 ownerToken = 0);

    const vec2& GetEffectiveLayoutSize() const { return m_EffectiveLayoutSize; }

    EUIScaleMode GetScaleMode() const { return m_ScaleMode; }
    void SetScaleMode(EUIScaleMode mode) { m_ScaleMode = mode; MarkLayoutDirty(); }

    f32 GetMatchWidthOrHeight() const { return m_MatchWidthOrHeight; }
    void SetMatchWidthOrHeight(f32 value);

    bool IsExternalOverrideEnabled() const { return m_ExternalOverrideEnabled; }
    uint64 GetExternalOwnerToken() const { return m_ExternalOwnerToken; }

    uint32 GetLayoutVersion() const { return m_LayoutVersion; }
#pragma endregion

#pragma region Layout Update
public:
    // 부모 Canvas(없으면 nullptr)와 fallback 크기(보통 swapchain)를 받아 최종 레이아웃 크기 계산
    vec2 ResolveLayoutSize(const UICanvas* parentCanvas, const vec2& fallbackSize) const;

    // 계산된 결과를 캐시하고 버전 증가
    void ApplyResolvedLayoutSize(const vec2& resolvedSize);

    void MarkLayoutDirty();
    bool IsLayoutDirty() const { return m_LayoutDirty; }
#pragma endregion

#pragma region Member Variable
protected:
    PROPERTY(EDITABLE)
        ECanvasRenderMode m_RenderMode = ECanvasRenderMode::ScreenSpace;

    PROPERTY(EDITABLE, CATEGORY("Layout"))
        bool m_IsLayoutRoot = true;

    PROPERTY(EDITABLE, CATEGORY("Layout"))
        ECanvasLayoutSource m_LayoutSource = ECanvasLayoutSource::ReferenceResolution;

    PROPERTY(EDITABLE, CATEGORY("Layout"))
        vec2 m_ReferenceResolution = { 1920.0f, 1080.0f };

    PROPERTY(EDITABLE, CATEGORY("Layout"))
        EUIScaleMode m_ScaleMode = EUIScaleMode::ScaleWithScreenSize;

    PROPERTY(EDITABLE, RANGE(0.f, 1.f, 0.01f), CATEGORY("Layout"))
        f32 m_MatchWidthOrHeight = 0.5f;

    PROPERTY(READONLY, NOSERIALIZE, CATEGORY("Layout"))
        vec2 m_EffectiveLayoutSize = { 1920.0f, 1080.0f };

    PROPERTY(NOSERIALIZE, CATEGORY("Layout"))
        vec2 m_ExternalLayoutSize = { 0.0f, 0.0f };

    PROPERTY(NOSERIALIZE, CATEGORY("Layout"))
        bool m_ExternalOverrideEnabled = false;

    PROPERTY(NOSERIALIZE, CATEGORY("Layout"))
        uint64 m_ExternalOwnerToken = 0;

    PROPERTY(READONLY, NOSERIALIZE, CATEGORY("Layout"))
        bool m_LayoutDirty = true;

    PROPERTY(READONLY, NOSERIALIZE, CATEGORY("Layout"))
        uint32 m_LayoutVersion = 0;
#pragma endregion
};

END