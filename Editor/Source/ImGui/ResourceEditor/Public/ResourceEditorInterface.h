#pragma once

#include "ImGuiInterface.h"

BEGIN(Editor)
class AssetPathRelocation;
class ResourceEditorInterface : public IImGui
{
public:
	virtual ~ResourceEditorInterface() = default;
	virtual bool IsSupported(const std::filesystem::path& assetPath) const = 0;
	virtual bool HasUnsavedResourceChanges() const { return false; }
    virtual void SaveResource() {}
    virtual bool CanUndoDocument() const { return false; }
    virtual bool CanRedoDocument() const { return false; }
    virtual void UndoDocument() {}
    virtual void RedoDocument() {}
    virtual void SetTargetResource(const std::filesystem::path& assetPath)
    {
        m_TargetResourcePath = assetPath;
    }

    const std::filesystem::path& GetTargetResource() const
    {
        return m_TargetResourcePath;
    }
    virtual std::vector<std::filesystem::path> GetRetainedResourcePaths() const { return { m_TargetResourcePath }; }
    virtual bool PrepareAssetPathRelocation(AssetPathRelocation& relocation);
    EPanelMenuCategory GetPanelMenuCategory() const override { return EPanelMenuCategory::Assets; }
public:
	void Draw() final;
	void SetOpen(bool open) final;
	void RunWithDiscardConfirmation(std::function<void()> action, bool offerSave = true);
protected:
    bool PrepareTargetPathRelocation(AssetPathRelocation& relocation);
    virtual ImVec2 GetInitialResourceWindowSize() const { return {}; }
    virtual ImGuiWindowFlags GetResourceWindowFlags() const { return ImGuiWindowFlags_None; }
    virtual string GetResourceWindowId() const { return WStrToStr(GetName()); }
    virtual string BuildResourceWindowTitle() const;
    virtual void DrawCustomToolbar();
    virtual void DrawCustomQuickAccess() {}
    virtual void DrawCustomContent() = 0;
    virtual void OnResourceWindowVisibility(bool) {}
    virtual void ReleaseResourceDocument() {}
    virtual bool RestoreSavedResource() { return true; }
    virtual const char* GetSaveActionId() const { return nullptr; }
    virtual bool SavePendingResourceChanges();
    virtual bool FocusRetainedResource(const std::filesystem::path&) { return false; }
    void RequestResourceClose();
    void DrawCloseMenuItem();
    void DrawSaveMenuItem();
    void DrawSaveButton(EditorUI::ToolbarLayout& toolbar);
    static void DrawHistoryMenuItems(std::string_view actionPrefix);
    static void DrawHistoryButtons(EditorUI::ToolbarLayout& toolbar, std::string_view actionPrefix);
    static bool IsHistoryGestureActive();
    bool RestoreSavedChanges();

    // A panel can scope its own draw context around this call.
    virtual void DrawResourceFrame();
    void DrawCopyResourcePath();
private:
    friend class ImGuiManager;
    void DrawDiscardConfirmation();
    void CloseResourceDocument();
protected:
    std::filesystem::path m_TargetResourcePath;
private:
    std::function<void()> m_PendingDiscardAction;
    std::function<void()> m_ConfirmedResourceAction;
    bool m_OfferSave = false;
};
END
