#pragma once
#include <filesystem>
#include <string>
#include <vector>
#include "Engine_API.h"

namespace Engine
{
    struct FilePathChange
    {
        std::filesystem::path Before;
        std::filesystem::path After;
    };

    struct Utf8FilePathUpdate
    {
        std::string* Path;
        std::string Replacement;
        void Apply() noexcept { Path->swap(Replacement); }
    };

    struct ContentMountDesc
    {
        std::string Name;
        std::filesystem::path RootPath;
        bool ReadOnly = true;
        bool ShowUnderAssets = false;
    };

    struct VirtualSchemeDesc
    {
        std::wstring Scheme;
        std::filesystem::path RootPath;
        bool ReadOnly = true;
        bool IncludeInFallbackResolve = false;
    };

    class ENGINE_API PlatformPath
    {
    public:
        static std::filesystem::path GetExecutableDirectory();
        static std::filesystem::path GetSystemFontDirectory();
        static std::filesystem::path Normalize(const std::filesystem::path& path);
        static bool IsSamePath(const std::filesystem::path& lhs, const std::filesystem::path& rhs);
        static bool IsFilePathAffected(const std::filesystem::path& pathOrUri, const std::vector<FilePathChange>& changes);
        // Physical paths only; no filesystem access. Windows path components compare without case.
        static std::filesystem::path RemapFilePath(const std::filesystem::path& path,
            const std::vector<FilePathChange>& changes);
        static bool PrepareUtf8FilePathUpdate(std::string& path, const std::vector<FilePathChange>& changes,
            std::vector<Utf8FilePathUpdate>& updates, bool asUri = false);

        // 엔진 루트 설정 및 조회 (미설정 시 프로젝트 루트로 폴백)
        static void SetEngineRoot(const std::filesystem::path& root);
        static const std::filesystem::path& GetEngineRoot();

        // 프로젝트 루트 설정 및 조회
        static void SetProjectRoot(const std::filesystem::path& root);
        static const std::filesystem::path& GetProjectRoot();
        // 연결된 모든 Git 워크트리 루트 경로 목록 조회 (현재 프로젝트 루트 포함)
        static std::vector<std::filesystem::path> GetWorktreeRoots();

        // [에셋 아키텍처 Phase 1~2: 가상 마운트 URI 시스템]
        // 엔진 시스템 빌트인 리소스 루트 ("Engine://")
        static void SetEngineResourceRoot(const std::filesystem::path& root);
        static const std::filesystem::path& GetEngineResourceRoot();

        // 사용자 프로젝트 애셋 루트 ("Project://", "Asset://")
        static void SetProjectAssetRoot(const std::filesystem::path& root);
        static const std::filesystem::path& GetProjectAssetRoot();

        // 범용 가상 스키마 등록/조회 ("<Scheme>://...")
        static void RegisterVirtualScheme(const VirtualSchemeDesc& desc);
        static void UnregisterVirtualScheme(std::wstring_view scheme);
        static void ClearVirtualSchemes();
        static std::filesystem::path GetVirtualSchemeRoot(std::wstring_view scheme);
        static bool IsVirtualSchemeUri(const std::wstring& uri, std::wstring_view scheme);
        static bool IsVirtualSchemeUri(const std::string& uri, std::string_view scheme);

        // 가상 URI 스키마 판별
        static bool IsEngineUri(const std::wstring& uri);
        static bool IsEngineUri(const std::string& uri);
        static bool IsProjectUri(const std::wstring& uri);
        static bool IsProjectUri(const std::string& uri);
        static bool IsPluginUri(std::wstring_view uri);
        static bool IsPluginUri(std::string_view uri);
        static bool IsMountUri(std::wstring_view uri);
        static bool IsMountUri(std::string_view uri);
        static bool IsVirtualUri(std::wstring_view uri);
        static bool IsVirtualUri(std::string_view uri);

        // [플러그인 가상 마운트 URI 시스템 ("Plugin://<pluginId>/...")]
        static void RegisterPluginMount(std::string_view pluginId, const std::filesystem::path& contentRoot);
        static void UnregisterPluginMount(std::string_view pluginId);
        static void ClearPluginMounts();
        static bool HasPluginMount(std::string_view pluginId);
        static std::filesystem::path GetPluginMountRoot(std::string_view pluginId);
        static std::filesystem::path ResolvePluginUri(std::wstring_view uri);
        static std::filesystem::path ResolvePluginUri(std::string_view uri);

        // [외부 콘텐츠 폴더 가상 마운트 URI 시스템 ("Mount://<Alias>/...")]
        static bool RegisterContentMount(const ContentMountDesc& desc);
        static void UnregisterContentMount(std::string_view name);
        static void ClearContentMounts();
        static std::vector<ContentMountDesc> GetContentMounts();
        static bool HasContentMount(std::string_view name);
        static std::filesystem::path ResolveMountUri(std::wstring_view uri);
        static std::filesystem::path ResolveMountUri(std::string_view uri);
        static bool IsMountReadOnly(std::string_view nameOrUri);
        static bool IsMountReadOnly(std::wstring_view nameOrUri);

        // 가상 URI 또는 상대/절대 경로를 물리 디스크 절대 경로로 안전하게 해석
        static std::filesystem::path ResolveUri(const std::filesystem::path& pathOrUri);
        static std::filesystem::path ResolveUri(const std::wstring& pathOrUri);
        static std::filesystem::path ResolveUri(const std::string& pathOrUri);
        static std::filesystem::path ResolveUri(const wchar_t* pathOrUri) { return ResolveUri(std::wstring(pathOrUri ? pathOrUri : L"")); }
        static std::filesystem::path ResolveUri(const char* pathOrUri) { return ResolveUri(std::string(pathOrUri ? pathOrUri : "")); }

        // 물리 디스크 경로를 가상 URI("Project://...", "Engine://...", "Plugin://...", "Mount://...")로 안전하게 정규화
        static std::wstring ToVirtualUri(const std::filesystem::path& physicalPath);

        // 읽기 전용 가드: Engine://, ReadOnly 가상 스키마, ReadOnly Mount:// 또는 해당 루트 하위인지 여부
        static bool IsReadOnlyTarget(const std::filesystem::path& pathOrUri);
        static bool IsReadOnlyTarget(const std::wstring& pathOrUri);
        static bool IsReadOnlyTarget(const std::string& pathOrUri);
        static bool IsReadOnlyTarget(const wchar_t* pathOrUri) { return IsReadOnlyTarget(std::wstring(pathOrUri ? pathOrUri : L"")); }
        static bool IsReadOnlyTarget(const char* pathOrUri) { return IsReadOnlyTarget(std::string(pathOrUri ? pathOrUri : "")); }

        // 상대 경로를 프로젝트 루트 기준 절대 경로로 안전하게 결합
        static std::filesystem::path ResolveProjectRelative(const std::filesystem::path& relativePath);
        // 절대 경로 또는 상대 경로를 프로젝트 루트 기준 상대 경로("Resources/...")로 안전하게 정규화
        static std::filesystem::path ToProjectRelative(const std::filesystem::path& path);
    };
}
