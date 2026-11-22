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

        // 프로젝트 루트 설정 및 조회
        static void SetProjectRoot(const std::filesystem::path& root);
        static const std::filesystem::path& GetProjectRoot();
        // 연결된 모든 Git 워크트리 루트 경로 목록 조회 (현재 프로젝트 루트 포함)
        static std::vector<std::filesystem::path> GetWorktreeRoots();

        // [에셋 아키텍처 Phase 1~2: 가상 마운트 URI 시스템]
        // 엔진 시스템 빌트인 리소스 루트 ("Engine://")
        static void SetEngineResourceRoot(const std::filesystem::path& root);
        static const std::filesystem::path& GetEngineResourceRoot();

        // 에디터 시스템 리소스 루트 ("Editor://")
        static void SetEditorResourceRoot(const std::filesystem::path& root);
        static const std::filesystem::path& GetEditorResourceRoot();

        // 사용자 프로젝트 애셋 루트 ("Project://", "Asset://")
        static void SetProjectAssetRoot(const std::filesystem::path& root);
        static const std::filesystem::path& GetProjectAssetRoot();

        // 가상 URI 스키마 판별
        static bool IsEngineUri(const std::wstring& uri);
        static bool IsEngineUri(const std::string& uri);
        static bool IsEditorUri(const std::wstring& uri);
        static bool IsEditorUri(const std::string& uri);
        static bool IsProjectUri(const std::wstring& uri);
        static bool IsProjectUri(const std::string& uri);
        static bool IsPluginUri(std::wstring_view uri);
        static bool IsPluginUri(std::string_view uri);
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

        // 가상 URI 또는 상대/절대 경로를 물리 디스크 절대 경로로 안전하게 해석
        static std::filesystem::path ResolveUri(const std::filesystem::path& pathOrUri);
        static std::filesystem::path ResolveUri(const std::wstring& pathOrUri);
        static std::filesystem::path ResolveUri(const std::string& pathOrUri);
        static std::filesystem::path ResolveUri(const wchar_t* pathOrUri) { return ResolveUri(std::wstring(pathOrUri ? pathOrUri : L"")); }
        static std::filesystem::path ResolveUri(const char* pathOrUri) { return ResolveUri(std::string(pathOrUri ? pathOrUri : "")); }

        // 물리 디스크 경로를 가상 URI("Engine://...", "Project://...")로 안전하게 정규화
        static std::wstring ToVirtualUri(const std::filesystem::path& physicalPath);

        // 엔진 빌트인 읽기 전용 가드: 대상 경로가 Engine:// 이거나 EngineResourceRoot 하위인지 여부
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
