#pragma once

#include "CommandHistory.h"
#include <optional>

namespace Editor
{
    template<class Snapshot>
    class ResourceDocumentHistory
    {
    public:
        void Clear()
        {
            m_Commands.Clear();
            m_Current.reset();
            m_PendingEdit = false;
            MarkSaved();
        }

        void Reset(Snapshot initial)
        {
            Clear();
            m_Current = std::make_shared<const Snapshot>(std::move(initial));
        }

        void MarkEdited() { m_PendingEdit = true; }
        bool HasPendingEdit() const { return m_PendingEdit; }
        bool IsDirty() const { return m_PendingEdit || !m_SavedState || m_Commands.GetCurrentStateId() != *m_SavedState; }
        void MarkSaved() { m_SavedState = m_Commands.GetCurrentStateId(); }

        template<class Apply>
        bool Record(Snapshot snapshot, Apply apply, std::string label = {})
        {
            if (!m_PendingEdit) return true;
            auto next = std::make_shared<const Snapshot>(std::move(snapshot));
            if (!m_Current)
            {
                m_Current = std::move(next);
                m_PendingEdit = false;
                m_SavedState.reset();
                return true;
            }
            auto restore = [this, apply](const std::shared_ptr<const Snapshot>& value) {
                if (!apply(*value)) return false;
                m_Current = value;
                return true;
            };

            if (!m_Commands.Record(std::make_unique<CallbackEditorCommand>(std::move(label),
                [restore, next] { return restore(next); }, [restore, previous = m_Current] { return restore(previous); })))
                return false;

            m_Current = std::move(next);
            m_PendingEdit = false;
            return true;
        }

        bool CanUndo() const { return m_PendingEdit || m_Commands.CanUndo(); }
        bool CanRedo() const { return !m_PendingEdit && m_Commands.CanRedo(); }
        bool Undo() { return !m_PendingEdit && m_Commands.Undo(); }
        bool Redo() { return !m_PendingEdit && m_Commands.Redo(); }

    private:
        CommandHistory m_Commands;
        std::shared_ptr<const Snapshot> m_Current;
        std::optional<uint64> m_SavedState = 0;
        bool m_PendingEdit = false;
    };
}
