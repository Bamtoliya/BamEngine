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
    static std::filesystem::path s_EditorResourceRoot;
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

        bool IsBuiltInRelativeResourcePath(std::wstring_view relStr)
        {
            return (StartsWithIgnoreCase(relStr, L"shader") && (relStr.size() == 6 || relStr[6] == L'/')) ||
                (StartsWithIgnoreCase(relStr, L"shaders") && (relStr.size() == 7 || relStr[7] == L'/')) ||
                (StartsWithIgnoreCase(relStr, L"data/localization") && (relStr.size() == 17 || relStr[17] == L'/')) ||
                (StartsWithIgnoreCase(relStr, L"font") && (relStr.size() == 4 || relStr[4] == L'/')) ||
                (StartsWithIgnoreCase(relStr, L"fonts") && (relStr.size() == 5 || relStr[5] == L'/')) ||
                StartsWithIgnoreCase(relStr, L"material/defaultmaterial.") ||
                StartsWithIgnoreCase(relStr, L"material/skinningmaterial.") ||
                StartsWithIgnoreCase(relStr, L"material/skymaterial.") ||
                StartsWithIgnoreCase(relStr, L"material/postprocessdefault") ||
                StartsWithIgnoreCase(relStr, L"material/missingmaterial.") ||
                StartsWithIgnoreCase(relStr, L"material/spritematerial.") ||
                StartsWithIgnoreCase(relStr, L"material/particlematerial_") ||
                StartsWithIgnoreCase(relStr, L"material/uimaterial.") ||
                StartsWithIgnoreCase(relStr, L"material/transparentmaterial.") ||
                StartsWithIgnoreCase(relStr, L"texture/white1x1.") ||
                StartsWithIgnoreCase(relStr, L"texture/black1x1.") ||
                StartsWithIgnoreCase(relStr, L"texture/blue1x1.") ||
                StartsWithIgnoreCase(relStr, L"texture/magenta1x1.") ||
                StartsWithIgnoreCase(relStr, L"texture/green1x1.") ||
                StartsWithIgnoreCase(relStr, L"texture/defaultchecker.") ||
                StartsWithIgnoreCase(relStr, L"texture/defaultnormal.") ||
                StartsWithIgnoreCase(relStr, L"texture/defaultpbr.") ||
                StartsWithIgnoreCase(relStr, L"texture/missingtexture.") ||
                StartsWithIgnoreCase(relStr, L"texture/particle_");
        }
    }

    void PlatformPath::SetProjectRoot(const std::filesystem::path& root)
    {
        s_ProjectRoot = Normalize(root);
    }

    const std::filesystem::path& PlatformPath::GetProjectRoot()
    {
        if (s_ProjectRoot.empty())
        {
            auto checkRoot = [](const std::filesystem::path& dir) -> bool {
                std::error_code ec;
                return std::filesystem::exists(dir / "CMakeLists.txt", ec) &&
                    (std::filesystem::is_directory(dir / "Engine", ec) ||
                     std::filesystem::is_directory(dir / "Resources", ec) ||
                     std::filesystem::is_directory(dir / "Assets", ec));
            };

            std::error_code ec;
            auto cur = std::filesystem::current_path(ec);
            while (!cur.empty())
            {
                if (checkRoot(cur))
                {
                    s_ProjectRoot = Normalize(cur);
                    return s_ProjectRoot;
                }
                const auto parent = cur.parent_path();
                if (parent == cur) break;
                cur = parent;
            }

            cur = GetExecutableDirectory();
            while (!cur.empty())
            {
                if (checkRoot(cur))
                {
                    s_ProjectRoot = Normalize(cur);
                    return s_ProjectRoot;
                }
                const auto parent = cur.parent_path();
                if (parent == cur) break;
                cur = parent;
            }

            s_ProjectRoot = GetExecutableDirectory();
        }
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

    void PlatformPath::SetEditorResourceRoot(const std::filesystem::path& root)
    {
        s_EditorResourceRoot = Normalize(root);
    }

    const std::filesystem::path& PlatformPath::GetEditorResourceRoot()
    {
        if (s_EditorResourceRoot.empty())
        {
            auto candidate = Normalize(GetProjectRoot() / "Editor/Resources");
            std::error_code ec;
            if (std::filesystem::is_directory(candidate, ec))
            {
                s_EditorResourceRoot = candidate;
            }
            else
            {
                s_EditorResourceRoot = Normalize(GetProjectRoot() / "Resources");
            }
        }
        return s_EditorResourceRoot;
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

    bool PlatformPath::IsEditorUri(const std::wstring& uri)
    {
        return StartsWithIgnoreCase(uri, L"editor://");
    }

    bool PlatformPath::IsEditorUri(const std::string& uri)
    {
        return StartsWithIgnoreCase(uri, "editor://");
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
        return IsEngineUri(uri) || IsEditorUri(uri) || IsProjectUri(uri);
    }

    bool PlatformPath::IsVirtualUri(const std::string& uri)
    {
        return IsEngineUri(uri) || IsEditorUri(uri) || IsProjectUri(uri);
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

        if (IsEditorUri(pathOrUri))
        {
            const std::wstring sub = pathOrUri.substr(9);
            const auto resolved = Normalize(GetEditorResourceRoot() / sub);
            if (!IsSubpathOf(resolved, GetEditorResourceRoot()) && resolved != GetEditorResourceRoot())
            {
                return GetEditorResourceRoot();
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

        const auto inEditor = Normalize(GetEditorResourceRoot() / fsPath);
        if (std::filesystem::exists(inEditor, ec))
            return inEditor;

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

            const auto strippedEditor = Normalize(GetEditorResourceRoot() / stripped);
            if (std::filesystem::exists(strippedEditor, ec))
                return strippedEditor;
        }

        const auto inCwd = Normalize(std::filesystem::current_path(ec) / fsPath);
        if (!ec && std::filesystem::exists(inCwd, ec))
            return inCwd;

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
        if (IsEngineUri(pathOrUri) || IsEditorUri(pathOrUri))
            return true;
        if (IsProjectUri(pathOrUri))
            return false;

        const auto resolved = ResolveUri(pathOrUri);
        const auto engineRoot = GetEngineResourceRoot();
        const auto editorRoot = GetEditorResourceRoot();
        const auto projectRoot = GetProjectAssetRoot();

        if (IsSubpathOf(resolved, engineRoot) || resolved == engineRoot)
        {
            if (engineRoot != projectRoot)
                return true;
            const auto rel = resolved.lexically_relative(engineRoot);
            return IsBuiltInRelativeResourcePath(rel.generic_wstring());
        }

        if (IsSubpathOf(resolved, editorRoot) || resolved == editorRoot)
        {
            return true;
        }

        return false;
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
