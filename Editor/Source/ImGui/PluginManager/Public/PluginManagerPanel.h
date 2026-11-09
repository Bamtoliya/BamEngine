#pragma once

#include "ImGuiInterface.h"
#include <string>
#include <vector>

BEGIN(Editor)

enum class EPluginCategory : uint8
{
	All,
	GameLogic,
	EditorTools,
	Experimental
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
	std::string Author;
	std::string Description;
	const char* Icon = nullptr;
	EPluginCategory Category = EPluginCategory::GameLogic;
	bool IsGameModule = false;
	bool IsShaderModule = false;
	bool IsAssetModule = false;
	bool IsResourcePlugin = false;
	std::wstring AssociatedPanelName;
};

class PluginManagerPanel final : public IImGui
{
public:
	PluginManagerPanel();
	virtual ~PluginManagerPanel() = default;

	void Draw() override;
	EPanelMenuCategory GetPanelMenuCategory() const override { return EPanelMenuCategory::Diagnostics; }

protected:
	EResult OnInitialize(void* arg) override;

private:
	void InitModuleList();
	void DrawMasterList();
	void DrawDetailView();
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
};

END
