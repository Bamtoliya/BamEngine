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
	virtual void Draw() BAM_PURE;
protected:
    string BuildResourceWindowTitle() const
    {
        return GetDisplayName() + " - " + WStrToStr(GetTargetResource().filename().wstring()) + "###" + WStrToStr(GetName());
    }
protected:
    std::filesystem::path m_TargetResourcePath;
};
END
