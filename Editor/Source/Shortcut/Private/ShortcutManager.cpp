#include "ShortcutManager.h"
#include "InputManager.h"
#include "Editor_Macros.h"
#include "ConfigManager.h"

IMPLEMENT_SINGLETON(ShortcutManager)

EResult ShortcutManager::Initialize(void* arg)
{
    if (!arg)
        return EResult::InvalidArgument;

    const auto& desc = *static_cast<const ShortcutManagerDesc*>(arg);
    m_ConfigPath = desc.ConfigPath;
    m_LegacyPath = desc.LegacyPath;
    m_CanSaveConfig = false;

    return EResult::Success;
}

void ShortcutManager::Free()
{
    m_StoredOverrides.clear();
    m_HintStringCache.clear();
    m_ConfigPath.clear();
    m_LegacyPath.clear();
    m_CanSaveConfig = false;
    m_Actions.clear();
    m_ActiveBindings.clear();
    __super::Free();
}

bool ShortcutManager::RegisterAction(const EditorAction& action)
{
    if (action.Id.empty() || m_Actions.contains(action.Id))
        return false;

    m_Actions.emplace(action.Id, action);
    auto chord = action.DefaultChord;
    const auto stored = m_StoredOverrides.find(action.Id);
    if (stored != m_StoredOverrides.end())
    {
        const auto overrideChord = ShortcutChord::FromString(stored->second);
        if (overrideChord.ToString() == stored->second)
            chord = overrideChord;
    }
    m_ActiveBindings.emplace(action.Id, chord);
    return true;
}

bool ShortcutManager::Rebind(const std::string& actionId, const ShortcutChord& newChord, bool resolveConflict)
{
    auto it = m_ActiveBindings.find(actionId);
    if (it == m_ActiveBindings.end())
        return false;
    if (newChord.IsValid() && resolveConflict)
    {
        const auto* action = FindAction(actionId);
        if (action)
        {
            const std::string conflictId = FindConflict(newChord, action->Context, actionId);
            if (!conflictId.empty())
            {
                auto conflictIt = m_ActiveBindings.find(conflictId);
                if (conflictIt != m_ActiveBindings.end())
                    conflictIt->second = ShortcutChord(); // 기존 액션 단축키 해제
            }
        }
    }
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

    const auto* excluded = FindAction(excludeActionId);
    for (const auto& [id, action] : m_Actions)
    {
        if (id == excludeActionId)
            continue;

        auto bindingIt = m_ActiveBindings.find(id);
        if (bindingIt == m_ActiveBindings.end() || bindingIt->second != chord)
            continue;

        // Local actions override Global actions; only actions in the same context compete.
        if (action.Context == context && (!excluded || action.PanelScope == excluded->PanelScope))
        {
            return id;
        }
    }
    return "";
}

void ShortcutManager::ProcessShortcuts(EShortcutContext activeContext, bool suppressed, std::wstring_view activePanel)
{
    if (suppressed || IsCapturing() || m_SuppressProcessOnce)
    {
        m_SuppressProcessOnce = false;
        return;
    }
    auto& input = Engine::InputManager::Get();
    // 1. Local Context 검사
    if (activeContext != EShortcutContext::Global)
    {
        for (const auto& [id, chord] : m_ActiveBindings)
        {
            const auto* action = FindAction(id);
            if (!action || action->Context != activeContext) continue;
            if (!action->PanelScope.empty() && action->PanelScope != activePanel) continue;
            if (chord.Evaluate(input, action->AllowRepeat))
            {
                if (!action->CanExecute || action->CanExecute())
                {
                    if (action->ExecuteHandler) action->ExecuteHandler();
                }
                // A disabled local command must not fall through to a scene command.
                return;
            }
        }
    }
    // 2. Global Context 검사
    for (const auto& [id, chord] : m_ActiveBindings)
    {
        const auto* action = FindAction(id);
        if (!action || action->Context != EShortcutContext::Global) continue;
        if (chord.Evaluate(input, action->AllowRepeat))
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

bool ShortcutManager::SaveToFile() const
{
    if (!m_CanSaveConfig || m_ConfigPath.empty())
    {
        EDITOR_LOG_WARN("Shortcut config saving is disabled; the existing file was preserved.");
        return false;
    }

    auto overrides = m_StoredOverrides;

    for (const auto& [id, action] : m_Actions)
    {
        const auto found = m_ActiveBindings.find(id);

        if (found == m_ActiveBindings.end() || found->second == action.DefaultChord)
            overrides.erase(id);
        else
            overrides[id] = found->second.ToString();
    }

    archive::JsonArchive writer;

    for (const auto& [id, chord] : overrides)
    {
        const auto result = writer.Write(id, chord);
        if (!result)
        {
            EDITOR_LOG_ERROR("Shortcut serialization failed: code={}, field={}",
                static_cast<int>(result.Code), result.Field);
            return false;
        }
    }

    const auto result = Engine::ConfigManager::SaveDocument(writer, m_ConfigPath);
    if (!result)
    {
        EDITOR_LOG_ERROR("Shortcut config save failed: code={}, field={}, filesystem={}",
            static_cast<int>(result.Error.Code), result.Error.Field, result.FileError.message());
        return false;
    }

    return true;
}
bool ShortcutManager::LoadFromFile()
{
    m_CanSaveConfig = false;

    if (m_ConfigPath.empty())
    {
        EDITOR_LOG_WARN("Shortcut config path is unavailable; current bindings were retained.");
        return false;
    }

    std::error_code error;
    auto source = m_ConfigPath;
    bool exists = std::filesystem::exists(source, error);

    if (!error && !exists && !m_LegacyPath.empty())
    {
        source = m_LegacyPath;
        exists = std::filesystem::exists(source, error);
    }

    if (error)
    {
        EDITOR_LOG_WARN("Shortcut config lookup failed: {}", error.message());
        return false;
    }

    if (!exists)
    {
        ResetAllBindings();
        m_StoredOverrides.clear();
        m_HintStringCache.clear();
        m_CanSaveConfig = true;
        return true;
    }

    archive::JsonArchive reader;
    auto result = reader.LoadFromFile(source);
    if (!result)
    {
        EDITOR_LOG_WARN("Shortcut config read failed: code={}, field={}",
            static_cast<int>(result.Code), result.Field);
        return false;
    }

    std::vector<std::string> ids;
    result = reader.GetFieldNames(ids);
    if (!result)
    {
        EDITOR_LOG_WARN("Shortcut config fields could not be read.");
        return false;
    }

    std::unordered_map<std::string, std::string> overrides;
    auto candidate = m_ActiveBindings;

    for (const auto& [id, action] : m_Actions)
        candidate[id] = action.DefaultChord;

    for (const auto& id : ids)
    {
        std::string text;
        result = reader.Read(id, text);

        if (!result)
        {
            EDITOR_LOG_WARN("Invalid shortcut config field: {}", id);
            return false;
        }

        overrides.emplace(id, text);

        if (!m_Actions.contains(id))
            continue;

        const auto chord = ShortcutChord::FromString(text);
        if (chord.ToString() != text)
        {
            EDITOR_LOG_WARN("Invalid shortcut chord: action={}, chord={}", id, text);
            return false;
        }

        candidate[id] = chord;
    }

    m_ActiveBindings = std::move(candidate);
    m_StoredOverrides = std::move(overrides);
    m_HintStringCache.clear();
    m_CanSaveConfig = true;

    return true;
}
