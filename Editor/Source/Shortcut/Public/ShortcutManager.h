#pragma once

#include "Editor_Includes.h"
#include "ShortcutTypes.h"
#include <unordered_map>

BEGIN(Editor)

class ShortcutManager : public Base
{
    DECLARE_SINGLETON(ShortcutManager)

private:
    ShortcutManager() = default;
    virtual ~ShortcutManager() = default;
    EResult Initialize(void* arg = nullptr);
    virtual void Free() override;

public:
    bool RegisterAction(const EditorAction& action);
    bool Rebind(const std::string& actionId, const ShortcutChord& newChord);
    void ResetBinding(const std::string& actionId);
    void ResetAllBindings();

    std::string FindConflict(const ShortcutChord& chord, EShortcutContext context, const std::string& excludeActionId = "") const;

    // ImGui 의존성 없이, 외부에서 억제 여부(suppressed)를 받아 순수하게 판정
    void ProcessShortcuts(EShortcutContext activeContext = EShortcutContext::Global, bool suppressed = false);

    // 액션 실행 및 가능 여부 (UI 클릭 시 호출)
    bool ExecuteAction(const std::string& actionId);
    bool CanExecuteAction(const std::string& actionId) const;

    // 단축키 힌트 문자열 반환 (ImGui가 소비할 수 있도록 C 문자열 포인터 캐시 제공)
    const char* GetShortcutHint(const std::string& actionId) const;
    ShortcutChord GetActiveChord(const std::string& actionId) const;
    const EditorAction* FindAction(const std::string& actionId) const;

    bool SaveToFile(const std::filesystem::path& path = "") const;
    bool LoadFromFile(const std::filesystem::path& path = "");

    const std::unordered_map<std::string, EditorAction>& GetActions() const { return m_Actions; }
    const std::unordered_map<std::string, ShortcutChord>& GetActiveBindings() const { return m_ActiveBindings; }
    static std::filesystem::path GetDefaultConfigPath();

private:
    std::unordered_map<std::string, EditorAction> m_Actions;
    std::unordered_map<std::string, ShortcutChord> m_ActiveBindings;
    mutable std::unordered_map<std::string, std::string> m_HintStringCache;
};

END