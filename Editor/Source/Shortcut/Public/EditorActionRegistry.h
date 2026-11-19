#pragma once

#include "Editor_Includes.h"
#include "ShortcutTypes.h"

BEGIN(Editor)

class ResourceEditorInterface;

class EditorActionRegistry
{
public:
    static void RegisterAll();
    static void RegisterDocumentHistory(std::string_view prefix, const char* categoryKey, EShortcutContext context,
        std::function<ResourceEditorInterface*()> findPanel, std::wstring_view panelScope = {});
};

END
