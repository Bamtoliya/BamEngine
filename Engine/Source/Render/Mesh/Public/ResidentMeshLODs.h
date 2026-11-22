#pragma once

#include "LODGroupComponent.h"
#include "Mesh.h"

namespace Engine
{
	// 내용이 있어 바로 쓸 수 있는 LOD. 아직 읽는 중인 LOD는 기다리지 않고 건너뛴다.
	struct ResidentMeshLODs
	{
		Mesh* Meshes[3] = {};
		uint32 Mask = 0;

		ResidentMeshLODs(Mesh* baseMesh, const LODGroupComponent* lodGroup)
		{
			Meshes[0] = baseMesh;
			if (lodGroup)
			{
				Meshes[1] = lodGroup->lod1Mesh.Get();
				Meshes[2] = lodGroup->lod2Mesh.Get();
			}
			for (uint32 i = 0; i < 3; ++i)
				if (Meshes[i] && Meshes[i]->IsResident()) Mask |= 1u << i;
		}

		[[nodiscard]] Mesh* Find(int32 lod) const
		{
			const int32 resolved = ResolveResidentLOD(lod, Mask);
			return resolved < 0 ? nullptr : Meshes[resolved];
		}
	};
}
