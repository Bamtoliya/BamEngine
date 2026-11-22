#pragma once

#include <cstdint>
#include <vector>

namespace Engine
{
	// 한 프레임에 새로 빌드할 BLAS의 한도. 개수(버퍼 생성 비용)와 삼각형 수(GPU 빌드 비용) 중 먼저 닿는 쪽에서 멈춘다.
	class RaytracingBuildBudget
	{
	public:
		constexpr RaytracingBuildBudget(std::uint32_t maxBuilds, std::uint64_t maxTriangles)
			: m_MaxBuilds(maxBuilds), m_MaxTriangles(maxTriangles) {}

		constexpr void Reset()
		{
			m_Builds = 0;
			m_Triangles = 0;
		}

		// 빌드해도 되면 예산을 차감하고 true를 반환한다.
		// 프레임의 첫 빌드는 삼각형 한도를 넘어도 허용한다. 한도보다 큰 메시가 영원히 밀리지 않게 하기 위함이다.
		constexpr bool TryConsume(std::uint64_t triangles)
		{
			if (m_Builds >= m_MaxBuilds)
				return false;
			if (m_Builds > 0 && (m_Triangles >= m_MaxTriangles || triangles > m_MaxTriangles - m_Triangles))
				return false;
			++m_Builds;
			m_Triangles += triangles;
			return true;
		}

	private:
		std::uint32_t m_MaxBuilds = 0;
		std::uint64_t m_MaxTriangles = 0;
		std::uint32_t m_Builds = 0;
		std::uint64_t m_Triangles = 0;
	};

	// TLAS 한 번의 빌드에 실제로 들어간 엔티티 집합. 엔티티 인덱스별 세대 번호만 바꿔 매 프레임 다시 채운다.
	class RaytracedCasterSet
	{
	public:
		void Begin() { ++m_Generation; }

		void Add(std::uint32_t entityIndex)
		{
			if (entityIndex >= m_Generations.size())
				m_Generations.resize(static_cast<std::size_t>(entityIndex) + 1, 0);
			m_Generations[entityIndex] = m_Generation;
		}

		bool Contains(std::uint32_t entityIndex) const
		{
			return entityIndex < m_Generations.size() && m_Generation != 0 && m_Generations[entityIndex] == m_Generation;
		}

	private:
		std::vector<std::uint64_t> m_Generations;
		std::uint64_t m_Generation = 0;
	};
}
