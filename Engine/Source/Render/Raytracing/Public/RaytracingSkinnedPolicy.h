#pragma once

#include <algorithm>
#include <cstdint>
#include <vector>

namespace Engine
{
	// 스킨드 메시 BLAS를 이번 프레임에 어떻게 맞출지.
	enum class ESkinnedBottomLevelAction : std::uint8_t
	{
		Build,    // BLAS가 없거나 기하(메시·정점 수)가 바뀌어 새로 만든다. 빌드 예산이 없으면 이번 프레임은 대기한다.
		Refit,    // 같은 기하의 새 정점으로 제자리 갱신한다.
		Rebuild,  // refit이 오래 이어져 BVH 품질을 되돌리려 새로 만든다. 빌드 예산이 없으면 refit으로 대신한다.
	};

	// rebuildInterval이 0이면 주기적 재빌드를 하지 않는다.
	constexpr ESkinnedBottomLevelAction ChooseSkinnedBottomLevelAction(bool hasBottomLevel, bool sameGeometry,
		std::uint32_t refitCount, std::uint32_t rebuildInterval)
	{
		if (!hasBottomLevel || !sameGeometry)
			return ESkinnedBottomLevelAction::Build;
		if (rebuildInterval != 0 && refitCount >= rebuildInterval)
			return ESkinnedBottomLevelAction::Rebuild;
		return ESkinnedBottomLevelAction::Refit;
	}

	// 한 프레임에 레이트레이싱에 넣을 스킨드 메시 수의 상한. 같은 메시를 여러 뷰가 써도 한 번만 센다.
	class RaytracingSkinnedLimit
	{
	public:
		constexpr explicit RaytracingSkinnedLimit(std::uint32_t maxInstances) : m_MaxInstances(maxInstances) {}

		constexpr void Reset() { m_Count = 0; }

		// 자리가 있으면 차지하고 true를 반환한다.
		constexpr bool TryAcquire()
		{
			if (m_Count >= m_MaxInstances)
				return false;
			++m_Count;
			return true;
		}

		constexpr std::uint32_t GetCount() const { return m_Count; }

	private:
		std::uint32_t m_MaxInstances = 0;
		std::uint32_t m_Count = 0;
	};

	struct RaytracingSkinnedPriority
	{
		float DistanceSquared = 0.0f;
		std::uint32_t Order = 0;
	};

	// 카메라에 가까운 스킨드 메시부터 한도 안에 넣도록 정렬한다. 거리가 같으면 원래 순서를 유지한다.
	inline void SortSkinnedByDistance(std::vector<RaytracingSkinnedPriority>& items)
	{
		std::stable_sort(items.begin(), items.end(), [](const RaytracingSkinnedPriority& a, const RaytracingSkinnedPriority& b)
			{
				if (a.DistanceSquared != b.DistanceSquared)
					return a.DistanceSquared < b.DistanceSquared;
				return a.Order < b.Order;
			});
	}
}
