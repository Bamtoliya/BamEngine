#pragma once

#include "ImGuiInterface.h"
#include <filesystem>
#include <optional>
#include <string>
#include <vector>
#include <memory>

BEGIN(Editor)
enum class EPluginCategory : uint8
{
	All,
	GameLogic,
	EditorTools
};

enum class EPluginStatus : uint8
{
	Loaded,
	Unloaded,
	Compiling,
	Failed
};

struct PluginModuleItem
{
	std::string Id;
	std::string DisplayName;
	std::string Version;
	const char* Icon = nullptr;
	EPluginCategory Category = EPluginCategory::GameLogic;
	bool IsGameModule = false;
	bool IsShaderModule = false;
	bool IsAssetModule = false;
	bool IsResourcePlugin = false;
	std::optional<std::size_t> HostIndex;
};

class PluginManagerPanel final : public IImGui
{
public:
	PluginManagerPanel();
	virtual ~PluginManagerPanel() = default;

	void Draw() override;
	void CaptureState(EditorWorkspace& workspace) const override;
	void RestoreState(const EditorWorkspace& workspace) override;
	EPanelMenuCategory GetPanelMenuCategory() const override { return EPanelMenuCategory::Diagnostics; }
protected:
	EResult OnInitialize(void* arg) override;

private:
	void InitModuleList();
	void RefreshPackages();
	void DrawMasterList(float width);
	void DrawDetailView();
	void DrawApprovalDialog();
	void DrawCreatePluginDialog();
	void DrawHeaderToolbar(const PluginModuleItem& item);
	void DrawMetadataTable(const PluginModuleItem& item);
	void DrawProvidedFeatures(const PluginModuleItem& item);
	void DrawBuildConsole(const PluginModuleItem& item);
	void DrawStatusBadge(EPluginStatus status);
	EPluginStatus DetermineModuleStatus(const PluginModuleItem& item) const;

private:
	std::vector<PluginModuleItem> m_Modules;
	int m_SelectedIndex = 0;
	EPluginCategory m_FilterCategory = EPluginCategory::All;
	char m_SearchFilter[128] = "";
	bool m_AutoScrollLog = true;
	float m_PreferredSidebarWidth = 270.0f;
	char m_NewPluginId[49] = "";
	char m_NewPluginName[128] = "";
	char m_NewPluginVersion[33] = "1.0.0";
	int m_NewPanelWidth = 640;
	int m_NewPanelHeight = 480;
	const char* m_CreateErrorKey = nullptr;
	std::filesystem::path m_PartialPluginDirectory;
};

END
