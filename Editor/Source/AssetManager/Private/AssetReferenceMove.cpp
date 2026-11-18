#include "AssetReferenceValidator.h"
#include "PlatformPath.h"
#include "Functions.h"
#include "Logger.h"
#include <Windows.h>
#include <algorithm>
#include <memory>

namespace Editor
{
    namespace
    {
        namespace fs = std::filesystem;
        using FileHandle = std::unique_ptr<void, decltype(&CloseHandle)>;

        void CheckReferencePath(const fs::path& path, const fs::path& root)
        {
            auto part = path.begin();
            auto parent = root.begin();
            while (part != path.end() && parent != root.end() &&
                CompareStringOrdinal(part->c_str(), -1, parent->c_str(), -1, TRUE) == CSTR_EQUAL)
            {
                ++part;
                ++parent;
            }
            if (parent != root.end() || path != path.lexically_normal())
                throw fs::filesystem_error("Reference outside allowed root", path,
                    std::make_error_code(std::errc::permission_denied));
            for (auto current = path;; current = current.parent_path())
            {
                const DWORD attributes = GetFileAttributesW(current.c_str());
                if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0)
                    throw fs::filesystem_error("Unreadable or redirected reference path", current,
                        std::make_error_code(std::errc::permission_denied));
                if (fs::equivalent(current, root))
                    break;
            }
        }

        FileHandle OpenReference(const fs::path& path, bool writable)
        {
            CheckReferencePath(path, Engine::PlatformPath::GetProjectAssetRoot());
            if (Engine::PlatformPath::IsReadOnlyTarget(path))
                throw fs::filesystem_error("Read-only reference document", path,
                    std::make_error_code(std::errc::permission_denied));
            FileHandle file(CreateFileW(path.c_str(), GENERIC_READ | (writable ? GENERIC_WRITE : 0), FILE_SHARE_READ,
                nullptr, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, nullptr), &CloseHandle);
            if (file.get() == INVALID_HANDLE_VALUE)
            {
                const DWORD error = GetLastError();
                file.release();
                throw fs::filesystem_error("Lock reference document", path,
                    { static_cast<int>(error), std::system_category() });
            }
            BY_HANDLE_FILE_INFORMATION info{};
            if (!GetFileInformationByHandle(file.get(), &info) || info.nNumberOfLinks != 1 ||
                (info.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) != 0)
                throw fs::filesystem_error("Unsupported reference document", path,
                    std::make_error_code(std::errc::operation_not_supported));
            return file;
        }

        bool ReadReferenceBytes(HANDLE file, std::string& text)
        {
            LARGE_INTEGER size{}, start{};
            if (!GetFileSizeEx(file, &size) || size.QuadPart < 0 ||
                static_cast<unsigned long long>(size.QuadPart) > text.max_size() ||
                !SetFilePointerEx(file, start, nullptr, FILE_BEGIN))
                return false;
            text.resize(static_cast<size_t>(size.QuadPart));
            size_t offset = 0;
            while (offset < text.size())
            {
                const auto count = static_cast<DWORD>((std::min)(text.size() - offset, size_t{ MAXDWORD }));
                DWORD read = 0;
                if (!ReadFile(file, text.data() + offset, count, &read, nullptr) || read == 0)
                    return false;
                offset += read;
            }
            return true;
        }

        bool WriteReferenceBytes(HANDLE file, const std::string& text)
        {
            LARGE_INTEGER start{};
            if (!SetFilePointerEx(file, start, nullptr, FILE_BEGIN))
                return false;
            size_t offset = 0;
            while (offset < text.size())
            {
                const auto count = static_cast<DWORD>((std::min)(text.size() - offset, size_t{ MAXDWORD }));
                DWORD written = 0;
                if (!WriteFile(file, text.data() + offset, count, &written, nullptr) || written == 0)
                    return false;
                offset += written;
            }
            return SetEndOfFile(file) && FlushFileBuffers(file);
        }

        bool SaveReferenceBackup(const fs::path& path, const std::string& text)
        {
            FileHandle file(CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, CREATE_NEW,
                FILE_ATTRIBUTE_NORMAL, nullptr), &CloseHandle);
            if (file.get() != INVALID_HANDLE_VALUE)
                return WriteReferenceBytes(file.get(), text);
            const DWORD error = GetLastError();
            file.release();
            if (error != ERROR_FILE_EXISTS && error != ERROR_ALREADY_EXISTS)
                return false;
            CheckReferencePath(path, Engine::PlatformPath::GetProjectRoot());
            file.reset(CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
            if (file.get() == INVALID_HANDLE_VALUE)
            {
                file.release();
                return false;
            }
            std::string existing;
            return ReadReferenceBytes(file.get(), existing) && existing == text;
        }
    }

    bool AssetReferenceValidator::ApplyReferenceRepairs(const std::vector<AssetReferenceEdit>& edits)
    {
        if (edits.empty())
            return true;

        try
        {
            using Path = Engine::PlatformPath;
            const auto project = Path::Normalize(Path::GetProjectRoot());
            const auto backupRoot = project / ".bam" / "asset-reference-repair";
            const auto relative = backupRoot.lexically_relative(Path::Normalize(Path::GetProjectAssetRoot()));
            if (project.empty() || (!relative.empty() && !relative.has_root_path() && *relative.begin() != ".."))
                return false;

            CheckReferencePath(project, project);
            fs::create_directory(backupRoot.parent_path());
            CheckReferencePath(backupRoot.parent_path(), project);
            fs::create_directory(backupRoot);
            CheckReferencePath(backupRoot, project);
            const auto prefix = std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(GetTickCount64());
            fs::path batch;
            for (size_t index = 0;; ++index)
            {
                batch = backupRoot / (prefix + L"-" + std::to_wstring(index));
                if (fs::create_directory(batch))
                    break;
            }

            const auto result = ApplyMoveReferences(edits, true, batch);
            if (result == EAssetReferenceWriteResult::RollbackFailed)
                BAM_LOG(Error, "Editor", "Asset", "Reference repair rollback failed. Backups: {}",
                    Engine::WStrToStr(batch.wstring()));
            return result == EAssetReferenceWriteResult::Success;
        }
        catch (const std::exception& error)
        {
            BAM_LOG(Warn, "Editor", "Asset", "Could not create reference repair backups: {}", error.what());
            return false;
        }
    }

    bool AssetReferenceValidator::ValidateMoveReferences(const std::vector<AssetReferenceEdit>& edits, bool forward)
    {
        try
        {
            for (const auto& edit : edits)
            {
                const auto file = OpenReference(forward ? edit.BeforePath : edit.AfterPath, false);
                std::string actual;
                if (!ReadReferenceBytes(file.get(), actual) || actual != (forward ? edit.BeforeText : edit.AfterText))
                    return false;
            }
            return true;
        }
        catch (const std::exception& error)
        {
            BAM_LOG(Warn, "Editor", "Asset", "Could not validate moved references: {}", error.what());
            return false;
        }
    }

    EAssetReferenceWriteResult AssetReferenceValidator::ApplyMoveReferences(const std::vector<AssetReferenceEdit>& edits,
        bool forward, const std::filesystem::path& recoveryDirectory)
    {
        if (edits.empty())
            return EAssetReferenceWriteResult::Success;
        std::vector<FileHandle> files;
        try
        {
            files.reserve(edits.size());
            CheckReferencePath(recoveryDirectory, Engine::PlatformPath::GetProjectRoot());
            std::string manifest;
            for (size_t index = 0; index < edits.size(); ++index)
            {
                const auto& edit = edits[index];
                files.push_back(OpenReference(forward ? edit.AfterPath : edit.BeforePath, true));
                std::string actual;
                if (!ReadReferenceBytes(files.back().get(), actual) || actual != (forward ? edit.BeforeText : edit.AfterText))
                    return EAssetReferenceWriteResult::Failed;
                const auto prefix = recoveryDirectory / std::to_wstring(index);
                if (!SaveReferenceBackup(fs::path(prefix).concat(L"-before.json"), edit.BeforeText) ||
                    !SaveReferenceBackup(fs::path(prefix).concat(L"-after.json"), edit.AfterText))
                    return EAssetReferenceWriteResult::Failed;
                manifest += std::to_string(index) + "\n" + Engine::WStrToStr(edit.BeforePath.wstring()) + "\n" +
                    Engine::WStrToStr(edit.AfterPath.wstring()) + "\n";
            }
            if (!SaveReferenceBackup(recoveryDirectory / "paths.txt", manifest))
                return EAssetReferenceWriteResult::Failed;
        }
        catch (const std::exception& error)
        {
            BAM_LOG(Warn, "Editor", "Asset", "Could not lock or back up reference documents: {}", error.what());
            return EAssetReferenceWriteResult::Failed;
        }

        // Keep file IDs and all write locks until the entire batch or its rollback has finished.
        for (size_t index = 0; index < files.size(); ++index)
        {
            if (WriteReferenceBytes(files[index].get(), forward ? edits[index].AfterText : edits[index].BeforeText))
                continue;
            bool restored = true;
            for (size_t count = index + 1; count > 0; --count)
            {
                const auto slot = count - 1;
                if (!WriteReferenceBytes(files[slot].get(), forward ? edits[slot].BeforeText : edits[slot].AfterText))
                    restored = false;
            }
            return restored ? EAssetReferenceWriteResult::Failed : EAssetReferenceWriteResult::RollbackFailed;
        }
        return EAssetReferenceWriteResult::Success;
    }
}
