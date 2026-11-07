#pragma once

#include "Engine_Includes.h"
#include "Skeleton.h"
#include "ReflectionMacro.h"
#include <string>
#include <vector>
#include <unordered_set>

BEGIN(Engine)

STRUCT()
struct ENGINE_API BoneMask
{
	REFLECT_BODY()

		PROPERTY(EDITABLE, ALLOW_MISSING)
		std::wstring rootBoneName = L""; // 마스크의 루트 뼈 이름 (예: L"Spine")

	PROPERTY(EDITABLE, ALLOW_MISSING)
		bool includeChildren = true; // 하위 자식 뼈를 재귀적으로 포함할지 여부

	// 스켈레톤의 본 목록 크기에 맞춘 뼈별 가중치 배열 (0.0f = 베이스, 1.0f = 레이어 오버라이드)
	std::vector<float> boneWeights;

	// 스켈레톤 구조를 순회하여 boneWeights 배열을 생성합니다.
	void BuildWeights(const Skeleton& skeleton)
	{
		const auto& bones = skeleton.GetBones();
		const size_t boneCount = bones.size();
		boneWeights.assign(boneCount, 0.0f);
		if (rootBoneName.empty() || boneCount == 0) return;

		int32 rootIndex = -1;
		for (size_t i = 0; i < boneCount; ++i)
		{
			if (bones[i].Name == rootBoneName)
			{
				rootIndex = static_cast<int32>(i);
				break;
			}
		}
		if (rootIndex < 0) return;

		std::unordered_set<int32> maskedIndices;
		maskedIndices.insert(rootIndex);
		boneWeights[rootIndex] = 1.0f;

		if (includeChildren)
		{
			// Skeleton의 Bones는 부모가 항상 자식보다 앞서 정의되는 계층 순서입니다.
			for (size_t i = static_cast<size_t>(rootIndex + 1); i < boneCount; ++i)
			{
				if (bones[i].ParentIndex >= 0 && maskedIndices.contains(bones[i].ParentIndex))
				{
					maskedIndices.insert(static_cast<int32>(i));
					boneWeights[i] = 1.0f;
				}
			}
		}
	}
};

END