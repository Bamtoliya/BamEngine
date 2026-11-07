#include "ModuleManager.h"
#include "Logger.h"
#include "Functions.h"

IMPLEMENT_SINGLETON(ModuleManager)

EResult ModuleManager::Initialize(void* arg)
{
    BAM_LOG(Info, "Engine","ModuleManager", "ModuleManager initialized.");
    return EResult::Success;
}

void ModuleManager::Free()
{
    UnloadAllModules();
}

EResult ModuleManager::LoadModule(const std::wstring& moduleName, const std::filesystem::path& dllPath)
{
    if (moduleName.empty() || dllPath.empty())
        return EResult::InvalidArgument;

    if (m_Modules.find(moduleName) != m_Modules.end())
    {
        BAM_LOG(Warn, "Engine","ModuleManager", "Module already loaded: {}", WStrToStr(moduleName));
        return EResult::AlreadyInitialized;
    }

    std::error_code ec;
    if (!std::filesystem::exists(dllPath, ec))
    {
        BAM_LOG(Error, "Engine","ModuleManager", "DLL file not found: {}", dllPath.string());
        return EResult::FileNotFound;
    }

    HMODULE hModule = LoadLibraryW(dllPath.c_str());
    if (!hModule)
    {
        const DWORD error = GetLastError();
        BAM_LOG(Error, "Engine","ModuleManager", "LoadLibraryW failed (code={}): {}", error, dllPath.string());
        return EResult::Fail;
    }

    auto createFunc = reinterpret_cast<CreateModuleFunc>(GetProcAddress(hModule, "CreateModule"));
    auto destroyFunc = reinterpret_cast<DestroyModuleFunc>(GetProcAddress(hModule, "DestroyModule"));

    if (!createFunc || !destroyFunc)
    {
        BAM_LOG(Error, "Engine","ModuleManager", "Entry point functions not found in: {}", dllPath.string());
        FreeLibrary(hModule);
        return EResult::Fail;
    }

    IModule* instance = createFunc();
    if (!instance)
    {
        BAM_LOG(Error, "Engine","ModuleManager", "CreateModule returned nullptr.");
        FreeLibrary(hModule);
        return EResult::Fail;
    }

    // 모듈 초기화 수명주기 훅 호출
    instance->OnLoad();

    LoadedModuleInfo info;
    info.Name = moduleName;
    info.DllPath = dllPath;
    info.Handle = hModule;
    info.Instance = instance;
    info.DestroyFunc = destroyFunc;

    m_Modules.emplace(moduleName, std::move(info));
    BAM_LOG(Info, "Engine","ModuleManager", "Module loaded: {} (from {})",
        WStrToStr(moduleName), dllPath.string());

    return EResult::Success;
}

EResult ModuleManager::UnloadModule(const std::wstring& moduleName)
{
    auto it = m_Modules.find(moduleName);
    if (it == m_Modules.end())
        return EResult::Fail;

    LoadedModuleInfo& info = it->second;

    if (info.Instance)
    {
        info.Instance->OnUnload();
        if (info.DestroyFunc)
            info.DestroyFunc(info.Instance);
        info.Instance = nullptr;
    }

    if (info.Handle)
    {
        FreeLibrary(info.Handle);
        info.Handle = nullptr;
    }

    BAM_LOG(Info, "Engine","ModuleManager", "Module unloaded: {}", WStrToStr(moduleName));
    m_Modules.erase(it);

    return EResult::Success;
}

EResult ModuleManager::ReloadModule(const std::wstring& moduleName, const std::filesystem::path& newDllPath)
{
    auto it = m_Modules.find(moduleName);
    if (it == m_Modules.end())
        return EResult::Fail;

    BAM_LOG(Info, "Engine","ModuleManager", "Reloading module: {}", WStrToStr(moduleName));

    // 1. 핫리로드 전처리 훅
    if (it->second.Instance)
        it->second.Instance->OnPreHotReload();

    // 2. 기존 모듈 언로드
    UnloadModule(moduleName);

    // 3. 새 DLL 로드
    const EResult loadResult = LoadModule(moduleName, newDllPath);
    if (loadResult != EResult::Success)
    {
        BAM_LOG(Error, "Engine","ModuleManager", "Failed to reload module: {}", ResultToString(loadResult));
        return loadResult;
    }

    // 4. 핫리로드 후처리 훅
    auto newIt = m_Modules.find(moduleName);
    if (newIt != m_Modules.end() && newIt->second.Instance)
        newIt->second.Instance->OnPostHotReload();
                
    BAM_LOG(Info, "Engine","ModuleManager", "Module reload completed: {}", WStrToStr(moduleName));
    return EResult::Success;
}

void ModuleManager::UnloadAllModules()
{
    std::vector<std::wstring> names;
    names.reserve(m_Modules.size());
    for (const auto& [name, _] : m_Modules)
        names.push_back(name);

    for (const auto& name : names)
        UnloadModule(name);

    m_Modules.clear();
}

IModule* ModuleManager::GetModule(const std::wstring& moduleName) const
{
    const auto it = m_Modules.find(moduleName);
    return (it != m_Modules.end()) ? it->second.Instance : nullptr;
}

bool ModuleManager::IsModuleLoaded(const std::wstring& moduleName) const
{
    return m_Modules.find(moduleName) != m_Modules.end();
}

const LoadedModuleInfo* ModuleManager::GetModuleInfo(const std::wstring& moduleName) const
{
    const auto it = m_Modules.find(moduleName);
    return (it != m_Modules.end()) ? &it->second : nullptr;
}