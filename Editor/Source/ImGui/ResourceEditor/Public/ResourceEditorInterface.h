#pragma once

#include "ImGuiInterface.h"
#include <filesystem>

BEGIN(Editor)
class ResourceEditorInterface : public ImGuiInterface
{
public:
	virtual ~ResourceEditorInterface() = default;
	virtual bool IsSupported(const std::filesystem::path& assetPath) const = 0;
    virtual void SetTargetResource(const std::filesystem::path& assetPath)
    {
        m_TargetResourcePath = assetPath;
    }

    const std::filesystem::path& GetTargetResource() const
    {
        return m_TargetResourcePath;
    }
public:
	virtual void Draw() BAM_PURE;
protected:
    std::filesystem::path m_TargetResourcePath;
};
END