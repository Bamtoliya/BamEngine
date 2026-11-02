#include "FileDialogs.h"
#include "Editor_Macros.h"

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "uuid.lib")

namespace
{
    static bool ShowDialog(wstring& outPath, bool save, bool pickFolders,
        const vector<FileDialogFilter>& filters, wstring_view defaultFileName,
        wstring_view defaultExt, wstring_view initialDir, EFileDialogContext context)
    {
        struct FileDialogComScope
        {
            HRESULT Result = CoInitializeEx(
                nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

            ~FileDialogComScope()
            {
                if (SUCCEEDED(Result))
                    CoUninitialize();
            }
        } comScope;

        const auto fail = [save](const char* stage, HRESULT result)
            {
                EDITOR_LOG_ERROR("File dialog failed: save={}, stage={}, HRESULT={}",
                    save, stage, static_cast<long>(result));
                return false;
            };

        if (FAILED(comScope.Result))
            return fail("CoInitializeEx", comScope.Result);

        Microsoft::WRL::ComPtr<IFileDialog> dialog;
        HRESULT result = CoCreateInstance(
            save ? CLSID_FileSaveDialog : CLSID_FileOpenDialog,
            nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(dialog.GetAddressOf()));

        if (FAILED(result))
            return fail("CreateDialog", result);

        const GUID contextGuid = {
            0xb92137d1, 0xa4e9, 0x41d2,
            { 0x98, 0x2e, 0x4b, 0x38, 0xd1, 0x65, 0x00, static_cast<unsigned char>(context) }
        };

        result = dialog->SetClientGuid(contextGuid);
        if (FAILED(result))
            return fail("SetClientGuid", result);

        FILEOPENDIALOGOPTIONS options = {};
        result = dialog->GetOptions(&options);
        if (FAILED(result))
            return fail("GetOptions", result);

        options |= FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST | FOS_NOCHANGEDIR;
        options |= save ? FOS_OVERWRITEPROMPT : FOS_FILEMUSTEXIST;
        if (pickFolders)
            options |= FOS_PICKFOLDERS;

        result = dialog->SetOptions(options);
        if (FAILED(result))
            return fail("SetOptions", result);

        const wstring allFilesName = StrToWStr(LOCAL("FileDialog.AllFiles"));
        vector<COMDLG_FILTERSPEC> specifications;

        if (filters.empty())
        {
            specifications.push_back({ allFilesName.c_str(), L"*.*" });
        }
        else
        {
            for (const auto& filter : filters)
                specifications.push_back({ filter.Name.c_str(), filter.Pattern.c_str() });
        }

        result = dialog->SetFileTypes(static_cast<UINT>(specifications.size()), specifications.data());
        if (FAILED(result))
            return fail("SetFileTypes", result);

        const wstring fileName(defaultFileName);
        if (!fileName.empty())
        {
            result = dialog->SetFileName(fileName.c_str());
            if (FAILED(result))
                return fail("SetFileName", result);
        }

        const wstring extension(defaultExt);
        if (!extension.empty())
        {
            result = dialog->SetDefaultExtension(extension.c_str());
            if (FAILED(result))
                return fail("SetDefaultExtension", result);
        }

        if (!initialDir.empty())
        {
            try
            {
                std::error_code error;
                const auto directory = std::filesystem::absolute(std::filesystem::path(initialDir), error);

                if (!error)
                {
                    Microsoft::WRL::ComPtr<IShellItem> folder;
                    result = SHCreateItemFromParsingName(
                        directory.c_str(), nullptr, IID_PPV_ARGS(folder.GetAddressOf()));

                    if (SUCCEEDED(result))
                    {
                        result = dialog->SetDefaultFolder(folder.Get());
                        if (FAILED(result))
                            EDITOR_LOG_WARN("File dialog default folder was not applied.");
                    }
                }
            }
            catch (const std::filesystem::filesystem_error& exception)
            {
                EDITOR_LOG_WARN("File dialog default folder is unavailable: {}", exception.what());
            }
        }

        result = dialog->Show(nullptr);
        if (result == HRESULT_FROM_WIN32(ERROR_CANCELLED))
            return false;

        if (FAILED(result))
            return fail("Show", result);

        Microsoft::WRL::ComPtr<IShellItem> selected;
        result = dialog->GetResult(selected.GetAddressOf());
        if (FAILED(result))
            return fail("GetResult", result);

        PWSTR selectedPath = nullptr;
        result = selected->GetDisplayName(SIGDN_FILESYSPATH, &selectedPath);
        std::unique_ptr<wchar_t, decltype(&CoTaskMemFree)> pathOwner(selectedPath, &CoTaskMemFree);

        if (FAILED(result) || !pathOwner)
            return fail("GetDisplayName", FAILED(result) ? result : E_FAIL);

        outPath = pathOwner.get();
        return true;
    }
}

bool FileDialogs::SaveFileDialog(wstring& outPath, const vector<FileDialogFilter>& filters,
    wstring_view defaultFileName, wstring_view defaultExt, EFileDialogContext context)
{
    return ShowDialog(outPath, true, false, filters, defaultFileName, defaultExt, L"", context);
}
bool FileDialogs::OpenFileDialog(wstring& outPath, const vector<FileDialogFilter>& filters,
    wstring_view initialDir, EFileDialogContext context)
{
    return ShowDialog(outPath, false, false, filters, L"", L"", initialDir, context);
}
bool FileDialogs::OpenFolderDialog(wstring& outPath, wstring_view initialDir, EFileDialogContext context)
{
    return ShowDialog(outPath, false, true, {}, L"", L"", initialDir, context);
}