#pragma once

#include "Engine_Includes.h"
#include <filesystem>
#include <string>
#include <string_view>

BEGIN(Engine)

/**
 * @brief 하이브리드 모델 에셋 정리 파이프라인(Phase 6)의 경로 레이아웃 결과 구조체
 */
struct ENGINE_API ModelSubfolderLayout
{
	std::filesystem::path RootDirectory;       // 예: Assets/Models/Characters/Barbarian
	std::filesystem::path ModelFilePath;       // 예: Assets/Models/Characters/Barbarian/Barbarian.bammodel
	std::filesystem::path SkeletonFilePath;    // 예: Assets/Models/Characters/Barbarian/Barbarian_Skeleton.bamskel
	std::filesystem::path MeshesDirectory;     // 예: Assets/Models/Characters/Barbarian/Meshes (비활성 시 RootDirectory)
	std::filesystem::path MaterialsDirectory;  // 예: Assets/Models/Characters/Barbarian/Materials (비활성 시 RootDirectory)
	std::filesystem::path TexturesDirectory;   // 예: Assets/Models/Characters/Barbarian/Textures (비활성 시 RootDirectory)
	std::filesystem::path AnimationsDirectory; // 예: Assets/Models/Characters/Barbarian/Animations (비활성 시 RootDirectory)
	std::filesystem::path SidecarFilePath;     // 예: Assets/Models/Characters/Barbarian/Barbarian.bamimport.json

	std::wstring ModelStem;

	bool UsesDedicatedModelFolder = false;
	bool UsesMeshSubfolder = true;
	bool UsesMaterialSubfolder = true;
	bool UsesTextureSubfolder = true;
	bool UsesAnimationSubfolder = true;
};

/**
 * @brief Phase 6 하이브리드 모델 에셋 서브폴더 정리 및 이름 정제(Sanitization), 충돌 회피 헬퍼
 */
class ENGINE_API ModelSubfolderHelper
{
public:
	/**
	 * @brief 파일명/디렉터리명에 부적합한 특수문자 치환, 공백/마침표 트리밍, 윈도우 예약어 회피
	 */
	static std::wstring SanitizeName(std::wstring_view name, std::wstring_view fallback = L"Model");

	/**
	 * @brief 대상 디렉터리와 모델 이름을 바탕으로 Phase 6 하이브리드 서브폴더 레이아웃 구성
	 * @param destinationDirectory 콘텐츠 브라우저에서 선택된 기본 출력 폴더
	 * @param modelOrSourceStem 원본 모델 파일명 또는 모델 식별자 stem
	 * @param createDedicatedFolder 모델 전용 폴더(ModelName/) 생성 여부 (이미 해당 폴더 안이면 중복 생성 방지)
	 * @param useMeshSubfolder Meshes/ 하위 폴더 사용 여부
	 * @param useMaterialSubfolder Materials/ 하위 폴더 사용 여부
	 * @param useTextureSubfolder Textures/ 하위 폴더 사용 여부
	 * @param useAnimationSubfolder Animations/ 하위 폴더 사용 여부
	 */
	static ModelSubfolderLayout ResolveLayout(
		const std::filesystem::path& destinationDirectory,
		std::wstring_view modelOrSourceStem,
		bool createDedicatedFolder = true,
		bool useMeshSubfolder = true,
		bool useMaterialSubfolder = true,
		bool useTextureSubfolder = true,
		bool useAnimationSubfolder = true);

	/**
	 * @brief 이미 존재하는 디렉터리와 충돌 시 _1, _2 등을 붙여 고유 디렉터리 경로 반환
	 */
	static std::filesystem::path ResolveUniqueDirectory(const std::filesystem::path& dirPath);

	/**
	 * @brief 이미 존재하는 파일과 충돌 시 _1, _2 등을 붙여 고유 파일 경로 반환
	 */
	static std::filesystem::path ResolveUniqueFilePath(const std::filesystem::path& filePath);

	/**
	 * @brief 파생 에셋(메시, 머티리얼, 텍스처, 애니메이션) 경로 빌더
	 */
	static std::filesystem::path BuildMeshPath(const ModelSubfolderLayout& layout, std::wstring_view meshStem, uint32 lodLevel = 0);
	static std::filesystem::path BuildMaterialPath(const ModelSubfolderLayout& layout, std::wstring_view materialStem);
	static std::filesystem::path BuildTexturePath(const ModelSubfolderLayout& layout, std::wstring_view textureStem, std::wstring_view extension = L".png");
	static std::filesystem::path BuildAnimationPath(const ModelSubfolderLayout& layout, std::wstring_view animationStem);
};

END
