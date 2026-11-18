#pragma once
#include <string>
#include <vector>
#include <filesystem>
#include <span>
#include <unordered_set>
#include "Engine_API.h"
#include "PlatformPath.h"

namespace Editor
{
    struct AssetReferenceScanTiming;

    enum class EAssetReferenceIssueType
    {
        MissingFile,          // 디스크에 대상 파일이 존재하지 않음 (누락)
        ExternalAbsolutePath, // 프로젝트 루트 외부의 절대 경로가 하드코딩됨
        InvalidExtension,      // 확장자가 누락되었거나 구형/손상 바이너리 참조
        UnreadableDocument,
        OutsideRuntimeRoots
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

    struct AssetReferenceLink
    {
        std::filesystem::path Source;
        std::filesystem::path Target;
    };

    struct AssetReferenceScan
    {
        std::vector<AssetReferenceIssue> Issues;
        std::vector<AssetReferenceLink> References;
    };

    struct AssetReferenceScanDelta
    {
        struct RemovedIssue
        {
            size_t PreviousIndex;
            EAssetReferenceIssueType IssueType;
            bool CanAutoFix;
        };

        // Removed rows are in previous list order. New rows form a suffix of the updated list.
        std::vector<RemovedIssue> RemovedIssues;
        size_t FirstAddedIssue = 0;
        std::vector<AssetReferenceLink> RemovedReferences;
        std::vector<AssetReferenceLink> AddedReferences;
    };

    struct AssetReferenceScanIndex
    {
        std::filesystem::path Root;
        std::filesystem::path ProjectRoot;
        std::filesystem::path EngineRoot;
        std::filesystem::path EditorRoot;
        std::unordered_set<std::filesystem::path> Documents;
    };

    struct AssetReferenceChanges
    {
        bool RequiresFullScan = false;
        std::vector<std::filesystem::path> Documents;

        bool Empty() const { return !RequiresFullScan && Documents.empty(); }
        void RequireFullScan();
        void ObserveFile(std::filesystem::path path, bool inProject, bool membershipChanged);
        void Merge(AssetReferenceChanges changes);

    private:
        void AddDocument(std::filesystem::path path);
    };

    struct AssetReferenceImpact
    {
        std::vector<AssetReferenceLink> References;
        bool IsComplete = true;
    };

    using AssetPathMove = Engine::FilePathChange;

    struct AssetReferenceEdit
    {
        std::filesystem::path BeforePath;
        std::filesystem::path AfterPath;
        std::string BeforeText;
        std::string AfterText;
    };

    enum class EAssetReferenceWriteResult { Success, Failed, RollbackFailed };

    class AssetReferenceValidator
    {
    public:
        // 단일 파일의 직렬화 참조 검증
        static std::vector<AssetReferenceIssue> ValidateFile(const std::filesystem::path& filePath);

        static std::vector<AssetReferenceIssue> ValidateBuildScene(const std::filesystem::path& scenePath,
            std::vector<std::filesystem::path>* referencedFiles = nullptr);

        // 파일 존재 여부와 별개로 참조 검사에서 읽을 확장자인지 확인한다.
        static bool IsReferenceDocumentPath(const std::filesystem::path& path);

        // 프로젝트 에셋 루트에서 저장된 문서의 직접 참조와 문제를 수집한다.
        static AssetReferenceScan ScanProject(AssetReferenceScanTiming* timing = nullptr,
            AssetReferenceScanIndex* index = nullptr);
        // Return false without modifying the scan or delta when an event cannot be matched to an existing source.
        static bool RefreshProjectDocuments(const AssetReferenceScanIndex& index,
            std::span<const std::filesystem::path> paths, AssetReferenceScan& scan,
            AssetReferenceScanDelta& delta, AssetReferenceScanTiming* timing = nullptr);
        static std::vector<AssetReferenceIssue> ValidateAllResources();
        static AssetReferenceImpact AnalyzeDeletion(const std::vector<std::filesystem::path>& targets);
        static AssetReferenceImpact AnalyzeMove(const std::vector<std::filesystem::path>& targets);

        // Prepare without writing. Reject incomplete scans and edits outside the project asset root.
        static bool PrepareMoveReferences(const std::vector<AssetPathMove>& moves, std::vector<AssetReferenceEdit>& edits,
            AssetReferenceImpact* impact = nullptr);
        static bool ValidateMoveReferences(const std::vector<AssetReferenceEdit>& edits, bool forward);
        // Call after the corresponding file moves. Hold write locks and restore all touched documents on failure.
        static EAssetReferenceWriteResult ApplyMoveReferences(const std::vector<AssetReferenceEdit>& edits, bool forward,
            const std::filesystem::path& recoveryDirectory);

        // 손상/누락 경로에 대한 안전한 대체 복구 경로 탐색
        static bool TryResolveFallbackPath(const std::string& brokenPath, std::filesystem::path& outResolvedPath);

        // 검사 결과와 현재 참조가 일치할 때만 파일을 복구한다.
        static bool FixIssue(const AssetReferenceIssue& issue);

        // 쓰기 성공이 확인된 리소스 핸들 수를 반환한다.
        static size_t FixAllSafeIssues(const std::vector<AssetReferenceIssue>& issues);

    private:
        static bool ApplyReferenceRepairs(const std::vector<AssetReferenceEdit>& edits);
    };
}
