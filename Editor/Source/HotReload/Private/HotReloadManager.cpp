#include "HotReloadManager.h"
#include "ModuleHotReloadManager.h"
#include "ShaderHotReloadManager.h"
#include "AssetHotReloadManager.h"
#include "ModuleBuildManager.h"
#include "PlatformPath.h"

static std::filesystem::path ResolveProjectRoot()
{
    return Engine::PlatformPath::GetProjectRoot();
}
static std::filesystem::path ResolveBuildDirectory(const std::filesystem::path& projectRoot)
{
    return projectRoot / "out/build/x64-debug";
}

IMPLEMENT_SINGLETON(HotReloadManager)
#pragma region Constructor&Destructor
EResult HotReloadManager::Initialize(void*)
{
    m_ModuleReloader = ModuleHotReloadManager::Create();
    if (!m_ModuleReloader) return EResult::Fail;
    m_ShaderReloader = ShaderHotReloadManager::Create();
    if (!m_ShaderReloader) { Free(); return EResult::Fail; }
    m_AssetReloader = AssetHotReloadManager::Create();
    if (!m_AssetReloader) { Free(); return EResult::Fail; }
    // 실제 프로젝트 루트와 CMake 빌드 디렉터리를 찾아 ModuleBuildManager 생성
    const std::filesystem::path projectRoot = ResolveProjectRoot();
    const std::filesystem::path buildDir = ResolveBuildDirectory(projectRoot);
    m_ModuleBuilder = ModuleBuildManager::Create(projectRoot, buildDir);
    if (!m_ModuleBuilder) { Free(); return EResult::Fail; }
    return EResult::Success;
}

void HotReloadManager::Free()
{
    Safe_Release(m_AssetReloader);
    Safe_Release(m_ShaderReloader);
    Safe_Release(m_ModuleReloader);
    Safe_Release(m_ModuleBuilder);

    __super::Free();
}
#pragma endregion

void HotReloadManager::Update()
{
    IHotReloader* reloaders[] = { m_ModuleReloader, m_ShaderReloader, m_AssetReloader, m_ModuleBuilder };
    for (IHotReloader* reloader : reloaders)
    {
        if (reloader) reloader->Update();
    }
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

ModuleBuildManager& HotReloadManager::GetModuleBuilder()
{
    assert(m_ModuleBuilder != nullptr);
    return *m_ModuleBuilder;
}
#pragma endregion