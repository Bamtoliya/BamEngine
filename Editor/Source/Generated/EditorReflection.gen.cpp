// ==================================================
// 자동 생성된 리플렉션 코드 (수정하지 마세요!)
// ==================================================

#include "Engine_Includes.h"
#include <entt/entt.hpp>
#include "AssetManager/Importer/Public/ModelImporter.h"
#include "ImGui/ViewportPanel/Public/ChannelFilter.h"

namespace Engine {

void RegisterReflection_EnTT() 
{
    using namespace entt::literals;

    BEGIN_ENTT_REFLECT_ENUM(EModelPostProcessFlag)
        ENTT_ENUM_ENTRY(EModelPostProcessFlag, None)
        ENTT_ENUM_ENTRY(EModelPostProcessFlag, CalcTangentSpace)
        ENTT_ENUM_ENTRY(EModelPostProcessFlag, JoinIdenticalVertices)
        ENTT_ENUM_ENTRY(EModelPostProcessFlag, MakeLeftHanded)
        ENTT_ENUM_ENTRY(EModelPostProcessFlag, Triangulate)
        ENTT_ENUM_ENTRY(EModelPostProcessFlag, RemoveComponent)
        ENTT_ENUM_ENTRY(EModelPostProcessFlag, GenNormals)
        ENTT_ENUM_ENTRY(EModelPostProcessFlag, GenSmoothNormals)
        ENTT_ENUM_ENTRY(EModelPostProcessFlag, SplitLargeMeshes)
        ENTT_ENUM_ENTRY(EModelPostProcessFlag, PreTransformVertices)
        ENTT_ENUM_ENTRY(EModelPostProcessFlag, LimitBoneWeights)
        ENTT_ENUM_ENTRY(EModelPostProcessFlag, ValidateDataStructure)
        ENTT_ENUM_ENTRY(EModelPostProcessFlag, ImproveCacheLocality)
        ENTT_ENUM_ENTRY(EModelPostProcessFlag, RemoveRedundantMaterials)
        ENTT_ENUM_ENTRY(EModelPostProcessFlag, FixInfacingNormals)
        ENTT_ENUM_ENTRY(EModelPostProcessFlag, SortByPType)
        ENTT_ENUM_ENTRY(EModelPostProcessFlag, FindDegenerates)
        ENTT_ENUM_ENTRY(EModelPostProcessFlag, FindInvalidData)
        ENTT_ENUM_ENTRY(EModelPostProcessFlag, GenUVCoords)
        ENTT_ENUM_ENTRY(EModelPostProcessFlag, TransformUVCoords)
        ENTT_ENUM_ENTRY(EModelPostProcessFlag, FindInstances)
        ENTT_ENUM_ENTRY(EModelPostProcessFlag, OptimizeMeshes)
        ENTT_ENUM_ENTRY(EModelPostProcessFlag, OptimizeGraph)
        ENTT_ENUM_ENTRY(EModelPostProcessFlag, FlipUVs)
        ENTT_ENUM_ENTRY(EModelPostProcessFlag, FlipWindingOrder)
    END_ENTT_REFLECT_ENUM()

    BEGIN_ENTT_REFLECT_ENUM(EModelImportOption)
        ENTT_ENUM_ENTRY(EModelImportOption, None)
        ENTT_ENUM_ENTRY(EModelImportOption, Animations)
        ENTT_ENUM_ENTRY(EModelImportOption, Materials)
        ENTT_ENUM_ENTRY(EModelImportOption, Meshes)
        ENTT_ENUM_ENTRY(EModelImportOption, Skeleton)
    END_ENTT_REFLECT_ENUM()

    BEGIN_ENTT_REFLECT_ENUM(EViewportChannelView)
        ENTT_ENUM_ENTRY(EViewportChannelView, None)
        ENTT_ENUM_ENTRY(EViewportChannelView, R)
        ENTT_ENUM_ENTRY(EViewportChannelView, G)
        ENTT_ENUM_ENTRY(EViewportChannelView, B)
        ENTT_ENUM_ENTRY(EViewportChannelView, A)
        ENTT_ENUM_ENTRY(EViewportChannelView, RGB)
        ENTT_ENUM_ENTRY(EViewportChannelView, RGBA)
    END_ENTT_REFLECT_ENUM()

}

} // namespace Engine

