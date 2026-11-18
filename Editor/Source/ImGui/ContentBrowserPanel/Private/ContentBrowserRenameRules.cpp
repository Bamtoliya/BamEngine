#include "ContentBrowserRename.h"
#include "Functions.h"
#include <Windows.h>

namespace Editor::ContentBrowser
{
    namespace
    {
        std::wstring RenameExtension(const std::filesystem::path& path, bool directory)
        {
            if (directory)
                return {};
            auto extension = path.extension().wstring();
            if (_wcsicmp(extension.c_str(), L".json") == 0 || _wcsicmp(extension.c_str(), L".beve") == 0)
            {
                const auto inner = path.stem().extension().wstring();
                if ((inner.size() >= 4 && _wcsnicmp(inner.c_str(), L".bam", 4) == 0) ||
                    _wcsicmp(inner.c_str(), L".actiontimeline") == 0)
                    extension = inner + extension;
            }
            return extension;
        }
    }

    std::string GetRenameBaseName(const std::filesystem::path& path, bool directory)
    {
        const auto filename = path.filename().wstring();
        const auto extension = RenameExtension(path, directory);
        return Engine::WStrToStr(filename.substr(0, filename.size() - extension.size()));
    }

    bool TryBuildRenamePath(const std::filesystem::path& path, bool directory, std::string_view name,
        std::filesystem::path& target)
    {
        target.clear();
        if (name.empty())
            return false;
        for (const unsigned char ch : name)
        {
            if (ch < 32 || std::string_view("<>:\"/\\|?*").find(static_cast<char>(ch)) != std::string_view::npos)
                return false;
        }
        auto base = Engine::Utf8ToPath(std::string(name)).wstring();
        const auto extension = RenameExtension(path, directory);
        if (!extension.empty() && base.size() >= extension.size() &&
            _wcsicmp(base.c_str() + base.size() - extension.size(), extension.c_str()) == 0)
            base.resize(base.size() - extension.size());
        if (base.empty() || base == L"." || base == L".." || base.back() == L'.' || base.back() == L' ' ||
            base.size() + extension.size() > 255)
            return false;
        auto device = base.substr(0, base.find(L'.'));
        while (!device.empty() && device.back() == L' ')
            device.pop_back();
        for (const wchar_t* reserved : { L"CON", L"PRN", L"AUX", L"NUL", L"CLOCK$", L"CONIN$", L"CONOUT$" })
        {
            if (_wcsicmp(device.c_str(), reserved) == 0)
                return false;
        }
        if (device.size() == 4 && (_wcsnicmp(device.c_str(), L"COM", 3) == 0 ||
            _wcsnicmp(device.c_str(), L"LPT", 3) == 0) &&
            ((device[3] >= L'1' && device[3] <= L'9') || device[3] == L'¹' || device[3] == L'²' || device[3] == L'³'))
            return false;
        target = path.parent_path() / (base + extension);
        return true;
    }
}
