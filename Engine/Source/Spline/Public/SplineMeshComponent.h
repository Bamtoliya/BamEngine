#pragma once

#include "Engine_Includes.h"
#include "ComponentAnnotations.h"

namespace Engine
{
	struct SplineComponent;

	ENUM()
		enum class ESplineMeshBuildStatus : uint8
	{
		NotEvaluated,
		Ready,
		NoOutput,
		InvalidPath,
		MissingSourceMesh,
		MissingMaterial,
		PlacementFailed,
		DeformationFailed,
		SourceLimitExceeded,
		StartDistanceAtOrPastEnd,
		ZeroSourceZLength,
		FoldedSection
	};

	STRUCT(COMPONENT, DISPLAY_NAME("STRUCT_SPLINE_MESH"), CATEGORY("CATEGORY_SPLINE"),
		REQUIRE_COMPONENTS(SplineComponent, StaticMeshRendererComponent, TransformComponent, WorldTransformComponent))
		struct ENGINE_API SplineMeshComponent
	{
		REFLECT_BODY()

			PROPERTY(EDITABLE, DISPLAY_NAME("PROP_SPLINE_SPACING"), EDITCONDITION(!DeformToPath))
			f64 Spacing = 1.0;

		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_SPLINE_START_DISTANCE"))
			f64 StartDistance = 0.0;

		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_SPLINE_INITIAL_UP"))
			vec3 InitialUp{ 0.0f, 1.0f, 0.0f };

		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_SCALE"), EDITCONDITION(!DeformToPath))
			vec3 InstanceScale{ 1.0f };

		PROPERTY(READONLY, NOSERIALIZE, DISPLAY_NAME("PROP_SPLINE_INSTANCE_COUNT"))
			uint32 GeneratedCount = 0;

		PROPERTY(READONLY, NOSERIALIZE, VISIBLE(false), DISPLAY_NAME("PROP_SPLINE_BUILD_ERROR"))
			bool HasBuildError = false;

		PROPERTY(READONLY, NOSERIALIZE, DISPLAY_NAME("PROP_SPLINE_LAST_BUILD_STATUS"))
			ESplineMeshBuildStatus LastBuildStatus = ESplineMeshBuildStatus::NotEvaluated;

		PROPERTY(EDITABLE, ALLOW_MISSING, DISPLAY_NAME("PROP_SPLINE_DEFORM_TO_PATH"))
			bool DeformToPath = false;

		PROPERTY(EDITABLE, ALLOW_MISSING, DISPLAY_NAME("PROP_SPLINE_UV_REPEAT_DISTANCE"), EDITCONDITION(DeformToPath))
		f64 UVRepeatDistance = 0.0;

		// 렌더링과 Ray 피킹이 같은 배치 규칙을 사용합니다.
		// 실패하면 outMatrices를 보존합니다.
		bool BuildMatrices(const SplineComponent& spline, const mat4& worldMatrix,
			std::vector<mat4>& outMatrices) const;
	};
}