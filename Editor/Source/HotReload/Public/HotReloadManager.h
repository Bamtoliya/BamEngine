#pragma once

#include "HotReloaderInterface.h"
#include <unordered_map>

BEGIN(Editor)
class ModuleHotReloadManager;
class ShaderHotReloadManager;
class AssetHotReloadManager;
class ModuleBuildManager;
class HotReloadManager final : public Base
{
    DECLARE_SINGLETON(HotReloadManager)

private:
    HotReloadManager() = default;
    virtual ~HotReloadManager() = default;
    EResult Initialize(void* arg = nullptr);

public:
    void Free() override;
    void Update();

public:
    [[nodiscard]] ModuleHotReloadManager& GetModuleReloader();
    [[nodiscard]] ShaderHotReloadManager& GetShaderReloader();
    [[nodiscard]] AssetHotReloadManager& GetAssetReloader();
	[[nodiscard]] ModuleBuildManager& GetModuleBuilder();
    [[nodiscard]] ModuleBuildManager* GetEditorPluginBuilder(const std::string& target) const;
private:
    ModuleHotReloadManager* m_ModuleReloader = nullptr;
    ShaderHotReloadManager* m_ShaderReloader = nullptr;
    AssetHotReloadManager* m_AssetReloader = nullptr;
	ModuleBuildManager* m_ModuleBuilder = nullptr;
    std::unordered_map<std::string, ModuleBuildManager*> m_EditorPluginBuilders;
};

END
