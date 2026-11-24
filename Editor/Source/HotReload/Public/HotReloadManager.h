#pragma once

#include "HotReloaderInterface.h"
#include <filesystem>
#include <unordered_map>

namespace Engine { struct ProjectDescriptor; }

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
    void QuiesceProjectHotReload();
    void UnloadProjectModule();
    EResult RebindForProject(const std::filesystem::path& projectRoot, const Engine::ProjectDescriptor& descriptor);

public:
    [[nodiscard]] ModuleHotReloadManager& GetModuleReloader();
    [[nodiscard]] ShaderHotReloadManager& GetShaderReloader();
    [[nodiscard]] AssetHotReloadManager& GetAssetReloader();
	[[nodiscard]] ModuleBuildManager& GetModuleBuilder();
    [[nodiscard]] ModuleBuildManager* GetPluginBuilder(const std::string& target) const;
private:
    ModuleHotReloadManager* m_ModuleReloader = nullptr;
    ShaderHotReloadManager* m_ShaderReloader = nullptr;
    AssetHotReloadManager* m_AssetReloader = nullptr;
	ModuleBuildManager* m_ModuleBuilder = nullptr;
    std::unordered_map<std::string, ModuleBuildManager*> m_PluginBuilders;
};

END
