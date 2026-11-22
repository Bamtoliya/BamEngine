#pragma once

#include "RHIPipelineTypes.h"
#include "Types.h"

namespace Engine
{
	// 축을 홀수 번 뒤집는 행렬인지. 삼각형의 감긴 방향이 반대로 보인다.
	[[nodiscard]] inline bool IsMirroredTransform(const mat4& matrix)
	{
		return glm::determinant(glm::mat3(matrix)) < 0.0f;
	}

	// 재질의 앞면 방향을 행렬이 뒤집은 만큼 바로잡는다.
	[[nodiscard]] constexpr EFrontFace ResolveFrontFace(EFrontFace frontFace, bool mirrored)
	{
		if (!mirrored) return frontFace;
		return frontFace == EFrontFace::Clockwise ? EFrontFace::CounterClockwise : EFrontFace::Clockwise;
	}
}
