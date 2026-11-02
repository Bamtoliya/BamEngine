#pragma once
#include <filesystem>
#include <string>
#include "Engine_API.h"

namespace Engine
{
    class ENGINE_API PlatformPath
    {
    public:
        static std::filesystem::path GetExecutableDirectory();
        static std::filesystem::path GetSystemFontDirectory();
        static std::filesystem::path Normalize(const std::filesystem::path& path);

        // 프로젝트 루트 설정 및 조회
        static void SetProjectRoot(const std::filesystem::path& root);
        static const std::filesystem::path& GetProjectRoot();

        // 상대 경로를 프로젝트 루트 기준 절대 경로로 안전하게 결합
        static std::filesystem::path ResolveProjectRelative(const std::filesystem::path& relativePath);
        // 절대 경로 또는 상대 경로를 프로젝트 루트 기준 상대 경로("Resources/...")로 안전하게 정규화
        static std::filesystem::path ToProjectRelative(const std::filesystem::path& path);
    };
}