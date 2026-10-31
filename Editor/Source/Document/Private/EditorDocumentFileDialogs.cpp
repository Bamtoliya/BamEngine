#include "EditorDocumentController.h"
#include "FileDialogs.h"

std::optional<EditorDocumentSaveResult> EditorDocumentController::ProcessPendingSave()
{
    if (!m_PendingSave)
        return std::nullopt;

    const auto request = *m_PendingSave;
    m_PendingSave.reset();

    try
    {
        auto* document = m_Documents.Find(request.Id);
        if (!document || !document->CanSave())
            return EditorDocumentSaveResult{ request.Id, EDocumentSaveStatus::Unavailable };

        auto path = document->GetPath();

        if (request.SaveAs || path.empty())
        {
            const auto info = document->GetSaveInfo();
            const auto defaultName = path.empty() ? info.DefaultFileName : path.filename().wstring();
            std::wstring selected;

            if (!FileDialogs::SaveFileDialog(selected,
                { { StrToWStr(LOCAL(info.FilterKey)), info.Pattern } },
                defaultName, info.DefaultExtension, info.Context))
            {
                return EditorDocumentSaveResult{ request.Id, EDocumentSaveStatus::NotSelected };
            }

            path = selected;
        }

        document = m_Documents.Find(request.Id);
        if (!document || !document->CanSave())
            return EditorDocumentSaveResult{ request.Id, EDocumentSaveStatus::Unavailable };

        const auto result = document->SaveTo(path);
        const auto status = result == Engine::EResult::Success
            ? EDocumentSaveStatus::Saved : EDocumentSaveStatus::Failed;

        return EditorDocumentSaveResult{ request.Id, status };
    }
    catch (const std::exception& exception)
    {
        BAM_LOG(Error, "Editor", "Document", "Document save exception: {}", exception.what());
        return EditorDocumentSaveResult{ request.Id, EDocumentSaveStatus::Failed };
    }
}