#include "PluginApproval.h"

#include <archive/ArchiveInterface.h>
#include <glaze/glaze.hpp>

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <memory>
#include <system_error>
#include <vector>

#if defined(_WIN32)
#include "Engine_Includes.h"
#endif

namespace Editor
{
    struct PluginApprovalDocument
    {
        std::uint32_t schemaVersion = 0;
        std::vector<PluginApproval> approvals;
    };
    namespace
    {
        constexpr std::uint32_t PluginApprovalSchemaVersion = 1;
        bool ReadPluginApprovalDocument(const std::filesystem::path& path, PluginApprovalDocument& document)
        {
            if (path.empty())
                return false;

            std::error_code error;
            if (!std::filesystem::exists(path, error))
            {
                document.schemaVersion = PluginApprovalSchemaVersion;
                return !error;
            }

            std::ifstream file(path, std::ios::binary);
            if (!file)
                return false;

            const std::string json{std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{}};
            glz::context context{};
            return !glz::read<glz::opts_validate{}>(document, json, context) &&
                document.schemaVersion == PluginApprovalSchemaVersion;
        }
    }

    bool HasPluginApproval(const std::filesystem::path& path, const PluginApproval& approval)
    {
        PluginApprovalDocument document;
        return ReadPluginApprovalDocument(path, document) &&
            std::find(document.approvals.begin(), document.approvals.end(), approval) != document.approvals.end();
    }

    bool SavePluginApproval(const std::filesystem::path& path, const PluginApproval& approval)
    {
        if (path.empty())
            return false;

#if defined(_WIN32)
        // 다른 Editor 프로세스의 읽기-수정-저장과 겹치면 기존 기록을 보호한다.
        auto lockPath = path;
        lockPath += L".lock";
        const HANDLE handle = CreateFileW(lockPath.c_str(), GENERIC_WRITE, 0, nullptr, OPEN_ALWAYS,
            FILE_ATTRIBUTE_NORMAL, nullptr);
        if (handle == INVALID_HANDLE_VALUE)
            return false;
        const std::unique_ptr<void, decltype(&CloseHandle)> lock(handle, &CloseHandle);
#endif

        PluginApprovalDocument document;
        if (!ReadPluginApprovalDocument(path, document))
            return false;
        if (std::find(document.approvals.begin(), document.approvals.end(), approval) != document.approvals.end())
            return true;

        document.approvals.push_back(approval);
        std::string json;
        if (glz::write_json(document, json))
            return false;

        return static_cast<bool>(archive::IArchive::SaveEncodedToFile(path, json));
    }
}
