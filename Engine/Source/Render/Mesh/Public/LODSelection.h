#pragma once

#include <algorithm>
#include <cstdint>
#include <vector>

namespace Engine
{
	// 투영된 반지름이 화면 절반 높이에서 차지하는 비율(0~1).
	inline float ComputeScreenCoverage(float worldRadius, float projectionScaleY, float distance)
	{
		return std::clamp((std::max)(worldRadius, 0.01f) * projectionScaleY / (std::max)(distance, 0.001f), 0.0f, 1.0f);
	}

	struct LODThresholds
	{
		float LOD1Coverage = 0.25f;
		float LOD2Coverage = 0.08f;
		float HysteresisMargin = 0.02f;
		bool HasLOD1 = false;
		bool HasLOD2 = false;
	};

	// currentLOD는 같은 뷰가 직전에 고른 LOD여야 한다. 다른 뷰의 값을 넘기면 경계에서 전환이 반복된다.
	inline std::int32_t SelectLODIndex(float coverage, std::int32_t currentLOD, const LODThresholds& thresholds)
	{
		const float lod2 = thresholds.LOD2Coverage + (currentLOD == 2 ? thresholds.HysteresisMargin : 0.0f);
		const float lod1 = thresholds.LOD1Coverage + (currentLOD == 1 ? thresholds.HysteresisMargin : 0.0f);
		if (thresholds.HasLOD2 && coverage <= lod2) return 2;
		if (thresholds.HasLOD1 && coverage <= lod1) return 1;
		return 0;
	}

	// residentMask의 비트 n은 LOD n의 내용이 있다는 뜻이다. 고른 LOD가 없으면 더 단순한 LOD, 그다음 더 자세한 LOD를 쓴다.
	// 표시할 LOD가 없으면 -1.
	inline std::int32_t ResolveResidentLOD(std::int32_t lod, std::uint32_t residentMask)
	{
		constexpr std::int32_t LODCount = 3;
		lod = std::clamp(lod, 0, LODCount - 1);
		for (std::int32_t i = lod; i < LODCount; ++i)
			if (residentMask & (1u << i)) return i;
		for (std::int32_t i = lod - 1; i >= 0; --i)
			if (residentMask & (1u << i)) return i;
		return -1;
	}

	// 뷰(Geometry 패스 ID)별로 마지막에 고른 LOD. 저장하지 않는 런타임 상태다.
	class ViewLODState
	{
	public:
		std::int32_t Get(std::uint32_t viewID) const
		{
			for (const auto& entry : m_Entries)
				if (entry.ViewID == viewID) return entry.LOD;
			return 0;
		}

		void Set(std::uint32_t viewID, std::int32_t lod)
		{
			for (auto& entry : m_Entries)
			{
				if (entry.ViewID != viewID) continue;
				entry.LOD = lod;
				return;
			}
			// 닫힌 뷰의 항목이 쌓이지 않도록 가장 오래된 것부터 버린다.
			if (m_Entries.size() >= MaxViews)
				m_Entries.erase(m_Entries.begin());
			m_Entries.push_back({ viewID, lod });
		}

	private:
		struct Entry
		{
			std::uint32_t ViewID = 0;
			std::int32_t LOD = 0;
		};
		static constexpr std::size_t MaxViews = 8;
		std::vector<Entry> m_Entries;
	};
}
