#pragma once
#include "FileDialogs.h"
#include "Editor_Macros.h"
#include <shobjidl.h>
#include <wrl/client.h>

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "uuid.lib")

bool FileDialogs::SaveFileDialog(wstring& outPath, const vector<FileDialogFilter>& filters, wstring_view defaultFileName, wstring_view defaultExt)
{
	wchar szFileName[MAX_PATH] = { 0 };
	if (!defaultFileName.empty())
		wcsncpy_s(szFileName, defaultFileName.data(), defaultFileName.size());

	vector<wchar> filterBuffer = FilterBuffer(filters);

	OPENFILENAMEW ofn = {};
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = nullptr;
	ofn.lpstrFile = szFileName;
	ofn.nMaxFile = MAX_PATH;
	ofn.lpstrFilter = filterBuffer.empty() ? nullptr : filterBuffer.data();
	ofn.nFilterIndex = 1;
	ofn.lpstrDefExt = defaultExt.empty() ? nullptr : defaultExt.data();
	ofn.Flags = OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;

	if (!GetSaveFileNameW(&ofn))
		return false;

	outPath = szFileName;
	return true;
}

bool FileDialogs::OpenFileDialog(wstring& outPath, const vector<FileDialogFilter>& filters,
    wstring_view initialDir, EFileDialogContext context)
{
    struct ComScope
    {
        HRESULT Result = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

        ~ComScope()
        {
            if (SUCCEEDED(Result))
                CoUninitialize();
        }
    } comScope;

    const auto fail = [](const char* stage, HRESULT result)
        {
            EDITOR_LOG_ERROR("File open dialog failed: stage={}, HRESULT={}", stage, static_cast<long>(result));
            return false;
        };

    if (FAILED(comScope.Result))
        return fail("CoInitializeEx", comScope.Result);

    Microsoft::WRL::ComPtr<IFileOpenDialog> dialog;
    HRESULT result = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(dialog.GetAddressOf()));

    if (FAILED(result))
        return fail("CreateDialog", result);

    // 고정 식별자: 용도별 마지막 폴더를 Windows가 구분해 저장한다.
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

    result = dialog->SetOptions(options | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST |
        FOS_FILEMUSTEXIST | FOS_NOCHANGEDIR);
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

    if (!initialDir.empty())
    {
        try
        {
            std::error_code error;
            const auto directory = std::filesystem::absolute(std::filesystem::path(initialDir), error);

            if (!error)
            {
                Microsoft::WRL::ComPtr<IShellItem> folder;
                result = SHCreateItemFromParsingName(directory.c_str(), nullptr,
                    IID_PPV_ARGS(folder.GetAddressOf()));

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

    if (FAILED(result))
        return fail("GetDisplayName", result);

    outPath = pathOwner.get();
    return true;
}

vector<wchar> FileDialogs::FilterBuffer(const vector<FileDialogFilter>& filters)
{
	vector<wchar> buffer;

	if (filters.empty())
	{
		const wstring allFilesFilter = StrToWStr(LOCAL("FileDialog.AllFiles"));
		constexpr wstring_view pattern = L"*.*";
		buffer.insert(buffer.end(), allFilesFilter.begin(), allFilesFilter.end());
		buffer.push_back(L'\0');
		buffer.insert(buffer.end(), pattern.begin(), pattern.end());
		buffer.push_back(L'\0');
	}
	else
	{
		for (const auto& filter : filters)
		{
			buffer.insert(buffer.end(), filter.Name.begin(), filter.Name.end());
			buffer.push_back(L'\0');
			buffer.insert(buffer.end(), filter.Pattern.begin(), filter.Pattern.end());
			buffer.push_back(L'\0');
		}
	}
	buffer.push_back(L'\0');
    return buffer;
}
