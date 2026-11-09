#pragma once

#include "ImGuiInterface.h"

BEGIN(Editor)
class ResourceEditorInterface : public IImGui
{
public:
	virtual ~ResourceEditorInterface() = default;
	virtual bool IsSupported(const std::filesystem::path& assetPath) const = 0;
	virtual bool HasUnsavedResourceChanges() const { return false; }
    virtual void SetTargetResource(const std::filesystem::path& assetPath)
    {
        m_TargetResourcePath = assetPath;
    }

    const std::filesystem::path& GetTargetResource() const
    {
        return m_TargetResourcePath;
    }
    EPanelMenuCategory GetPanelMenuCategory() const override { return EPanelMenuCategory::Assets; }
public:
	void Draw() final;
protected:
    virtual ImVec2 GetInitialResourceWindowSize() const { return {}; }
    virtual ImGuiWindowFlags GetResourceWindowFlags() const { return ImGuiWindowFlags_None; }
    virtual string GetResourceWindowId() const { return WStrToStr(GetName()); }
    virtual string BuildResourceWindowTitle() const;
    virtual void DrawCustomToolbar();
    virtual void DrawCustomQuickAccess() {}
    virtual void DrawCustomContent() = 0;
    virtual void OnResourceWindowVisibility(bool) {}
    virtual void OnResourceCloseRequested() { m_Open = false; }
    virtual void AfterResourceWindow() {}

    // A panel can scope its own draw context around this call.
    virtual void DrawResourceFrame();
    void DrawCopyResourcePath();

    std::filesystem::path m_TargetResourcePath;
};
END
