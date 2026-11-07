#include "PlatformPath.h"
#include <SDL3/SDL_filesystem.h>
#include <windows.h>
#include <shlobj.h>
#include <algorithm>
#include <cwctype>

namespace Engine
{
    static std::filesystem::path s_ProjectRoot;
    static std::filesystem::path s_EngineResourceRoot;
    static std::filesystem::path s_ProjectAssetRoot;

    namespace
    {
        bool StartsWithIgnoreCase(std::wstring_view str, std::wstring_view prefix)
        {
            if (str.size() < prefix.size()) return false;
            for (size_t i = 0; i < prefix.size(); ++i)
            {
                if (::towlower(str[i]) != ::towlower(prefix[i]))
                    return false;
            }
            return true;
        }

        bool StartsWithIgnoreCase(std::string_view str, std::string_view prefix)
        {
            if (str.size() < prefix.size()) return false;
            for (size_t i = 0; i < prefix.size(); ++i)
            {
                if (::tolower(static_cast<unsigned char>(str[i])) != ::tolower(static_cast<unsigned char>(prefix[i])))
                    return false;
            }
            return true;
        }

        bool IsSubpathOf(const std::filesystem::path& path, const std::filesystem::path& base)
        {
            if (path.empty() || base.empty()) return false;
            auto normPath = PlatformPath::Normalize(path);
            auto normBase = PlatformPath::Normalize(base);
            auto rel = normPath.lexically_relative(normBase);
            if (rel.empty()) return false;
            const auto nativeStr = rel.native();
            if (nativeStr.starts_with(L"..") || rel.is_absolute()) return false;
            return true;
        }
    }

    void PlatformPath::SetProjectRoot(const std::filesystem::path& root)
    {
        s_ProjectRoot = Normalize(root);
    }

    const std::filesystem::path& PlatformPath::GetProjectRoot()
    {
        if (s_ProjectRoot.empty())
            s_ProjectRoot = GetExecutableDirectory();
        return s_ProjectRoot;
    }

    void PlatformPath::SetEngineResourceRoot(const std::filesystem::path& root)
    {
        s_EngineResourceRoot = Normalize(root);
    }

    const std::filesystem::path& PlatformPath::GetEngineResourceRoot()
    {
        if (s_EngineResourceRoot.empty())
        {
            auto candidate = Normalize(GetProjectRoot() / "Engine/Resources");
            std::error_code ec;
            if (std::filesystem::is_directory(candidate, ec))
            {
                s_EngineResourceRoot = candidate;
            }
            else
            {
                s_EngineResourceRoot = Normalize(GetProjectRoot() / "Resources");
            }
        }
        return s_EngineResourceRoot;
    }

    void PlatformPath::SetProjectAssetRoot(const std::filesystem::path& root)
    {
        s_ProjectAssetRoot = Normalize(root);
    }

    const std::filesystem::path& PlatformPath::GetProjectAssetRoot()
    {
        if (s_ProjectAssetRoot.empty())
        {
            auto candidate = Normalize(GetProjectRoot() / "Assets");
            std::error_code ec;
            if (std::filesystem::is_directory(candidate, ec))
            {
                s_ProjectAssetRoot = candidate;
            }
            else
            {
                s_ProjectAssetRoot = Normalize(GetProjectRoot() / "Resources");
            }
        }
        return s_ProjectAssetRoot;
    }

    bool PlatformPath::IsEngineUri(const std::wstring& uri)
    {
        return StartsWithIgnoreCase(uri, L"engine://");
    }

    bool PlatformPath::IsEngineUri(const std::string& uri)
    {
        return StartsWithIgnoreCase(uri, "engine://");
    }

    bool PlatformPath::IsProjectUri(const std::wstring& uri)
    {
        return StartsWithIgnoreCase(uri, L"project://") || StartsWithIgnoreCase(uri, L"asset://");
    }

    bool PlatformPath::IsProjectUri(const std::string& uri)
    {
        return StartsWithIgnoreCase(uri, "project://") || StartsWithIgnoreCase(uri, "asset://");
    }

    bool PlatformPath::IsVirtualUri(const std::wstring& uri)
    {
        return IsEngineUri(uri) || IsProjectUri(uri);
    }

    bool PlatformPath::IsVirtualUri(const std::string& uri)
    {
        return IsEngineUri(uri) || IsProjectUri(uri);
    }

    std::filesystem::path PlatformPath::ResolveUri(const std::wstring& pathOrUri)
    {
        if (pathOrUri.empty()) return {};

        if (IsEngineUri(pathOrUri))
        {
            const std::wstring sub = pathOrUri.substr(9);
            const auto resolved = Normalize(GetEngineResourceRoot() / sub);
            if (!IsSubpathOf(resolved, GetEngineResourceRoot()) && resolved != GetEngineResourceRoot())
            {
                return GetEngineResourceRoot();
            }
            return resolved;
        }

        if (IsProjectUri(pathOrUri))
        {
            const size_t prefixLen = StartsWithIgnoreCase(pathOrUri, L"project://") ? 10 : 8;
            const std::wstring sub = pathOrUri.substr(prefixLen);
            const auto resolved = Normalize(GetProjectAssetRoot() / sub);
            if (!IsSubpathOf(resolved, GetProjectAssetRoot()) && resolved != GetProjectAssetRoot())
            {
                return GetProjectAssetRoot();
            }
            return resolved;
        }

        const std::filesystem::path fsPath(pathOrUri);
        if (fsPath.is_absolute())
        {
            return Normalize(fsPath);
        }

        std::error_code ec;
        const auto inProject = Normalize(GetProjectAssetRoot() / fsPath);
        if (std::filesystem::exists(inProject, ec))
            return inProject;

        const auto inEngine = Normalize(GetEngineResourceRoot() / fsPath);
        if (std::filesystem::exists(inEngine, ec))
            return inEngine;

        auto normRel = Normalize(fsPath);
        const std::wstring relStr = normRel.generic_wstring();
        if (StartsWithIgnoreCase(relStr, L"resources/"))
        {
            const std::wstring stripped = relStr.substr(10);
            const auto strippedProject = Normalize(GetProjectAssetRoot() / stripped);
            if (std::filesystem::exists(strippedProject, ec))
                return strippedProject;

            const auto strippedEngine = Normalize(GetEngineResourceRoot() / stripped);
            if (std::filesystem::exists(strippedEngine, ec))
                return strippedEngine;
        }

        return ResolveProjectRelative(fsPath);
    }

    std::filesystem::path PlatformPath::ResolveUri(const std::filesystem::path& pathOrUri)
    {
        return ResolveUri(pathOrUri.wstring());
    }

    std::filesystem::path PlatformPath::ResolveUri(const std::string& pathOrUri)
    {
        std::wstring wstr(pathOrUri.begin(), pathOrUri.end());
        return ResolveUri(wstr);
    }

    std::wstring PlatformPath::ToVirtualUri(const std::filesystem::path& physicalPath)
    {
        if (physicalPath.empty()) return {};

        const std::wstring rawStr = physicalPath.wstring();
        if (IsVirtualUri(rawStr)) return rawStr;

        const auto norm = Normalize(physicalPath);
        const auto engineRoot = GetEngineResourceRoot();
        const auto projectRoot = GetProjectAssetRoot();

        if (engineRoot != projectRoot && (IsSubpathOf(norm, engineRoot) || norm == engineRoot))
        {
            auto rel = norm.lexically_relative(engineRoot);
            return L"Engine://" + rel.generic_wstring();
        }

        if (projectRoot != engineRoot && (IsSubpathOf(norm, projectRoot) || norm == projectRoot))
        {
            auto rel = norm.lexically_relative(projectRoot);
            return L"Project://" + rel.generic_wstring();
        }

        if (IsSubpathOf(norm, engineRoot) || norm == engineRoot)
        {
            auto rel = norm.lexically_relative(engineRoot);
            const std::wstring relStr = rel.generic_wstring();

            if (StartsWithIgnoreCase(relStr, L"shader/") ||
                StartsWithIgnoreCase(relStr, L"shaders/") ||
                StartsWithIgnoreCase(relStr, L"data/localization/engine.csv") ||
                StartsWithIgnoreCase(relStr, L"font/") ||
                StartsWithIgnoreCase(relStr, L"fonts/"))
            {
                return L"Engine://" + relStr;
            }

            return L"Project://" + relStr;
        }

        return ToProjectRelative(norm).generic_wstring();
    }

    bool PlatformPath::IsReadOnlyTarget(const std::filesystem::path& pathOrUri)
    {
        return IsReadOnlyTarget(pathOrUri.wstring());
    }

    bool PlatformPath::IsReadOnlyTarget(const std::string& pathOrUri)
    {
        std::wstring wstr(pathOrUri.begin(), pathOrUri.end());
        return IsReadOnlyTarget(wstr);
    }

    bool PlatformPath::IsReadOnlyTarget(const std::wstring& pathOrUri)
    {
        if (IsEngineUri(pathOrUri))
            return true;

        const auto resolved = ResolveUri(pathOrUri);
        const auto engineRoot = GetEngineResourceRoot();
        const auto projectRoot = GetProjectAssetRoot();

        if (engineRoot != projectRoot)
        {
            return IsSubpathOf(resolved, engineRoot) || resolved == engineRoot;
        }

        return IsEngineUri(pathOrUri);
    }

    std::filesystem::path PlatformPath::ResolveProjectRelative(const std::filesystem::path& relativePath)
    {
        if (relativePath.is_absolute()) return Normalize(relativePath);
        return Normalize(GetProjectRoot() / relativePath);
    }

    std::filesystem::path PlatformPath::GetExecutableDirectory()
    {
        static std::filesystem::path cachedPath = []() {
            const char* basePath = SDL_GetBasePath();
            if (basePath)
            {
                return Normalize(std::filesystem::u8path(basePath));
            }

            wchar_t buffer[MAX_PATH] = {};
            GetModuleFileNameW(nullptr, buffer, MAX_PATH);
            return Normalize(std::filesystem::path(buffer).parent_path());
            }();
        return cachedPath;
    }

    std::filesystem::path PlatformPath::GetSystemFontDirectory()
    {
        wchar_t path[MAX_PATH] = {};
        if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_FONTS, nullptr, SHGFP_TYPE_CURRENT, path)))
            return Normalize(std::filesystem::path(path));

        const char* windir = std::getenv("WINDIR");
        if (windir)
            return Normalize(std::filesystem::path(windir) / "Fonts");

        return Normalize(std::filesystem::path("C:/Windows/Fonts"));
    }

    std::filesystem::path PlatformPath::Normalize(const std::filesystem::path& path)
    {
        std::wstring str = path.wstring();
        std::replace(str.begin(), str.end(), L'\\', L'/');
        return std::filesystem::path(str).lexically_normal();
    }

    std::filesystem::path PlatformPath::ToProjectRelative(const std::filesystem::path& path)
    {
        if (path.empty()) return path;
        std::error_code ec;
        const std::filesystem::path normalized = path.lexically_normal();
        const std::filesystem::path root = GetProjectRoot().lexically_normal();
        if (root.empty()) return normalized;
        auto rel = normalized.lexically_relative(root);
        if (!rel.empty() && rel.native()[0] != L'.' && !rel.is_absolute())
        {
            return rel.generic_wstring();
        }
        return normalized.generic_wstring();
    }
}
