#pragma once

#include "Types.h"
#include <algorithm>
#include <vector>

namespace Engine
{
	// 후처리 패스 우선순위. 디퍼드·TAA 패스(100번대 초반) 뒤, UI(200) 앞이다.
	// 슬롯마다 보조 패스 자리(짝수)와 슬롯 패스 자리(홀수)를 둬서, 체인 안의 위치와 상관없이
	// 효과의 보조 패스가 앞 슬롯의 결과를 읽은 뒤 자기 슬롯 패스보다 먼저 실행되게 한다.
	inline constexpr uint32 PostProcessBasePriority = 130u;
	inline constexpr uint32 PostProcessPrePassPriority(uint32 slot) { return PostProcessBasePriority + slot * 2u; }
	inline constexpr uint32 PostProcessSlotPriority(uint32 slot) { return PostProcessBasePriority + slot * 2u + 1u; }

	// 파일에서 읽은 순서(loaded)에 없는 항목을 현재 순서(current)에서 바로 뒤따르는 항목 앞에 끼운다.
	// 뒤따르는 항목이 하나도 없으면 맨 뒤에 둔다. 새로 추가한 효과가 예전 프리셋에서도 기본 위치에 놓인다.
	template <typename T>
	std::vector<T> MergeEffectOrder(const std::vector<T>& current, std::vector<T> loaded)
	{
		for (std::size_t i = 0; i < current.size(); ++i)
		{
			const T& item = current[i];
			if (!item || std::find(loaded.begin(), loaded.end(), item) != loaded.end())
				continue;

			auto position = loaded.end();
			for (std::size_t next = i + 1; next < current.size(); ++next)
			{
				const auto found = std::find(loaded.begin(), loaded.end(), current[next]);
				if (found != loaded.end())
				{
					position = found;
					break;
				}
			}
			loaded.insert(position, item);
		}
		return loaded;
	}
}
