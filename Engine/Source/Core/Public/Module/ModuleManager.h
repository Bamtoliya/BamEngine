#pragma once

#include "Engine_Includes.h"
#include "Base.h"
#include "ModuleInterface.h"

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

BEGIN(Engine)

struct LoadedModuleInfo
{
    std::wstring Name;
    std::filesystem::path DllPath;
    HMODULE Handle = nullptr;
    IModule* Instance = nullptr;
    DestroyModuleFunc DestroyFunc = nullptr;
};

class ENGINE_API ModuleManager final : public Base
{
    DECLARE_SINGLETON(ModuleManager)

private:
    ModuleManager() = default;
    virtual ~ModuleManager() = default;
    EResult Initialize(void* arg = nullptr);

public:
    void Free() override;

    // 동적 모듈 로드 (LoadLibraryW -> CreateModule -> OnLoad)
    EResult LoadModule(const std::wstring& moduleName, const std::filesystem::path& dllPath);

    // 동적 모듈 언로드 (OnUnload -> DestroyModule -> FreeLibrary)
    EResult UnloadModule(const std::wstring& moduleName);

    // 모듈 교체 (OnPreHotReload -> 언로드 -> 새 경로 로드 -> OnPostHotReload)
    EResult ReloadModule(const std::wstring& moduleName, const std::filesystem::path& newDllPath);

    // 등록된 모든 모듈 일괄 언로드
    void UnloadAllModules();

    [[nodiscard]] IModule* GetModule(const std::wstring& moduleName) const;
    [[nodiscard]] bool IsModuleLoaded(const std::wstring& moduleName) const;
    [[nodiscard]] const LoadedModuleInfo* GetModuleInfo(const std::wstring& moduleName) const;
    [[nodiscard]] const std::unordered_map<std::wstring, LoadedModuleInfo>& GetAllModules() const { return m_Modules; }

private:
    std::unordered_map<std::wstring, LoadedModuleInfo> m_Modules;
};

END