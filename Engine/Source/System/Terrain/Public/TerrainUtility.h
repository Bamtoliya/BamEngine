#pragma once

#include "Engine_API.h"
#include "Types.h"
#include "Mesh.h"

BEGIN(Engine)

class ENGINE_API TerrainUtility
{
public:
	// 지정한 가로/세로 크기와 분할 해상도로 XZ 평면 그리드 Mesh를 생성하여 반환합니다.
	static Mesh* CreateGridMesh(float width, float length, uint32 resolution);
};

END