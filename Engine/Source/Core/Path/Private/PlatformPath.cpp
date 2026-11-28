#include "PlatformPath.h"
#include "Functions.h"
#include <SDL3/SDL_filesystem.h>
#if defined(_WIN32)
#include <windows.h>
#include <shlobj.h>
#endif
#include <algorithm>
#include <cwctype>
#include <fstream>
#include <mutex>

namespace Engine
{
    static std::filesystem::path s_EngineRoot;
    static std::filesystem::path s_ProjectRoot;
    static std::filesystem::path s_EngineResourceRoot;
    static std::filesystem::path s_ProjectAssetRoot;
    static bool s_EngineResourceRootExplicit = false;
    static bool s_ProjectAssetRootExplicit = false;

    static std::mutex s_VirtualSchemeMutex;
    static std::vector<VirtualSchemeDesc> s_VirtualSchemes;

    struct PluginMountEntry
    {
        std::string Id;
        std::filesystem::path ContentRoot;
    };
    static std::mutex s_PluginMountMutex;
    static std::vector<PluginMountEntry> s_PluginMounts;

    static std::mutex s_ContentMountMutex;
    static std::vector<ContentMountDesc> s_ContentMounts;

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

        bool EqualsIgnoreCase(std::string_view lhs, std::string_view rhs)
        {
            return lhs.size() == rhs.size() && StartsWithIgnoreCase(lhs, rhs);
        }

        bool EqualsIgnoreCase(std::wstring_view lhs, std::wstring_view rhs)
        {
            return lhs.size() == rhs.size() && StartsWithIgnoreCase(lhs, rhs);
        }

        std::wstring_view TrimSchemeSuffix(std::wstring_view scheme)
        {
            if (scheme.size() >= 3 && scheme.substr(scheme.size() - 3) == L"://")
                return scheme.substr(0, scheme.size() - 3);
            return scheme;
        }

        std::string_view TrimSchemeSuffix(std::string_view scheme)
        {
            if (scheme.size() >= 3 && scheme.substr(scheme.size() - 3) == "://")
                return scheme.substr(0, scheme.size() - 3);
            return scheme;
        }

        bool MatchesSchemePrefix(std::wstring_view uri, std::wstring_view scheme)
        {
            scheme = TrimSchemeSuffix(scheme);
            if (scheme.empty()) return false;
            return uri.size() >= scheme.size() + 3 &&
                StartsWithIgnoreCase(uri, scheme) &&
                uri.substr(scheme.size(), 3) == L"://";
        }

        bool MatchesSchemePrefix(std::string_view uri, std::string_view scheme)
        {
            scheme = TrimSchemeSuffix(scheme);
            if (scheme.empty()) return false;
            return uri.size() >= scheme.size() + 3 &&
                StartsWithIgnoreCase(uri, scheme) &&
                uri.substr(scheme.size(), 3) == "://";
        }

        bool IsValidContentMountName(std::string_view name)
        {
            if (name.empty() || name == "." || name == "..") return false;
            bool hasNonSpace = false;
            for (char ch : name)
            {
                if (ch == '/' || ch == '\\' || ch == ':' || ch == '*' || ch == '?' || ch == '"' || ch == '<' || ch == '>' || ch == '|')
                    return false;
                if (ch != ' ' && ch != '\t' && ch != '\r' && ch != '\n')
                    hasNonSpace = true;
            }
            return hasNonSpace;
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

    void PlatformPath::SetEngineRoot(const std::filesystem::path& root)
    {
        s_EngineRoot = root.empty() ? std::filesystem::path{} : Normalize(root);
        if (!s_EngineResourceRootExplicit) s_EngineResourceRoot.clear();
    }

    const std::filesystem::path& PlatformPath::GetEngineRoot()
    {
        if (!s_EngineRoot.empty()) return s_EngineRoot;
        return GetProjectRoot();
    }

    void PlatformPath::SetProjectRoot(const std::filesystem::path& root)
    {
        s_ProjectRoot = root.empty() ? std::filesystem::path{} : Normalize(root);
        if (!s_ProjectAssetRootExplicit) s_ProjectAssetRoot.clear();
        if (s_EngineRoot.empty())
        {
            if (!s_EngineResourceRootExplicit) s_EngineResourceRoot.clear();
        }
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

    std::vector<std::filesystem::path> PlatformPath::GetWorktreeRoots()
    {
        std::vector<std::filesystem::path> roots;
        const auto currentRoot = GetProjectRoot();
        if (currentRoot.empty()) return roots;

        roots.push_back(currentRoot);

        std::error_code ec;
        const auto gitPath = currentRoot / ".git";
        std::filesystem::path mainGitDir;

        if (std::filesystem::is_directory(gitPath, ec))
        {
            mainGitDir = gitPath;
        }
        else if (std::filesystem::is_regular_file(gitPath, ec))
        {
            std::ifstream file(gitPath);
            if (file.is_open())
            {
                std::string line;
                if (std::getline(file, line))
                {
                    const std::string prefix = "gitdir:";
                    const auto pos = line.find(prefix);
                    if (pos != std::string::npos)
                    {
                        std::string gitdirStr = line.substr(pos + prefix.length());
                        const auto first = gitdirStr.find_first_not_of(" \t\r\n");
                        const auto last = gitdirStr.find_last_not_of(" \t\r\n");
                        if (first != std::string::npos && last != std::string::npos)
                        {
                            gitdirStr = gitdirStr.substr(first, last - first + 1);
                            std::filesystem::path linkedGitDir = Utf8ToPath(gitdirStr);
                            if (linkedGitDir.is_relative()) linkedGitDir = currentRoot / linkedGitDir;
                            linkedGitDir = Normalize(linkedGitDir);

                            if (linkedGitDir.parent_path().filename() == "worktrees")
                            {
                                mainGitDir = linkedGitDir.parent_path().parent_path();
                            }
                        }
                    }
                }
            }
        }

        if (!mainGitDir.empty() && std::filesystem::is_directory(mainGitDir, ec))
        {
            const auto mainRoot = Normalize(mainGitDir.parent_path());
            if (std::filesystem::is_directory(mainRoot, ec))
            {
                if (std::find(roots.begin(), roots.end(), mainRoot) == roots.end())
                    roots.push_back(mainRoot);
            }

            const auto worktreesDir = mainGitDir / "worktrees";
            if (std::filesystem::is_directory(worktreesDir, ec))
            {
                for (const auto& entry : std::filesystem::directory_iterator(worktreesDir, ec))
                {
                    if (!entry.is_directory(ec)) continue;

                    const auto gitdirFile = entry.path() / "gitdir";
                    if (!std::filesystem::is_regular_file(gitdirFile, ec)) continue;

                    std::ifstream subFile(gitdirFile);
                    if (!subFile.is_open()) continue;

                    std::string line;
                    if (std::getline(subFile, line))
                    {
                        const auto first = line.find_first_not_of(" \t\r\n");
                        const auto last = line.find_last_not_of(" \t\r\n");
                        if (first != std::string::npos && last != std::string::npos)
                        {
                            std::filesystem::path subGitPath = Utf8ToPath(line.substr(first, last - first + 1));
                            if (subGitPath.is_relative()) subGitPath = mainGitDir / subGitPath;
                            subGitPath = Normalize(subGitPath);

                            const auto wtRoot = Normalize(subGitPath.parent_path());
                            if (std::filesystem::is_directory(wtRoot, ec))
                            {
                                if (std::find(roots.begin(), roots.end(), wtRoot) == roots.end())
                                    roots.push_back(wtRoot);
                            }
                        }
                    }
                }
            }
        }

        return roots;
    }

    void PlatformPath::SetEngineResourceRoot(const std::filesystem::path& root)
    {
        s_EngineResourceRoot = root.empty() ? std::filesystem::path{} : Normalize(root);
        s_EngineResourceRootExplicit = !root.empty();
    }

    const std::filesystem::path& PlatformPath::GetEngineResourceRoot()
    {
        if (s_EngineResourceRoot.empty())
        {
            const auto& engineRoot = GetEngineRoot();
            auto candidate = Normalize(engineRoot / "Engine/Resources");
            std::error_code ec;
            if (std::filesystem::is_directory(candidate, ec))
            {
                s_EngineResourceRoot = candidate;
            }
            else
            {
                s_EngineResourceRoot = Normalize(engineRoot / "Resources");
            }
        }
        return s_EngineResourceRoot;
    }

    void PlatformPath::RegisterVirtualScheme(const VirtualSchemeDesc& desc)
    {
        const std::wstring_view trimmed = TrimSchemeSuffix(desc.Scheme);
        if (trimmed.empty()) return;

        VirtualSchemeDesc normalized = desc;
        normalized.Scheme = std::wstring(trimmed);
        normalized.RootPath = desc.RootPath.empty() ? std::filesystem::path{} : Normalize(desc.RootPath);

        const std::lock_guard lock(s_VirtualSchemeMutex);
        for (auto& existing : s_VirtualSchemes)
        {
            if (EqualsIgnoreCase(existing.Scheme, normalized.Scheme))
            {
                existing.RootPath = std::move(normalized.RootPath);
                existing.ReadOnly = normalized.ReadOnly;
                existing.IncludeInFallbackResolve = normalized.IncludeInFallbackResolve;
                return;
            }
        }
        s_VirtualSchemes.push_back(std::move(normalized));
    }

    void PlatformPath::UnregisterVirtualScheme(std::wstring_view scheme)
    {
        const std::wstring_view trimmed = TrimSchemeSuffix(scheme);
        if (trimmed.empty()) return;
        const std::lock_guard lock(s_VirtualSchemeMutex);
        std::erase_if(s_VirtualSchemes, [trimmed](const VirtualSchemeDesc& s) {
            return EqualsIgnoreCase(s.Scheme, trimmed);
        });
    }

    void PlatformPath::ClearVirtualSchemes()
    {
        const std::lock_guard lock(s_VirtualSchemeMutex);
        s_VirtualSchemes.clear();
    }

    std::filesystem::path PlatformPath::GetVirtualSchemeRoot(std::wstring_view scheme)
    {
        const std::wstring_view trimmed = TrimSchemeSuffix(scheme);
        if (trimmed.empty()) return {};
        const std::lock_guard lock(s_VirtualSchemeMutex);
        for (const auto& s : s_VirtualSchemes)
        {
            if (EqualsIgnoreCase(s.Scheme, trimmed))
                return s.RootPath;
        }
        return {};
    }

    bool PlatformPath::IsVirtualSchemeUri(const std::wstring& uri, std::wstring_view scheme)
    {
        return MatchesSchemePrefix(uri, scheme);
    }

    bool PlatformPath::IsVirtualSchemeUri(const std::string& uri, std::string_view scheme)
    {
        return MatchesSchemePrefix(uri, scheme);
    }

    void PlatformPath::SetProjectAssetRoot(const std::filesystem::path& root)
    {
        s_ProjectAssetRoot = root.empty() ? std::filesystem::path{} : Normalize(root);
        s_ProjectAssetRootExplicit = !root.empty();
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

    bool PlatformPath::IsPluginUri(std::wstring_view uri)
    {
        return StartsWithIgnoreCase(uri, L"plugin://");
    }

    bool PlatformPath::IsPluginUri(std::string_view uri)
    {
        return StartsWithIgnoreCase(uri, "plugin://");
    }

    bool PlatformPath::IsMountUri(std::wstring_view uri)
    {
        return StartsWithIgnoreCase(uri, L"mount://");
    }

    bool PlatformPath::IsMountUri(std::string_view uri)
    {
        return StartsWithIgnoreCase(uri, "mount://");
    }

    bool PlatformPath::IsVirtualUri(std::wstring_view uri)
    {
        if (IsEngineUri(std::wstring(uri)) || IsProjectUri(std::wstring(uri)) || IsPluginUri(uri) || IsMountUri(uri))
            return true;
        const std::lock_guard lock(s_VirtualSchemeMutex);
        for (const auto& s : s_VirtualSchemes)
        {
            if (MatchesSchemePrefix(uri, s.Scheme))
                return true;
        }
        return false;
    }

    bool PlatformPath::IsVirtualUri(std::string_view uri)
    {
        if (IsEngineUri(std::string(uri)) || IsProjectUri(std::string(uri)) || IsPluginUri(uri) || IsMountUri(uri))
            return true;
        const std::wstring wuri = Utf8ToPath(uri).wstring();
        const std::lock_guard lock(s_VirtualSchemeMutex);
        for (const auto& s : s_VirtualSchemes)
        {
            if (MatchesSchemePrefix(wuri, s.Scheme))
                return true;
        }
        return false;
    }

    void PlatformPath::RegisterPluginMount(std::string_view pluginId, const std::filesystem::path& contentRoot)
    {
        if (pluginId.empty()) return;
        const std::lock_guard lock(s_PluginMountMutex);
        const auto normRoot = Normalize(contentRoot);
        for (auto& mount : s_PluginMounts)
        {
            if (StartsWithIgnoreCase(mount.Id, pluginId) && mount.Id.size() == pluginId.size())
            {
                mount.ContentRoot = normRoot;
                return;
            }
        }
        s_PluginMounts.push_back({ std::string(pluginId), normRoot });
    }

    void PlatformPath::UnregisterPluginMount(std::string_view pluginId)
    {
        if (pluginId.empty()) return;
        const std::lock_guard lock(s_PluginMountMutex);
        std::erase_if(s_PluginMounts, [pluginId](const PluginMountEntry& m) {
            return StartsWithIgnoreCase(m.Id, pluginId) && m.Id.size() == pluginId.size();
        });
    }

    void PlatformPath::ClearPluginMounts()
    {
        const std::lock_guard lock(s_PluginMountMutex);
        s_PluginMounts.clear();
    }

    bool PlatformPath::HasPluginMount(std::string_view pluginId)
    {
        if (pluginId.empty()) return false;
        const std::lock_guard lock(s_PluginMountMutex);
        for (const auto& mount : s_PluginMounts)
        {
            if (StartsWithIgnoreCase(mount.Id, pluginId) && mount.Id.size() == pluginId.size())
                return true;
        }
        return false;
    }

    std::filesystem::path PlatformPath::GetPluginMountRoot(std::string_view pluginId)
    {
        if (pluginId.empty()) return {};
        const std::lock_guard lock(s_PluginMountMutex);
        for (const auto& mount : s_PluginMounts)
        {
            if (StartsWithIgnoreCase(mount.Id, pluginId) && mount.Id.size() == pluginId.size())
                return mount.ContentRoot;
        }
        return {};
    }

    std::filesystem::path PlatformPath::ResolvePluginUri(std::wstring_view uri)
    {
        if (!IsPluginUri(uri)) return {};
        std::wstring_view rest = uri.substr(9);
        if (rest.empty()) return {};

        const size_t slashPos = rest.find_first_of(L"/\\");
        std::wstring_view pluginIdW;
        std::wstring_view subPathW;
        if (slashPos == std::wstring_view::npos)
        {
            pluginIdW = rest;
        }
        else
        {
            pluginIdW = rest.substr(0, slashPos);
            subPathW = rest.substr(slashPos + 1);
        }

        if (pluginIdW.empty()) return {};

        std::string pluginId;

        if (!Unicode::TryWideToUtf8(pluginIdW, pluginId))
            return {};
        std::filesystem::path contentRoot;
        {
            const std::lock_guard lock(s_PluginMountMutex);
            for (const auto& mount : s_PluginMounts)
            {
                if (StartsWithIgnoreCase(mount.Id, pluginId) && mount.Id.size() == pluginId.size())
                {
                    contentRoot = mount.ContentRoot;
                    break;
                }
            }
        }

        if (contentRoot.empty()) return {};

        const auto subPath = std::filesystem::path(subPathW);
        const auto resolved = Normalize(contentRoot / subPath);

        // Security traversal check: block ".." path traversal outside contentRoot!
        if (!IsSubpathOf(resolved, contentRoot) && resolved != contentRoot)
        {
            return contentRoot;
        }
        return resolved;
    }

    std::filesystem::path PlatformPath::ResolvePluginUri(std::string_view uri)
    {
        if (!IsPluginUri(uri)) return {};
        std::wstring wstr;

        if (!Unicode::TryUtf8ToWide(uri, wstr))
            return {};
        return ResolvePluginUri(std::wstring_view(wstr));
    }

    bool PlatformPath::RegisterContentMount(const ContentMountDesc& desc)
    {
        if (!IsValidContentMountName(desc.Name) || desc.RootPath.empty())
            return false;

        ContentMountDesc normalized = desc;
        normalized.RootPath = Normalize(desc.RootPath);

        const std::lock_guard lock(s_ContentMountMutex);
        for (auto& mount : s_ContentMounts)
        {
            if (EqualsIgnoreCase(mount.Name, normalized.Name))
            {
                mount.RootPath = std::move(normalized.RootPath);
                mount.ReadOnly = normalized.ReadOnly;
                mount.ShowUnderAssets = normalized.ShowUnderAssets;
                return true;
            }
        }
        s_ContentMounts.push_back(std::move(normalized));
        return true;
    }

    void PlatformPath::UnregisterContentMount(std::string_view name)
    {
        if (name.empty()) return;
        const std::lock_guard lock(s_ContentMountMutex);
        std::erase_if(s_ContentMounts, [name](const ContentMountDesc& m) {
            return EqualsIgnoreCase(m.Name, name);
        });
    }

    void PlatformPath::ClearContentMounts()
    {
        const std::lock_guard lock(s_ContentMountMutex);
        s_ContentMounts.clear();
    }

    std::vector<ContentMountDesc> PlatformPath::GetContentMounts()
    {
        const std::lock_guard lock(s_ContentMountMutex);
        return s_ContentMounts;
    }

    bool PlatformPath::HasContentMount(std::string_view name)
    {
        if (name.empty()) return false;
        const std::lock_guard lock(s_ContentMountMutex);
        for (const auto& mount : s_ContentMounts)
        {
            if (EqualsIgnoreCase(mount.Name, name))
                return true;
        }
        return false;
    }

    std::filesystem::path PlatformPath::ResolveMountUri(std::wstring_view uri)
    {
        if (!IsMountUri(uri)) return {};
        std::wstring_view rest = uri.substr(8);
        if (rest.empty()) return {};

        const size_t slashPos = rest.find_first_of(L"/\\");
        std::wstring_view aliasW;
        std::wstring_view subPathW;
        if (slashPos == std::wstring_view::npos)
        {
            aliasW = rest;
        }
        else
        {
            aliasW = rest.substr(0, slashPos);
            subPathW = rest.substr(slashPos + 1);
        }

        if (aliasW.empty()) return {};

        const std::string alias = Engine::Unicode::WideToUtf8(std::wstring(aliasW));
        std::filesystem::path mountRoot;
        {
            const std::lock_guard lock(s_ContentMountMutex);
            for (const auto& mount : s_ContentMounts)
            {
                if (EqualsIgnoreCase(mount.Name, alias))
                {
                    mountRoot = mount.RootPath;
                    break;
                }
            }
        }

        if (mountRoot.empty()) return {};

        if (subPathW.empty()) return mountRoot;

        const auto subPath = std::filesystem::path(subPathW);
        const auto resolved = Normalize(mountRoot / subPath);

        if (!IsSubpathOf(resolved, mountRoot) && !IsSamePath(resolved, mountRoot))
        {
            return mountRoot;
        }
        return resolved;
    }

    std::filesystem::path PlatformPath::ResolveMountUri(std::string_view uri)
    {
        if (!IsMountUri(uri)) return {};
        const std::wstring wstr = Utf8ToPath(uri).wstring();
        return ResolveMountUri(std::wstring_view(wstr));
    }

    bool PlatformPath::IsMountReadOnly(std::string_view nameOrUri)
    {
        if (nameOrUri.empty()) return false;

        std::string_view alias = nameOrUri;
        if (IsMountUri(nameOrUri))
        {
            std::string_view rest = nameOrUri.substr(8);
            const size_t slashPos = rest.find_first_of("/\\");
            alias = (slashPos == std::string_view::npos) ? rest : rest.substr(0, slashPos);
        }

        if (alias.empty()) return false;

        const std::lock_guard lock(s_ContentMountMutex);
        for (const auto& mount : s_ContentMounts)
        {
            if (EqualsIgnoreCase(mount.Name, alias))
                return mount.ReadOnly;
        }
        return false;
    }

    bool PlatformPath::IsMountReadOnly(std::wstring_view nameOrUri)
    {
        if (nameOrUri.empty()) return false;
        const std::string utf8 = Engine::Unicode::WideToUtf8(std::wstring(nameOrUri));
        return IsMountReadOnly(std::string_view(utf8));
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

        if (IsPluginUri(pathOrUri))
        {
            return ResolvePluginUri(pathOrUri);
        }

        if (IsMountUri(pathOrUri))
        {
            return ResolveMountUri(pathOrUri);
        }

        {
            std::filesystem::path schemeRoot;
            size_t prefixLen = 0;
            {
                const std::lock_guard lock(s_VirtualSchemeMutex);
                for (const auto& s : s_VirtualSchemes)
                {
                    if (MatchesSchemePrefix(pathOrUri, s.Scheme))
                    {
                        schemeRoot = s.RootPath;
                        prefixLen = s.Scheme.size() + 3;
                        break;
                    }
                }
            }
            if (prefixLen > 0)
            {
                if (schemeRoot.empty()) return {};
                const std::wstring sub = pathOrUri.substr(prefixLen);
                const auto resolved = Normalize(schemeRoot / sub);
                if (!IsSubpathOf(resolved, schemeRoot) && !IsSamePath(resolved, schemeRoot))
                {
                    return schemeRoot;
                }
                return resolved;
            }
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

        std::vector<std::filesystem::path> fallbackRoots;
        {
            const std::lock_guard lock(s_VirtualSchemeMutex);
            for (const auto& s : s_VirtualSchemes)
            {
                if (s.IncludeInFallbackResolve && !s.RootPath.empty())
                    fallbackRoots.push_back(s.RootPath);
            }
        }

        for (const auto& fallbackRoot : fallbackRoots)
        {
            const auto inScheme = Normalize(fallbackRoot / fsPath);
            if (std::filesystem::exists(inScheme, ec))
                return inScheme;
        }

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

            for (const auto& fallbackRoot : fallbackRoots)
            {
                const auto strippedScheme = Normalize(fallbackRoot / stripped);
                if (std::filesystem::exists(strippedScheme, ec))
                    return strippedScheme;
            }
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
        return ResolveUri(Utf8ToPath(pathOrUri).wstring());
    }

    std::wstring PlatformPath::ToVirtualUri(const std::filesystem::path& physicalPath)
    {
        if (physicalPath.empty()) return {};

        const std::wstring rawStr = physicalPath.wstring();
        if (IsVirtualUri(rawStr)) return rawStr;

        const auto norm = physicalPath.is_absolute() ? Normalize(physicalPath) : ResolveUri(physicalPath);

        const auto engineRoot = GetEngineResourceRoot();
        const auto projectRoot = GetProjectAssetRoot();

        // Priority order: Project:// -> Engine:// -> registered virtual schemes -> Plugin:// -> Mount://<Name>/
        if (projectRoot != engineRoot && (IsSubpathOf(norm, projectRoot) || IsSamePath(norm, projectRoot)))
        {
            auto rel = norm.lexically_relative(projectRoot);
            const auto relW = rel.generic_wstring();
            if (relW.empty() || relW == L".")
                return L"Project://";
            return L"Project://" + relW;
        }

        if (engineRoot != projectRoot && (IsSubpathOf(norm, engineRoot) || IsSamePath(norm, engineRoot)))
        {
            auto rel = norm.lexically_relative(engineRoot);
            const auto relW = rel.generic_wstring();
            if (relW.empty() || relW == L".")
                return L"Engine://";
            return L"Engine://" + relW;
        }

        {
            const std::lock_guard lock(s_VirtualSchemeMutex);
            for (const auto& s : s_VirtualSchemes)
            {
                if (s.RootPath.empty() || s.RootPath == projectRoot) continue;
                if (IsSubpathOf(norm, s.RootPath) || IsSamePath(norm, s.RootPath))
                {
                    auto rel = norm.lexically_relative(s.RootPath);
                    const auto relW = rel.generic_wstring();
                    if (relW.empty() || relW == L".")
                        return s.Scheme + L"://";
                    return s.Scheme + L"://" + relW;
                }
            }
        }

        // Check registered plugin content mounts
        {
            const std::lock_guard lock(s_PluginMountMutex);
            for (const auto& mount : s_PluginMounts)
            {
                if (mount.ContentRoot.empty()) continue;
                if (IsSubpathOf(norm, mount.ContentRoot) || IsSamePath(norm, mount.ContentRoot))
                {
                    auto rel = norm.lexically_relative(mount.ContentRoot);
                    const std::wstring idW = Utf8ToPath(mount.Id).wstring();
                    const auto relW = rel.generic_wstring();
                    if (relW.empty() || relW == L".")
                        return L"Plugin://" + idW;
                    return L"Plugin://" + idW + L"/" + relW;
                }
            }
        }

        // Check registered external content mounts (prefer longest matching RootPath)
        {
            const std::lock_guard lock(s_ContentMountMutex);
            const ContentMountDesc* bestMount = nullptr;
            size_t bestRootLen = 0;
            for (const auto& mount : s_ContentMounts)
            {
                if (mount.RootPath.empty()) continue;
                if (IsSubpathOf(norm, mount.RootPath) || IsSamePath(norm, mount.RootPath))
                {
                    const size_t rootLen = mount.RootPath.generic_wstring().size();
                    if (!bestMount || rootLen > bestRootLen)
                    {
                        bestMount = &mount;
                        bestRootLen = rootLen;
                    }
                }
            }
            if (bestMount)
            {
                auto rel = norm.lexically_relative(bestMount->RootPath);
                const std::wstring nameW = Utf8ToPath(bestMount->Name).wstring();
                const auto relW = rel.generic_wstring();
                if (relW.empty() || relW == L".")
                    return L"Mount://" + nameW;
                return L"Mount://" + nameW + L"/" + relW;
            }
        }

        return ToProjectRelative(norm).generic_wstring();
    }

    bool PlatformPath::IsReadOnlyTarget(const std::filesystem::path& pathOrUri)
    {
        return IsReadOnlyTarget(pathOrUri.wstring());
    }

    bool PlatformPath::IsReadOnlyTarget(const std::string& pathOrUri)
    {
        return IsReadOnlyTarget(Utf8ToPath(pathOrUri).wstring());
    }

    bool PlatformPath::IsReadOnlyTarget(const std::wstring& pathOrUri)
    {
        if (IsEngineUri(pathOrUri))
            return true;
        if (IsProjectUri(pathOrUri))
            return false;
        if (IsMountUri(pathOrUri))
            return IsMountReadOnly(pathOrUri);
        if (IsPluginUri(pathOrUri))
        {
            const auto resolved = ResolvePluginUri(pathOrUri);
            const auto engineRoot = GetEngineResourceRoot();
            if (IsSubpathOf(resolved, engineRoot) || IsSamePath(resolved, engineRoot))
                return true;
            return false;
        }
        {
            const std::lock_guard lock(s_VirtualSchemeMutex);
            for (const auto& s : s_VirtualSchemes)
            {
                if (MatchesSchemePrefix(pathOrUri, s.Scheme))
                    return s.ReadOnly;
            }
        }

        const auto resolved = ResolveUri(pathOrUri);
        const auto engineRoot = GetEngineResourceRoot();
        const auto projectRoot = GetProjectAssetRoot();

        if (IsSubpathOf(resolved, engineRoot) || IsSamePath(resolved, engineRoot))
        {
            if (engineRoot != projectRoot)
                return true;
            const auto rel = resolved.lexically_relative(engineRoot);
            return IsBuiltInRelativeResourcePath(rel.generic_wstring());
        }

        {
            const std::lock_guard lock(s_VirtualSchemeMutex);
            for (const auto& s : s_VirtualSchemes)
            {
                if (s.RootPath.empty()) continue;
                if (IsSubpathOf(resolved, s.RootPath) || IsSamePath(resolved, s.RootPath))
                    return s.ReadOnly;
            }
        }

        if (projectRoot != engineRoot && (IsSubpathOf(resolved, projectRoot) || IsSamePath(resolved, projectRoot)))
        {
            return false;
        }

        // Check registered external content mounts for physical paths (prefer longest matching RootPath)
        {
            const std::lock_guard lock(s_ContentMountMutex);
            const ContentMountDesc* bestMount = nullptr;
            size_t bestRootLen = 0;
            for (const auto& mount : s_ContentMounts)
            {
                if (mount.RootPath.empty()) continue;
                if (IsSubpathOf(resolved, mount.RootPath) || IsSamePath(resolved, mount.RootPath))
                {
                    const size_t rootLen = mount.RootPath.generic_wstring().size();
                    if (!bestMount || rootLen > bestRootLen)
                    {
                        bestMount = &mount;
                        bestRootLen = rootLen;
                    }
                }
            }
            if (bestMount)
                return bestMount->ReadOnly;
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

#if defined(_WIN32)
            wchar_t buffer[MAX_PATH] = {};
            GetModuleFileNameW(nullptr, buffer, MAX_PATH);
            return Normalize(std::filesystem::path(buffer).parent_path());
#else
            std::error_code ec;
            return Normalize(std::filesystem::current_path(ec));
#endif
            }();
        return cachedPath;
    }

    std::filesystem::path PlatformPath::GetSystemFontDirectory()
    {
#if defined(_WIN32)
        wchar_t path[MAX_PATH] = {};
        if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_FONTS, nullptr, SHGFP_TYPE_CURRENT, path)))
            return Normalize(std::filesystem::path(path));

        const char* windir = std::getenv("WINDIR");
        if (windir)
            return Normalize(std::filesystem::path(windir) / "Fonts");

        return Normalize(std::filesystem::path("C:/Windows/Fonts"));
#else
        return {};
#endif
    }

    bool PlatformPath::IsSamePath(const std::filesystem::path& lhs, const std::filesystem::path& rhs)
    {
        auto left = Normalize(lhs);
        auto right = Normalize(rhs);
        while (!left.empty() && left != left.root_path() && left.filename().empty()) left = left.parent_path();
        while (!right.empty() && right != right.root_path() && right.filename().empty()) right = right.parent_path();
#if defined(_WIN32)
        return CompareStringOrdinal(left.c_str(), -1, right.c_str(), -1, TRUE) == CSTR_EQUAL;
#else
        return left == right;
#endif
    }

    bool PlatformPath::IsFilePathAffected(const std::filesystem::path& pathOrUri,
        const std::vector<FilePathChange>& changes)
    {
        if (pathOrUri.empty()) return false;
        const auto physical = Normalize(ResolveUri(pathOrUri));
        return RemapFilePath(physical, changes) != physical;
    }

    std::filesystem::path PlatformPath::RemapFilePath(const std::filesystem::path& path,
        const std::vector<FilePathChange>& changes)
    {
        if (path.empty())
            return path;
        const auto normalized = Normalize(path);
        for (const auto& change : changes)
        {
            auto source = Normalize(change.Before);
            while (!source.empty() && source != source.root_path() && source.filename().empty())
                source = source.parent_path();
            auto part = normalized.begin();
            auto root = source.begin();
            while (part != normalized.end() && root != source.end() && IsSamePath(*part, *root))
            {
                ++part;
                ++root;
            }
            if (source.empty() || root != source.end())
                continue;
            auto result = Normalize(change.After);
            for (; part != normalized.end(); ++part)
                result /= *part;
            return result;
        }
        return normalized;
    }

    bool PlatformPath::PrepareUtf8FilePathUpdate(std::string& path, const std::vector<FilePathChange>& changes,
        std::vector<Utf8FilePathUpdate>& updates, bool asUri)
    {
        if (path.empty()) return false;
        const auto physical = Normalize(ResolveUri(Utf8ToPath(path)));
        const auto mapped = RemapFilePath(physical, changes);
        if (physical == mapped) return false;
        updates.push_back({ &path, Engine::Unicode::WideToUtf8(asUri || IsVirtualUri(path) ? ToVirtualUri(mapped) : mapped.wstring()) });
        return true;
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
