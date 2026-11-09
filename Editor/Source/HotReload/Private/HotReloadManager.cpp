#include "HotReloadManager.h"
#include "ModuleHotReloadManager.h"
#include "ShaderHotReloadManager.h"
#include "AssetHotReloadManager.h"
#include "ModuleBuildManager.h"
#include "PlatformPath.h"
#include "ImGuiManager.h"
#include "PluginResourceEditor.h"

static std::filesystem::path ResolveProjectRoot()
{
    return Engine::PlatformPath::GetProjectRoot();
}
static std::filesystem::path ResolveBuildDirectory(const std::filesystem::path& projectRoot)
{
    std::error_code ec;
    const std::filesystem::path exeDir = Engine::PlatformPath::GetExecutableDirectory();
    // 1. <buildDir>/bin/<Config>/Editor.exe 구조 역추적 (Visual Studio 멀티 구성 프리셋)
    const std::filesystem::path candidate1 = exeDir.parent_path().parent_path();
    if (std::filesystem::exists(candidate1 / "CMakeCache.txt", ec))
    {
        return candidate1;
    }
    // 2. <buildDir>/bin/Editor.exe 구조 역추적 (Ninja 등 단일 구성 프리셋)
    const std::filesystem::path candidate2 = exeDir.parent_path();
    if (std::filesystem::exists(candidate2 / "CMakeCache.txt", ec))
    {
        return candidate2;
    }
    // 3. 빌드 프리셋 기본 경로 Fallback
#ifdef _DEBUG
    return projectRoot / "out/build/x64-debug";
#else
    return projectRoot / "out/build/x64-release";
#endif
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
    std::error_code ec;
    if (ImGuiManager::IsCreated() && std::filesystem::is_regular_file(buildDir / "CMakeCache.txt", ec))
    {
        for (const auto* panel : ImGuiManager::Get().GetPluginResourceEditors())
        {
            const auto& source = panel->GetSourceDirectory();
            if (!std::filesystem::is_directory(projectRoot / source, ec))
                continue;

            const auto& target = panel->GetTarget();
            auto* builder = ModuleBuildManager::Create(
                projectRoot, buildDir, source, std::wstring(target.begin(), target.end()));
            if (!builder)
                continue;

            builder->SetBuildCompletedCallback([target](bool success, const std::string&)
            {
                if (!success || !ImGuiManager::IsCreated())
                    return;
                if (auto* editor = ImGuiManager::Get().GetPluginResourceEditor(target))
                    editor->RequestReload();
            });
            m_EditorPluginBuilders.emplace(target, builder);
        }
    }
    return EResult::Success;
}

void HotReloadManager::Free()
{
    for (auto& [target, builder] : m_EditorPluginBuilders)
        Safe_Release(builder);
    m_EditorPluginBuilders.clear();
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
    for (const auto& [target, builder] : m_EditorPluginBuilders)
        builder->Update();
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

ModuleBuildManager* HotReloadManager::GetEditorPluginBuilder(const std::string& target) const
{
    const auto found = m_EditorPluginBuilders.find(target);
    return found != m_EditorPluginBuilders.end() ? found->second : nullptr;
}
