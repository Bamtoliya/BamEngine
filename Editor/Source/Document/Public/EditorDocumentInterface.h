#pragma once

#include "Engine_Includes.h"
#include "Result.h"

namespace Editor
{
    using EditorDocumentId = std::uint64_t;
    inline constexpr EditorDocumentId InvalidEditorDocumentId = 0;
    struct EditorDocumentSaveInfo
    {
        std::string FilterKey;
        std::wstring Pattern;
        std::wstring DefaultFileName;
        std::wstring DefaultExtension;
    };

    class IEditorDocument
    {
    public:
        virtual ~IEditorDocument() = default;

        virtual const std::filesystem::path& GetPath() const = 0;
        virtual bool HasUnsavedChanges() const = 0;
        virtual bool CanSave() const = 0;

        virtual Engine::EResult SaveTo(const std::filesystem::path& path) = 0;
        virtual Engine::EResult DiscardChanges() = 0;

        virtual EditorDocumentSaveInfo GetSaveInfo() const = 0;
    };
}