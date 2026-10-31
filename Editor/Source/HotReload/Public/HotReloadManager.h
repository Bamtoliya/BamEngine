#pragma once

#include "HotReloaderInterface.h"

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
private:
    ModuleHotReloadManager* m_ModuleReloader = nullptr;
    ShaderHotReloadManager* m_ShaderReloader = nullptr;
    AssetHotReloadManager* m_AssetReloader = nullptr;
	ModuleBuildManager* m_ModuleBuilder = nullptr;
};

END