#pragma once

#include "EditorDocumentManager.h"

namespace Editor
{
    enum class EDocumentSaveStatus
    {
        Saved,
        NotSelected,
        Unavailable,
        Failed
    };

    enum class EDocumentActionChoice
    {
        Save,
        Discard,
        Cancel
    };

    struct EditorDocumentSaveResult
    {
        EditorDocumentId Id = InvalidEditorDocumentId;
        EDocumentSaveStatus Status = EDocumentSaveStatus::Unavailable;
    };

    class EditorDocumentController final
    {
    public:
        explicit EditorDocumentController(EditorDocumentManager& documents) : m_Documents(documents) {}

        std::optional<EditorDocumentSaveResult> ProcessPendingSave();

    public:
        using GuardedAction = std::function<Engine::EResult(const std::vector<EditorDocumentId>&)>;

        bool RequestAction(std::vector<EditorDocumentId> documents, GuardedAction action);
        std::optional<Engine::EResult> ProcessPendingAction();
        EditorDocumentId GetConfirmationDocumentId() const;
        bool ResolveConfirmation(EDocumentActionChoice choice);
        void NotifySaveResult(const EditorDocumentSaveResult& result);

        bool HasPendingAction() const { return m_PendingAction.has_value(); }
        bool IsBusy() const { return m_PendingAction.has_value() || m_PendingSave.has_value(); }
        bool RequestSave(EditorDocumentId id, bool saveAs = false);
        void CancelForDocument(EditorDocumentId id);

        bool IsSavePendingFor(EditorDocumentId id) const
        {
            return m_PendingSave && m_PendingSave->Id == id;
        }
    private:
        void CancelAction();

    private:
        struct SaveRequest
        {
            EditorDocumentId Id;
            bool SaveAs;
        };
        EditorDocumentManager& m_Documents;
        std::optional<SaveRequest> m_PendingSave;
    private:
        struct PendingAction
        {
            std::vector<EditorDocumentId> Documents;
            std::vector<EditorDocumentId> Discarded;
            GuardedAction Execute;
            std::size_t Cursor = 0;
            bool Started = false;
            bool WaitingSave = false;
        };
        std::optional<PendingAction> m_PendingAction;
    };
}