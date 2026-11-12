#include "FileDialogs.h"
#include "Editor_Macros.h"
#include "Functions.h"
#include "PlatformPath.h"

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "uuid.lib")

namespace
{
    static std::filesystem::path GetDefaultFolderForContext(Editor::EFileDialogContext context)
    {
        const auto& assetRoot = Engine::PlatformPath::GetProjectAssetRoot();
        if (assetRoot.empty())
            return {};

        std::filesystem::path target;
        switch (context)
        {
        case Editor::EFileDialogContext::SceneOpen:
        case Editor::EFileDialogContext::SceneSave:
            target = assetRoot / "Scenes";
            break;
        case Editor::EFileDialogContext::ParticleOpen:
        case Editor::EFileDialogContext::ParticleSave:
            target = assetRoot / "Effect";
            break;
        case Editor::EFileDialogContext::SpriteOpen:
        case Editor::EFileDialogContext::SpriteSave:
            target = assetRoot / "Texture";
            break;
        case Editor::EFileDialogContext::TimelineOpen:
        case Editor::EFileDialogContext::TimelineSave:
            target = assetRoot / "Data/Animation/Timeline";
            break;
        case Editor::EFileDialogContext::GraphOpen:
        case Editor::EFileDialogContext::GraphSave:
            target = assetRoot / "Scripts";
            break;
        case Editor::EFileDialogContext::PrefabOpen:
        case Editor::EFileDialogContext::PrefabSave:
            target = assetRoot / "Prefab";
            break;
        case Editor::EFileDialogContext::ResourceImport:
        case Editor::EFileDialogContext::ModelImport:
            target = assetRoot / "Source";
            break;
        case Editor::EFileDialogContext::MaterialOpen:
        case Editor::EFileDialogContext::MaterialSave:
            target = assetRoot / "Material";
            break;
        default:
            target = assetRoot;
            break;
        }

        std::error_code ec;
        if (std::filesystem::is_directory(target, ec))
            return target;

        return assetRoot;
    }

    static GUID GetDialogGuid(Editor::EFileDialogContext context, std::string_view historyKey)
    {
        if (historyKey.empty())
        {
            return { 0xb92137d1, 0xa4e9, 0x41d2,
                { 0x98, 0x2e, 0x4b, 0x38, 0xd1, 0x65, 0x00, static_cast<unsigned char>(context) } };
        }

        // Keep the namespace and byte order stable for persisted dialog history.
        const uint64 hash = Engine::RunTimeHash(historyKey);
        GUID guid{ 0x5a171d2c, 0x4372, 0x8cf1, {} };
        for (std::size_t index = 0; index < sizeof(guid.Data4); ++index)
            guid.Data4[index] = static_cast<unsigned char>(hash >> (index * 8));

        guid.Data4[0] = static_cast<unsigned char>((guid.Data4[0] & 0x3f) | 0x80);
        return guid;
    }

    static bool ShowDialog(wstring& outPath, bool save, bool pickFolders,
        const vector<FileDialogFilter>& filters, wstring_view defaultFileName,
        wstring_view defaultExt, wstring_view initialDir, Editor::EFileDialogContext context,
        std::string_view historyKey = {})
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

        result = dialog->SetClientGuid(GetDialogGuid(context, historyKey));
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

        if (!pickFolders)
        {
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
        }

        const wstring fileName(defaultFileName);
        if (!fileName.empty())
        {
            result = dialog->SetFileName(fileName.c_str());
            if (FAILED(result))
                return fail("SetFileName", result);
        }

        const wstring extension(defaultExt);
        if (!pickFolders && !extension.empty())
        {
            result = dialog->SetDefaultExtension(extension.c_str());
            if (FAILED(result))
                return fail("SetDefaultExtension", result);
        }

        std::filesystem::path resolvedDir;
        if (!initialDir.empty())
        {
            std::filesystem::path candidate(initialDir);
            if (candidate.is_relative())
            {
                candidate = Engine::PlatformPath::ResolveProjectRelative(candidate);
            }
            std::error_code ec;
            if (std::filesystem::is_directory(candidate, ec))
                resolvedDir = candidate;
        }

        if (resolvedDir.empty())
        {
            resolvedDir = GetDefaultFolderForContext(context);
        }

        if (!resolvedDir.empty())
        {
            try
            {
                std::error_code error;
                const auto directory = std::filesystem::absolute(resolvedDir, error);

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

namespace Editor
{
    bool FileDialogs::SaveFileDialog(wstring& outPath, const vector<FileDialogFilter>& filters,
        wstring_view defaultFileName, wstring_view defaultExt, EFileDialogContext context)
    {
        return ShowDialog(outPath, true, false, filters, defaultFileName, defaultExt, L"", context);
    }

    bool FileDialogs::SaveFileDialog(wstring& outPath, const vector<FileDialogFilter>& filters,
        wstring_view defaultFileName, wstring_view defaultExt,
        wstring_view initialDir, EFileDialogContext context)
    {
        return ShowDialog(outPath, true, false, filters, defaultFileName, defaultExt, initialDir, context);
    }

    bool FileDialogs::OpenFileDialog(wstring& outPath, const vector<FileDialogFilter>& filters,
        wstring_view initialDir, EFileDialogContext context)
    {
        return ShowDialog(outPath, false, false, filters, L"", L"", initialDir, context);
    }

    bool FileDialogs::OpenFileDialog(wstring& outPath, const vector<FileDialogFilter>& filters,
        wstring_view initialDir, std::string_view historyKey)
    {
        return ShowDialog(outPath, false, false, filters, L"", L"", initialDir, EFileDialogContext::General, historyKey);
    }

    bool FileDialogs::OpenFolderDialog(wstring& outPath, wstring_view initialDir, EFileDialogContext context)
    {
        return ShowDialog(outPath, false, true, {}, L"", L"", initialDir, context);
    }

    bool FileDialogs::OpenFolderDialog(wstring& outPath, wstring_view initialDir, std::string_view historyKey)
    {
        return ShowDialog(outPath, false, true, {}, L"", L"", initialDir, EFileDialogContext::General, historyKey);
    }
}
