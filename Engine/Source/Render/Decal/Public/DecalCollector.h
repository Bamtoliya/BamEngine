#pragma once

#include "DecalProjection.h"
#include "Engine_API.h"

namespace Engine
{
	class Scene;

	// 활성·표시 상태인 DecalComponent를 그리기 정보로 바꿔 output 뒤에 붙인다. 부피가 없는 상자는 건너뛴다.
	ENGINE_API void CollectDecalComponents(const Scene& scene, std::vector<DecalDrawData>& output);
}
