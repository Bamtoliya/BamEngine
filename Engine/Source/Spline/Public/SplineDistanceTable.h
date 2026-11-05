#pragma once

#include "Engine_Includes.h"

namespace Engine
{
	class ENGINE_API SplineDistanceTable final
	{
	public:
		static constexpr size_t MaxSubdivisionCount = 4096;

		// 실패 시 기존 거리 표를 보존합니다.
		bool Build(std::span<const vec3> points, bool closed, const mat4& localToWorld,
			size_t subdivisionsPerSegment = 32);

		// 유한한 거리의 범위 초과는 양 끝으로 제한합니다.
		// 길이가 0이면 parameter=0을 반환합니다.
		// 실패 시 outParameter를 변경하지 않습니다.
		bool TryGetParameter(f64 distance, f64& outParameter) const;

		f64 GetLength() const { return m_Entries.empty() ? 0.0 : m_Entries.back().Distance; }

	private:
		struct Entry
		{
			f64 Parameter = 0.0;
			f64 Distance = 0.0;
		};

		std::vector<Entry> m_Entries;
	};
}