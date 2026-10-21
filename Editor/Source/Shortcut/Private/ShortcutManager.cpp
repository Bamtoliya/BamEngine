#include "ShortcutManager.h"
#include "InputManager.h"
#include "imgui.h"
#include "Editor_Macros.h"
#include <glaze/glaze.hpp>
#include <filesystem>

IMPLEMENT_SINGLETON(ShortcutManager)

EResult ShortcutManager::Initialize(void* arg)
{
    return EResult::Success;
}

void ShortcutManager::Free()
{
    m_Actions.clear();
    m_ActiveBindings.clear();
    __super::Free();
}

bool ShortcutManager::RegisterAction(const EditorAction& action)
{
    if (action.Id.empty() || m_Actions.contains(action.Id))
        return false;

    m_Actions.emplace(action.Id, action);
    m_ActiveBindings.emplace(action.Id, action.DefaultChord);
    return true;
}

bool ShortcutManager::Rebind(const std::string& actionId, const ShortcutChord& newChord)
{
    auto it = m_ActiveBindings.find(actionId);
    if (it == m_ActiveBindings.end())
        return false;

    it->second = newChord;
    return true;
}

void ShortcutManager::ResetBinding(const std::string& actionId)
{
    auto actionIt = m_Actions.find(actionId);
    auto bindingIt = m_ActiveBindings.find(actionId);
    if (actionIt != m_Actions.end() && bindingIt != m_ActiveBindings.end())
    {
        bindingIt->second = actionIt->second.DefaultChord;
    }
}

void ShortcutManager::ResetAllBindings()
{
    for (auto& [id, chord] : m_ActiveBindings)
    {
        auto actionIt = m_Actions.find(id);
        if (actionIt != m_Actions.end())
            chord = actionIt->second.DefaultChord;
    }
}

std::string ShortcutManager::FindConflict(
    const ShortcutChord& chord, EShortcutContext context, const std::string& excludeActionId) const
{
    if (!chord.IsValid())
        return "";

    for (const auto& [id, action] : m_Actions)
    {
        if (id == excludeActionId)
            continue;

        auto bindingIt = m_ActiveBindings.find(id);
        if (bindingIt == m_ActiveBindings.end() || bindingIt->second != chord)
            continue;

        // Global 문맥이거나 동일한 Local 문맥인 경우 충돌로 판정
        if (action.Context == EShortcutContext::Global || context == EShortcutContext::Global || action.Context == context)
        {
            return id;
        }
    }
    return "";
}

void ShortcutManager::ProcessShortcuts(EShortcutContext activeContext, bool suppressed)
{
    if (suppressed)
        return;
    auto& input = Engine::InputManager::Get();
    // 1. Local Context 검사
    if (activeContext != EShortcutContext::Global)
    {
        for (const auto& [id, chord] : m_ActiveBindings)
        {
            const auto* action = FindAction(id);
            if (!action || action->Context != activeContext) continue;
            if (chord.Evaluate(input))
            {
                if (!action->CanExecute || action->CanExecute())
                {
                    if (action->ExecuteHandler) action->ExecuteHandler();
                    return;
                }
            }
        }
    }
    // 2. Global Context 검사
    for (const auto& [id, chord] : m_ActiveBindings)
    {
        const auto* action = FindAction(id);
        if (!action || action->Context != EShortcutContext::Global) continue;
        if (chord.Evaluate(input))
        {
            if (!action->CanExecute || action->CanExecute())
            {
                if (action->ExecuteHandler) action->ExecuteHandler();
                return;
            }
        }
    }
}

bool ShortcutManager::ExecuteAction(const std::string& actionId)
{
    const auto* action = FindAction(actionId);
    if (!action || !action->ExecuteHandler)
        return false;
    if (action->CanExecute && !action->CanExecute())
        return false;
    action->ExecuteHandler();
    return true;
}

bool ShortcutManager::CanExecuteAction(const std::string& actionId) const
{
    const auto* action = FindAction(actionId);
    return action && (!action->CanExecute || action->CanExecute());
}
const char* ShortcutManager::GetShortcutHint(const std::string& actionId) const
{
    auto it = m_ActiveBindings.find(actionId);
    if (it == m_ActiveBindings.end() || !it->second.IsValid())
        return nullptr;
    m_HintStringCache[actionId] = it->second.ToString();
    return m_HintStringCache[actionId].c_str();
}

ShortcutChord ShortcutManager::GetActiveChord(const std::string& actionId) const
{
    auto it = m_ActiveBindings.find(actionId);
    return (it != m_ActiveBindings.end()) ? it->second : ShortcutChord();
}

const EditorAction* ShortcutManager::FindAction(const std::string& actionId) const
{
    auto it = m_Actions.find(actionId);
    return (it != m_Actions.end()) ? &it->second : nullptr;
}

bool ShortcutManager::SaveToFile(const std::filesystem::path& path) const
{
    const std::filesystem::path targetPath = path.empty() ? GetDefaultConfigPath() : path;
    if (targetPath.has_parent_path())
        std::filesystem::create_directories(targetPath.parent_path());
    // 기본값과 달라진 액션만 추출 (Delta Storage)
    std::unordered_map<std::string, std::string> overrides;
    for (const auto& [id, action] : m_Actions)
    {
        auto it = m_ActiveBindings.find(id);
        if (it != m_ActiveBindings.end() && it->second != action.DefaultChord)
        {
            overrides[id] = it->second.ToString(); // "None" 또는 "Ctrl+Shift+Z"
        }
    }
    std::string buffer;
    auto ec = glz::write_file_json < glz::opts{ .prettify = true } > (overrides, targetPath.string(), buffer);
    return ec == glz::error_code::none;
}
bool ShortcutManager::LoadFromFile(const std::filesystem::path& path)
{
    const std::filesystem::path targetPath = path.empty() ? GetDefaultConfigPath() : path;
    if (!std::filesystem::exists(targetPath))
        return true; // 설정 파일이 없으면 기본 바인딩 유지
    std::unordered_map<std::string, std::string> overrides;
    std::string buffer;
    auto ec = glz::read_file_json(overrides, targetPath.string(), buffer);
    if (ec != glz::error_code::none)
        return false;
    for (const auto& [id, chordStr] : overrides)
    {
        if (m_Actions.contains(id))
        {
            m_ActiveBindings[id] = ShortcutChord::FromString(chordStr);
        }
    }
    return true;
}
std::filesystem::path ShortcutManager::GetDefaultConfigPath()
{
    return std::filesystem::path(CONFIG_PATH) / "EditorShortcuts.json";
}