#pragma once

#include "ImporterInterface.h"

#undef DBG_NEW
#undef new
#include "assimp/Importer.hpp"
#include "assimp/scene.h"
#include "assimp/postprocess.h"

#ifndef DBG_NEW 
#define DBG_NEW new ( _NORMAL_BLOCK , __FILE__ , __LINE__ ) 
#define new DBG_NEW 
#endif

ENUM()
enum class EModelPostProcessFlag : uint32
{
	None = 0,
	CalcTangentSpace = aiProcess_CalcTangentSpace,
	JoinIdenticalVertices = aiProcess_JoinIdenticalVertices,
	MakeLeftHanded = aiProcess_MakeLeftHanded,
	Triangulate = aiProcess_Triangulate,
	RemoveComponent = aiProcess_RemoveComponent,
	GenNormals = aiProcess_GenNormals,
	GenSmoothNormals = aiProcess_GenSmoothNormals,
	SplitLargeMeshes = aiProcess_SplitLargeMeshes,
	PreTransformVertices = aiProcess_PreTransformVertices,
	LimitBoneWeights = aiProcess_LimitBoneWeights,
	ValidateDataStructure = aiProcess_ValidateDataStructure,
	ImproveCacheLocality = aiProcess_ImproveCacheLocality,
	RemoveRedundantMaterials = aiProcess_RemoveRedundantMaterials,
	FixInfacingNormals = aiProcess_FixInfacingNormals,
	SortByPType = aiProcess_SortByPType,
	FindDegenerates = aiProcess_FindDegenerates,
	FindInvalidData = aiProcess_FindInvalidData,
	GenUVCoords = aiProcess_GenUVCoords,
	TransformUVCoords = aiProcess_TransformUVCoords,
	FindInstances = aiProcess_FindInstances,
	OptimizeMeshes = aiProcess_OptimizeMeshes,
	OptimizeGraph = aiProcess_OptimizeGraph,
	FlipUVs = aiProcess_FlipUVs,
	FlipWindingOrder = aiProcess_FlipWindingOrder,
};

ENABLE_BITMASK_OPERATORS(EModelPostProcessFlag);

ENUM()
enum class EModelImportOption : uint32
{
	None = 0,
	Animations = 1 << 0,
	Materials = 1 << 1,
	Meshes = 1 << 2,
	Skeleton = 1 << 3,
};

ENABLE_BITMASK_OPERATORS(EModelImportOption);

struct ModelImportDesc
{
	EModelPostProcessFlag PostProcessFlags = EModelPostProcessFlag::Triangulate | EModelPostProcessFlag::FlipUVs | EModelPostProcessFlag::MakeLeftHanded | EModelPostProcessFlag::FlipWindingOrder;
	EModelImportOption ImportOptions = EModelImportOption::Meshes | EModelImportOption::Skeleton | EModelImportOption::Animations | EModelImportOption::Materials;
	float UniformScale = 1.0f;
	std::array<float, 3> RotationOffset{ 0.0f, 0.0f, 0.0f };
	bool SaveToFile = true;
	bool Overwrite = true;
};

BEGIN(Editor)
class ModelImportTask;
class ModelImporter : public IImporter
{
#pragma region Contructor&Destructor
	using DESC = ModelImportDesc;
private:
	ModelImporter() = default;
	virtual ~ModelImporter() = default;
	virtual EResult Initialize(void* arg = nullptr) override { return EResult::Success; }
public:
	static ModelImporter* Create() { return new ModelImporter(); }
	virtual void Free() override { IImporter::Free(); }
#pragma endregion
public:
	EResult Import(const filesystem::path& sourcePath, const filesystem::path& destDir = {}, void* arg = nullptr) override;
	EResult CreateJob(const filesystem::path& sourcePath, const filesystem::path& destDir, void* arg, std::unique_ptr<ImportJob>& outJob) override;
	EResult Finalize(ModelImportTask& task);
	// nullopt: 진행 중, 값 있음: 전체 작업 종료
	std::optional<EResult> FinalizeStep(ModelImportTask& task);
	static ModelImportDesc GetDefaultDesc(const std::filesystem::path& sourcePath);
private:
	std::optional<EResult> FinalizeOneStep(ModelImportTask& task);
};
END