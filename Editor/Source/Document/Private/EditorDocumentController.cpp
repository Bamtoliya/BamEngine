#include "EditorDocumentController.h"

namespace Editor
{
    bool EditorDocumentController::RequestSave(EditorDocumentId id, bool saveAs)
    {
        const auto* document = m_Documents.Find(id);
        if (IsBusy() || !document || !document->CanSave())
            return false;

        m_PendingSave = SaveRequest{ id, saveAs };
        return true;
    }

    bool EditorDocumentController::RequestAction(std::vector<EditorDocumentId> documents, GuardedAction action)
    {
        if (IsBusy() || !action)
            return false;

        for (const auto id : documents)
        {
            if (!m_Documents.Find(id))
                return false;
        }

        PendingAction pending;
        pending.Documents = std::move(documents);
        pending.Execute = std::move(action);
        m_PendingAction = std::move(pending);
        return true;
    }

    void EditorDocumentController::CancelAction()
    {
        if (m_PendingAction && m_PendingAction->WaitingSave)
            m_PendingSave.reset();

        m_PendingAction.reset();
    }

    void EditorDocumentController::CancelForDocument(EditorDocumentId id)
    {
        if (m_PendingSave && m_PendingSave->Id == id)
            m_PendingSave.reset();

        if (m_PendingAction)
        {
            const auto& documents = m_PendingAction->Documents;
            if (std::find(documents.begin(), documents.end(), id) != documents.end())
                CancelAction();
        }
    }

    EditorDocumentId EditorDocumentController::GetConfirmationDocumentId() const
    {
        if (!m_PendingAction || !m_PendingAction->Started || m_PendingAction->WaitingSave)
            return InvalidEditorDocumentId;

        const auto& pending = *m_PendingAction;
        if (pending.Cursor >= pending.Documents.size())
            return InvalidEditorDocumentId;

        return pending.Documents[pending.Cursor];
    }

    bool EditorDocumentController::ResolveConfirmation(EDocumentActionChoice choice)
    {
        if (!m_PendingAction)
            return false;

        if (choice == EDocumentActionChoice::Cancel)
        {
            CancelAction();
            return true;
        }

        const auto id = GetConfirmationDocumentId();
        auto* document = m_Documents.Find(id);
        if (!document)
            return false;

        if (choice == EDocumentActionChoice::Save)
        {
            if (!document->CanSave())
                return false;

            m_PendingSave = SaveRequest{ id, false };
            m_PendingAction->WaitingSave = true;
        }
        else
        {
            m_PendingAction->Discarded.push_back(id);
            m_PendingAction->Cursor = m_PendingAction->Documents.size();
        }

        return true;
    }

    void EditorDocumentController::NotifySaveResult(const EditorDocumentSaveResult& result)
    {
        if (!m_PendingAction || !m_PendingAction->WaitingSave)
            return;

        auto& pending = *m_PendingAction;
        if (pending.Documents[pending.Cursor] != result.Id)
            return;

        if (m_PendingSave && m_PendingSave->Id == result.Id)
            m_PendingSave.reset();

        const auto* document = m_Documents.Find(result.Id);
        if (result.Status != EDocumentSaveStatus::Saved || !document || document->HasUnsavedChanges())
        {
            CancelAction();
            return;
        }

        pending.WaitingSave = false;
        pending.Cursor = pending.Documents.size();
    }

    std::optional<Engine::EResult> EditorDocumentController::ProcessPendingAction()
    {
        if (!m_PendingAction || m_PendingAction->WaitingSave)
            return std::nullopt;

        auto& pending = *m_PendingAction;
        pending.Started = true;

        for (std::size_t index = 0; index < pending.Documents.size(); ++index)
        {
            const auto id = pending.Documents[index];
            const auto* document = m_Documents.Find(id);

            if (!document)
            {
                CancelAction();
                return Engine::EResult::Fail;
            }

            const bool discarded =
                std::find(pending.Discarded.begin(), pending.Discarded.end(), id) != pending.Discarded.end();

            if (document->HasUnsavedChanges() && !discarded)
            {
                pending.Cursor = index;
                return std::nullopt;
            }
        }

        auto approved = std::move(*m_PendingAction);
        m_PendingAction.reset();

        try
        {
            return approved.Execute(approved.Discarded);
        }
        catch (const std::exception& exception)
        {
            BAM_LOG(Error, "Editor", "Document", "Document action exception: {}", exception.what());
            return Engine::EResult::Fail;
        }
    }
}