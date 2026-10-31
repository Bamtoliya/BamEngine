#include "EditorDocumentManager.h"

namespace Editor
{
    EditorDocumentId EditorDocumentManager::Add(std::unique_ptr<IEditorDocument> document)
    {
        if (!document || m_NextId == InvalidEditorDocumentId)
            return InvalidEditorDocumentId;

        const auto id = m_NextId;
        m_Documents.emplace(id, std::move(document));
        ++m_NextId;

        if (m_ActiveId == InvalidEditorDocumentId)
            m_ActiveId = id;

        return id;
    }

    IEditorDocument* EditorDocumentManager::Find(EditorDocumentId id)
    {
        const auto found = m_Documents.find(id);
        return found == m_Documents.end() ? nullptr : found->second.get();
    }

    const IEditorDocument* EditorDocumentManager::Find(EditorDocumentId id) const
    {
        const auto found = m_Documents.find(id);
        return found == m_Documents.end() ? nullptr : found->second.get();
    }

    bool EditorDocumentManager::SetActive(EditorDocumentId id)
    {
        if (id != InvalidEditorDocumentId && !Find(id))
            return false;

        m_ActiveId = id;
        return true;
    }

    std::vector<EditorDocumentId> EditorDocumentManager::GetDocumentIds() const
    {
        std::vector<EditorDocumentId> ids;
        ids.reserve(m_Documents.size());

        for (const auto& [id, document] : m_Documents)
            ids.push_back(id);

        return ids;
    }

    std::vector<EditorDocumentId> EditorDocumentManager::GetModifiedDocumentIds() const
    {
        std::vector<EditorDocumentId> ids;

        for (const auto& [id, document] : m_Documents)
        {
            if (document->HasUnsavedChanges())
                ids.push_back(id);
        }

        return ids;
    }

    bool EditorDocumentManager::Remove(EditorDocumentId id)
    {
        const auto found = m_Documents.find(id);
        if (found == m_Documents.end())
            return false;

        if (m_ActiveId == id)
            m_ActiveId = InvalidEditorDocumentId;

        m_Documents.erase(found);
        return true;
    }

    void EditorDocumentManager::Clear()
    {
        m_ActiveId = InvalidEditorDocumentId;
        m_Documents.clear();
    }
}