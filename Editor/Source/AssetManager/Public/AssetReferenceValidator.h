#pragma once
#include <string>
#include <vector>
#include <filesystem>
#include "Engine_API.h"

namespace Editor
{
    enum class EAssetReferenceIssueType
    {
        MissingFile,          // 디스크에 대상 파일이 존재하지 않음 (누락)
        ExternalAbsolutePath, // 프로젝트 루트 외부의 절대 경로가 하드코딩됨
        InvalidExtension      // 확장자가 누락되었거나 구형/손상 바이너리 참조
    };

    struct AssetReferenceIssue
    {
        std::filesystem::path SourceFile;        // 결함이 발견된 에셋 파일 (상대 경로)
        std::string PropertyName;                // 참조 속성 (AssetKey, AssetPath 등)
        std::string TargetReference;             // 기록된 잘못된 대상 경로 문자열
        std::filesystem::path SuggestedFix;      // 안전한 자동 복구 제안 경로
        EAssetReferenceIssueType IssueType = EAssetReferenceIssueType::MissingFile;
        bool CanAutoFix = false;                 // 자동 복구 가능 여부
        std::string Description;                 // 상세 설명
    };

    class AssetReferenceValidator
    {
    public:
        // 단일 파일의 직렬화 참조 검증
        static std::vector<AssetReferenceIssue> ValidateFile(const std::filesystem::path& filePath);

        // Resources 폴더 내 모든 에셋(씬, 프리팹, 머티리얼, 모델) 전수 검증
        static std::vector<AssetReferenceIssue> ValidateAllResources();

        // 손상/누락 경로에 대한 안전한 대체 복구 경로 탐색
        static bool TryResolveFallbackPath(const std::string& brokenPath, std::filesystem::path& outResolvedPath);

        // 단일 이슈 원자적 파일 복구 치환
        static bool FixIssue(const AssetReferenceIssue& issue);

        // 안전 복구 가능한 모든 이슈 일괄 치환
        static size_t FixAllSafeIssues(const std::vector<AssetReferenceIssue>& issues);
    };
}