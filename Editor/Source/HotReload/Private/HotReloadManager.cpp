#include "HotReloadManager.h"
#include "ModuleHotReloadManager.h"
#include "ShaderHotReloadManager.h"
#include "AssetHotReloadManager.h"

IMPLEMENT_SINGLETON(HotReloadManager)
#pragma region Constructor&Destructor
EResult HotReloadManager::Initialize(void*)
{
    m_ModuleReloader = ModuleHotReloadManager::Create();
    if (!m_ModuleReloader)
        return EResult::Fail;

    m_ShaderReloader = ShaderHotReloadManager::Create();
    if (!m_ShaderReloader)
    {
        Free();
        return EResult::Fail;
    }

    m_AssetReloader = AssetHotReloadManager::Create();
    if (!m_AssetReloader)
    {
        Free();
        return EResult::Fail;
    }

    return EResult::Success;
}

void HotReloadManager::Free()
{
    Safe_Release(m_AssetReloader);
    Safe_Release(m_ShaderReloader);
    Safe_Release(m_ModuleReloader);

    __super::Free();
}
#pragma endregion

void HotReloadManager::Update()
{
    IHotReloader* reloaders[] = { m_ModuleReloader, m_ShaderReloader, m_AssetReloader };

    for (IHotReloader* reloader : reloaders)
        reloader->Update();
}

#pragma region Getter
ModuleHotReloadManager& HotReloadManager::GetModuleReloader()
{
    assert(m_ModuleReloader != nullptr);
    return *m_ModuleReloader;
}

ShaderHotReloadManager& HotReloadManager::GetShaderReloader()
{
    assert(m_ShaderReloader != nullptr);
    return *m_ShaderReloader;
}

AssetHotReloadManager& HotReloadManager::GetAssetReloader()
{
    assert(m_AssetReloader != nullptr);
    return *m_AssetReloader;
}
#pragma endregion