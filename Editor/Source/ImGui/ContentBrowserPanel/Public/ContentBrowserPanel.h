#pragma once

#include "Editor_Includes.h"
#include "ImGuiInterface.h"
#include <filesystem>

BEGIN(Editor)
class ContentBrowserPanel : public ImGuiInterface
{
public:
	ContentBrowserPanel() = default;
	virtual ~ContentBrowserPanel() = default;
	virtual void Draw() override;
private:
	filesystem::path m_RootPath;
	filesystem::path m_CurrentDirectory;

	f32 m_ThumbnailSize = 64.0f;
	f32 m_Padding = 16.0f;
}
END