#pragma once

#include "EditorDocumentInterface.h"

namespace Editor
{
    class EditorDocumentManager final
    {
    public:
        EditorDocumentManager() = default;
        ~EditorDocumentManager() = default;

        EditorDocumentManager(const EditorDocumentManager&) = delete;
        EditorDocumentManager& operator=(const EditorDocumentManager&) = delete;
        EditorDocumentManager(EditorDocumentManager&&) = delete;
        EditorDocumentManager& operator=(EditorDocumentManager&&) = delete;

        EditorDocumentId Add(std::unique_ptr<IEditorDocument> document);
        IEditorDocument* Find(EditorDocumentId id);
        const IEditorDocument* Find(EditorDocumentId id) const;

        bool SetActive(EditorDocumentId id);
        EditorDocumentId GetActiveId() const { return m_ActiveId; }

        std::vector<EditorDocumentId> GetDocumentIds() const;
        std::vector<EditorDocumentId> GetModifiedDocumentIds() const;

        // 저장 확인 또는 변경 폐기가 완료된 후 호출한다.
        bool Remove(EditorDocumentId id);

        // 확인 절차를 수행하지 않는 종료 정리용 함수다.
        void Clear();

    private:
        std::map<EditorDocumentId, std::unique_ptr<IEditorDocument>> m_Documents;
        EditorDocumentId m_NextId = 1;
        EditorDocumentId m_ActiveId = InvalidEditorDocumentId;
    };
}