#pragma once

#include "Editor_Includes.h"
#include "FileDialogContext.h"
#include <string_view>

BEGIN(Editor)
struct FileDialogFilter
{
    std::wstring Name;
    std::wstring Pattern;
};

class FileDialogs
{
public:
    static bool SaveFileDialog(wstring& outPath, const vector<FileDialogFilter>& filters = {},
        wstring_view defaultFileName = L"", wstring_view defaultExt = L"",
        EFileDialogContext context = EFileDialogContext::General);

    static bool SaveFileDialog(wstring& outPath, const vector<FileDialogFilter>& filters,
        wstring_view defaultFileName, wstring_view defaultExt,
        wstring_view initialDir, EFileDialogContext context = EFileDialogContext::General);

    static bool OpenFileDialog(wstring& outPath, const vector<FileDialogFilter>& filters = {},
        wstring_view initialDir = L"", EFileDialogContext context = EFileDialogContext::General);

    static bool OpenFileDialog(wstring& outPath, const vector<FileDialogFilter>& filters,
        wstring_view initialDir, std::string_view historyKey);

    static bool OpenFolderDialog(wstring& outPath, wstring_view initialDir = L"",
        EFileDialogContext context = EFileDialogContext::General);

    static bool OpenFolderDialog(wstring& outPath, wstring_view initialDir, std::string_view historyKey);
};
END
