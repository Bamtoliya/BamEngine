#include "PlatformPath.h"
#include <SDL3/SDL_filesystem.h>
#include <windows.h>
#include <shlobj.h>

namespace Engine
{
    static std::filesystem::path s_ProjectRoot;

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

    std::filesystem::path Engine::PlatformPath::ToProjectRelative(const std::filesystem::path& path)
    {
        if (path.empty()) return path;
        std::error_code ec;
        const std::filesystem::path normalized = path.lexically_normal();
        const std::filesystem::path root = GetProjectRoot().lexically_normal();
        if (root.empty()) return normalized;
        // 경로가 프로젝트 루트로 시작하면 상대 경로로 변환
        auto rel = normalized.lexically_relative(root);
        if (!rel.empty() && rel.native()[0] != L'.' && !rel.is_absolute())
        {
            return rel.generic_wstring();
        }
        return normalized.generic_wstring();
    }
}