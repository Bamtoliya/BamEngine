#pragma once

#include "Types.h"

#include <algorithm>
#include <array>
#include <cmath>

// 프로브 GI(DDGI 방식)의 격자·아틀라스 계산. GPU 셰이더(probeGI.hlsli, 2단계)와 같은 규칙을 쓴다.
// - 프로브는 볼륨 상자의 모서리부터 반대쪽 모서리까지 축마다 Counts개를 같은 간격으로 둔다.
// - 프로브 번호는 x + y * Counts.x + z * Counts.x * Counts.y 이다.
// - 아틀라스에서 프로브 (x, y, z)의 칸은 열 x + y * Counts.x, 행 z 이다.
// - 칸마다 옥타헤드럴 텍셀 N×N 둘레에 1텍셀 테두리를 두어 쌍선형 샘플이 이웃 칸으로 새지 않게 한다.
namespace Engine::ProbeGrid
{
	// 축마다 최소 2개(보간에 필요), 아틀라스 너비와 갱신 비용을 위해 x·z 32개, y 16개까지 둔다.
	constexpr uint32 MinCountPerAxis = 2u;
	constexpr std::array<uint32, 3> MaxCountPerAxis = { 32u, 16u, 32u };
	// 조도는 8×8, 거리(가시성)는 16×16 텍셀을 쓴다(DDGI 기본값).
	constexpr uint32 IrradianceTexels = 8u;
	constexpr uint32 DistanceTexels = 16u;
	// 프로브마다 쏘는 광선 수의 범위. 측정 전 시작값이다.
	constexpr uint32 MinRaysPerProbe = 32u;
	constexpr uint32 MaxRaysPerProbe = 256u;
	// 볼륨의 최소 크기(m). 0으로 나누지 않게 한다.
	constexpr f32 MinVolumeSize = 0.1f;

	struct GridCounts
	{
		uint32 X = MinCountPerAxis;
		uint32 Y = MinCountPerAxis;
		uint32 Z = MinCountPerAxis;

		constexpr uint32 Total() const { return X * Y * Z; }
		constexpr bool operator==(const GridCounts&) const = default;
	};

	struct GridCoord
	{
		uint32 X = 0u;
		uint32 Y = 0u;
		uint32 Z = 0u;

		constexpr bool operator==(const GridCoord&) const = default;
	};

	// 월드 축 정렬 상자. 회전은 쓰지 않는다.
	struct GridDesc
	{
		vec3 Origin{ 0.0f };   // 첫 프로브(0, 0, 0)의 위치 = 상자의 최소 모서리
		vec3 Spacing{ 1.0f };  // 축마다 프로브 사이 거리(m)
		GridCounts Counts{};
	};

	constexpr uint32 ClampCount(int32 value, uint32 axis)
	{
		const int32 maxCount = static_cast<int32>(MaxCountPerAxis[axis]);
		return static_cast<uint32>(std::clamp(value, static_cast<int32>(MinCountPerAxis), maxCount));
	}

	constexpr GridCounts ClampCounts(int32 x, int32 y, int32 z)
	{
		return { ClampCount(x, 0u), ClampCount(y, 1u), ClampCount(z, 2u) };
	}

	constexpr uint32 ClampRaysPerProbe(int32 rays)
	{
		return static_cast<uint32>(std::clamp(rays, static_cast<int32>(MinRaysPerProbe), static_cast<int32>(MaxRaysPerProbe)));
	}

	// 중심과 전체 크기로 격자를 만든다. 크기와 개수는 허용 범위로 자른다.
	inline GridDesc MakeGrid(const vec3& center, const vec3& size, int32 countX, int32 countY, int32 countZ)
	{
		GridDesc grid;
		grid.Counts = ClampCounts(countX, countY, countZ);
		const vec3 clampedSize = glm::max(glm::abs(size), vec3(MinVolumeSize));
		grid.Origin = center - clampedSize * 0.5f;
		grid.Spacing = clampedSize / vec3(static_cast<f32>(grid.Counts.X - 1u), static_cast<f32>(grid.Counts.Y - 1u), static_cast<f32>(grid.Counts.Z - 1u));
		return grid;
	}

	constexpr uint32 ToIndex(const GridCounts& counts, const GridCoord& coord)
	{
		return coord.X + coord.Y * counts.X + coord.Z * counts.X * counts.Y;
	}

	constexpr GridCoord ToCoord(const GridCounts& counts, uint32 index)
	{
		const uint32 slice = counts.X * counts.Y;
		return { index % counts.X, (index % slice) / counts.X, index / slice };
	}

	inline vec3 ProbePosition(const GridDesc& grid, const GridCoord& coord)
	{
		return grid.Origin + grid.Spacing * vec3(static_cast<f32>(coord.X), static_cast<f32>(coord.Y), static_cast<f32>(coord.Z));
	}

	// 위치를 둘러싼 8개 프로브의 기준(최소) 좌표와 그 안의 비율(0~1). 상자 밖은 가장 가까운 면으로 자른다.
	struct CellLocation
	{
		GridCoord Base{};
		vec3 Alpha{ 0.0f };
	};

	inline CellLocation LocateCell(const GridDesc& grid, const vec3& position)
	{
		const vec3 local = (position - grid.Origin) / grid.Spacing;
		const vec3 maxBase(static_cast<f32>(grid.Counts.X - 2u), static_cast<f32>(grid.Counts.Y - 2u), static_cast<f32>(grid.Counts.Z - 2u));
		const vec3 base = glm::clamp(glm::floor(local), vec3(0.0f), maxBase);
		CellLocation cell;
		cell.Base = { static_cast<uint32>(base.x), static_cast<uint32>(base.y), static_cast<uint32>(base.z) };
		cell.Alpha = glm::clamp(local - base, vec3(0.0f), vec3(1.0f));
		return cell;
	}

	// 8개 이웃 중 corner(비트 0 = x, 1 = y, 2 = z)의 좌표 오프셋
	constexpr GridCoord CornerOffset(uint32 corner)
	{
		return { corner & 1u, (corner >> 1u) & 1u, (corner >> 2u) & 1u };
	}

	// corner 프로브의 3선형 가중치. 8개의 합은 1이다.
	inline f32 TrilinearWeight(const vec3& alpha, uint32 corner)
	{
		const GridCoord offset = CornerOffset(corner);
		const vec3 select(static_cast<f32>(offset.X), static_cast<f32>(offset.Y), static_cast<f32>(offset.Z));
		const vec3 weights = glm::mix(vec3(1.0f) - alpha, alpha, select);
		return weights.x * weights.y * weights.z;
	}

	// 상자 경계에서 기존 환경광으로 넘어가는 비율. 안쪽은 1, 경계에서 blendDistance(m)만큼 바깥은 0이다.
	inline f32 VolumeBlendWeight(const GridDesc& grid, const vec3& position, f32 blendDistance)
	{
		const vec3 extent = grid.Spacing * vec3(static_cast<f32>(grid.Counts.X - 1u), static_cast<f32>(grid.Counts.Y - 1u), static_cast<f32>(grid.Counts.Z - 1u));
		const vec3 outside = glm::max(grid.Origin - position, position - (grid.Origin + extent));
		const f32 farthest = std::max(outside.x, std::max(outside.y, outside.z));
		if (farthest <= 0.0f)
			return 1.0f;
		if (blendDistance <= 0.0f)
			return 0.0f;
		return std::clamp(1.0f - farthest / blendDistance, 0.0f, 1.0f);
	}

	// 점에서 상자까지의 거리(m). 상자 안이면 0이다. 뷰마다 카메라를 담거나 가장 가까운 볼륨을 고를 때 쓴다.
	inline f32 DistanceToVolume(const GridDesc& grid, const vec3& point)
	{
		const vec3 extent = grid.Spacing * vec3(static_cast<f32>(grid.Counts.X - 1u), static_cast<f32>(grid.Counts.Y - 1u), static_cast<f32>(grid.Counts.Z - 1u));
		const vec3 outside = glm::max(glm::max(grid.Origin - point, point - (grid.Origin + extent)), vec3(0.0f));
		return glm::length(outside);
	}

	// ── 벽 속 프로브 옮기기와 끄기 (DDGI의 Relocation·Classification, 4단계) ──
	// 셰이더(probeGIRelocateRTCS)와 같은 규칙이다. 한쪽을 바꾸면 다른 쪽과 ProbeGridCheck도 맞춘다.
	// 뒷면에 닿은 광선이 이 비율보다 많으면 벽 속으로 본다.
	constexpr f32 BackfaceRatioThreshold = 0.25f;
	// 이미 꺼진 프로브는 뒷면 비율이 이 값 이하로 내려와야 다시 켠다(얇은 벽 경계 0↔1 깜빡임 방지).
	constexpr f32 BackfaceReactivateThreshold = 0.15f;
	// 앞면까지 이 거리(가장 좁은 간격에 대한 비율)보다 가까우면 면에서 떼어 낸다.
	constexpr f32 MinFrontfaceDistanceRatio = 0.2f;
	// 앞면에서 떼어 낼 때 한 번에 이동하는 비율(얇은 벽·코너 진동 완화).
	constexpr f32 FrontfacePushDamping = 0.5f;
	// 격자 위치에서 축마다 간격의 이 비율까지만 옮긴다. 이웃 셀로 넘어가면 보간이 깨진다.
	constexpr f32 MaxProbeOffsetRatio = 0.45f;

	// 한 프로브의 광선 결과 요약. 거리는 실제 거리(m)이며 닿은 것이 없으면 아주 큰 값이다.
	struct ProbeRayStats
	{
		uint32 RayCount = 0u;
		uint32 BackfaceCount = 0u;
		f32 ClosestBackfaceDistance = 1e27f;
		vec3 ClosestBackfaceDirection{ 0.0f };
		f32 ClosestFrontfaceDistance = 1e27f;
		vec3 ClosestFrontfaceDirection{ 0.0f };
	};

	struct ProbeState
	{
		vec3 Offset{ 0.0f };  // 격자 위치에서 옮긴 거리(m)
		bool Active = true;   // false면 조명과 다중 반사에서 쓰지 않는다
	};

	inline f32 MinSpacing(const vec3& spacing)
	{
		return std::min(spacing.x, std::min(spacing.y, spacing.z));
	}

	// 이번 광선 결과로 프로브를 옮기고 사용 여부를 정한다. 결과는 다음 갱신의 광선부터 반영된다.
	inline ProbeState RelocateProbe(const ProbeState& current, const ProbeRayStats& stats, const vec3& spacing)
	{
		ProbeState next = current;
		if (stats.RayCount == 0u)
			return next;
		const f32 minFrontface = MinSpacing(spacing) * MinFrontfaceDistanceRatio;
		const f32 backfaceRatio = static_cast<f32>(stats.BackfaceCount) / static_cast<f32>(stats.RayCount);
		const f32 threshold = current.Active ? BackfaceRatioThreshold : BackfaceReactivateThreshold;
		const bool inside = backfaceRatio > threshold;
		if (inside)
		{
			// 가장 가까운 뒷면을 지나 벽 밖으로 나간다.
			next.Offset += stats.ClosestBackfaceDirection * (stats.ClosestBackfaceDistance + minFrontface * 0.5f);
		}
		else if (stats.ClosestFrontfaceDistance < minFrontface)
		{
			// 면에 너무 붙어 있으면 반대쪽으로 감쇠 이동해 얇은 벽·코너 진동을 줄인다.
			next.Offset -= stats.ClosestFrontfaceDirection * ((minFrontface - stats.ClosestFrontfaceDistance) * FrontfacePushDamping);
		}
		const vec3 limit = spacing * MaxProbeOffsetRatio;
		next.Offset = glm::clamp(next.Offset, -limit, limit);
		// 옮겨도 벽 속이면(벽이 간격보다 두꺼움) 쓰지 않는다. 다음 갱신에서 다시 판단한다.
		next.Active = !inside;
		return next;
	}

	// 단위 방향을 [-1, 1]² 옥타헤드럴 좌표로 바꾼다(위쪽 반구가 안쪽 마름모).
	inline vec2 EncodeOctahedral(const vec3& direction)
	{
		const f32 sum = std::abs(direction.x) + std::abs(direction.y) + std::abs(direction.z);
		vec2 result = vec2(direction.x, direction.z) / std::max(sum, 1e-8f);
		if (direction.y < 0.0f)
		{
			const vec2 sign(result.x >= 0.0f ? 1.0f : -1.0f, result.y >= 0.0f ? 1.0f : -1.0f);
			result = (vec2(1.0f) - glm::abs(vec2(result.y, result.x))) * sign;
		}
		return result;
	}

	inline vec3 DecodeOctahedral(const vec2& octahedral)
	{
		vec3 direction(octahedral.x, 1.0f - std::abs(octahedral.x) - std::abs(octahedral.y), octahedral.y);
		if (direction.y < 0.0f)
		{
			const vec2 sign(direction.x >= 0.0f ? 1.0f : -1.0f, direction.z >= 0.0f ? 1.0f : -1.0f);
			const vec2 folded = (vec2(1.0f) - glm::abs(vec2(direction.z, direction.x))) * sign;
			direction.x = folded.x;
			direction.z = folded.y;
		}
		return glm::normalize(direction);
	}

	// 아틀라스 크기(텍셀). 칸 하나는 texels + 2(테두리)이다.
	struct AtlasSize
	{
		uint32 Width = 0u;
		uint32 Height = 0u;
	};

	constexpr AtlasSize GetAtlasSize(const GridCounts& counts, uint32 texels)
	{
		const uint32 tile = texels + 2u;
		return { counts.X * counts.Y * tile, counts.Z * tile };
	}

	// 프로브 칸의 왼쪽 위 텍셀(테두리 포함)
	constexpr std::array<uint32, 2> TileOrigin(const GridCounts& counts, uint32 probeIndex, uint32 texels)
	{
		const GridCoord coord = ToCoord(counts, probeIndex);
		const uint32 tile = texels + 2u;
		return { (coord.X + coord.Y * counts.X) * tile, coord.Z * tile };
	}

	// 방향을 샘플할 아틀라스 UV(0~1). 칸 안쪽 N×N 영역에 대응한다.
	inline vec2 AtlasUV(const GridCounts& counts, uint32 probeIndex, uint32 texels, const vec3& direction)
	{
		const auto origin = TileOrigin(counts, probeIndex, texels);
		const AtlasSize size = GetAtlasSize(counts, texels);
		const vec2 inside = (EncodeOctahedral(direction) * 0.5f + vec2(0.5f)) * static_cast<f32>(texels);
		const vec2 texel = vec2(static_cast<f32>(origin[0]), static_cast<f32>(origin[1])) + vec2(1.0f) + inside;
		return texel / vec2(static_cast<f32>(size.Width), static_cast<f32>(size.Height));
	}

	// 칸 안쪽 텍셀(1~N)의 중심이 가리키는 방향
	inline vec3 TexelDirection(uint32 localX, uint32 localY, uint32 texels)
	{
		const vec2 uv = (vec2(static_cast<f32>(localX) - 0.5f, static_cast<f32>(localY) - 0.5f)) / static_cast<f32>(texels);
		return DecodeOctahedral(uv * 2.0f - vec2(1.0f));
	}

	constexpr bool IsBorderTexel(uint32 localX, uint32 localY, uint32 texels)
	{
		return localX == 0u || localY == 0u || localX == texels + 1u || localY == texels + 1u;
	}

	// 테두리 텍셀(칸 기준 0~N+1)이 복사해 올 안쪽 텍셀. 옥타헤드럴 경계는 반대쪽으로 접히므로 뒤집어 읽는다.
	constexpr std::array<uint32, 2> BorderSource(uint32 localX, uint32 localY, uint32 texels)
	{
		const uint32 last = texels + 1u;
		const bool left = localX == 0u, right = localX == last, top = localY == 0u, bottom = localY == last;
		if ((left || right) && (top || bottom))
			return { left ? texels : 1u, top ? texels : 1u };
		if (top || bottom)
			return { last - localX, top ? 1u : texels };
		if (left || right)
			return { left ? 1u : texels, last - localY };
		return { localX, localY };
	}

	// 프레임마다 budget개씩 돌아가며 갱신한다. i번째(0 ≤ i < budget) 갱신 대상 프로브 번호.
	constexpr uint32 ClampUpdateBudget(int32 budget, uint32 total)
	{
		return total == 0u ? 0u : static_cast<uint32>(std::clamp(budget, 1, static_cast<int32>(total)));
	}

	constexpr uint32 ScheduledProbe(uint64 frameIndex, uint32 i, uint32 budget, uint32 total)
	{
		const uint64 first = (frameIndex * budget) % total;
		return static_cast<uint32>((first + i) % total);
	}

	// 구 위에 고르게 퍼진 i번째 방향(구면 피보나치). 셰이더는 여기에 프레임마다 무작위 회전을 곱한다.
	inline vec3 SphericalFibonacci(uint32 i, uint32 count)
	{
		constexpr f32 goldenAngle = 2.39996322972865332f; // π(3 - √5)
		const f32 y = 1.0f - (2.0f * static_cast<f32>(i) + 1.0f) / static_cast<f32>(count);
		const f32 radius = std::sqrt(std::max(0.0f, 1.0f - y * y));
		const f32 phi = goldenAngle * static_cast<f32>(i);
		return vec3(std::cos(phi) * radius, y, std::sin(phi) * radius);
	}

	// 프레임마다 광선 묶음을 돌리는 무작위 회전(균일한 회전 분포, Shoemake 1992). 같은 프레임 번호는 같은 회전이다.
	inline mat3 RandomRayRotation(uint64 frameIndex)
	{
		uint64 state = frameIndex * 0x9E3779B97F4A7C15ull + 0x632BE59BD9B4E019ull;
		const auto next = [&state]()
			{
				// splitmix64의 상위 24비트로 [0, 1) 값을 만든다.
				state += 0x9E3779B97F4A7C15ull;
				uint64 z = state;
				z = (z ^ (z >> 30u)) * 0xBF58476D1CE4E5B9ull;
				z = (z ^ (z >> 27u)) * 0x94D049BB133111EBull;
				z ^= z >> 31u;
				return static_cast<f32>(z >> 40u) / static_cast<f32>(1u << 24u);
			};
		constexpr f32 twoPi = 6.28318530717958647f;
		const f32 u1 = next();
		const f32 u2 = next();
		const f32 u3 = next();
		const f32 a = std::sqrt(1.0f - u1);
		const f32 b = std::sqrt(u1);
		// glm::quat 생성자 순서는 (w, x, y, z)다.
		const quat rotation(b * std::cos(twoPi * u3), a * std::sin(twoPi * u2), a * std::cos(twoPi * u2), b * std::sin(twoPi * u3));
		return glm::mat3_cast(glm::normalize(rotation));
	}

	// 프로브를 돌아가며 갱신하는 순서와 이력 초기화를 관리한다(뷰마다 하나).
	// 격자·씬·갱신 수가 바뀌면 아틀라스를 비우고 처음부터 다시 돈다. 처음 한 바퀴의 프로브는 이전 값과 섞지 않는다.
	class ProbeUpdateSchedule
	{
	public:
		struct Frame
		{
			bool Clear = false;        // 이번 프레임에 아틀라스를 먼저 비운다
			uint32 FirstProbe = 0u;    // 이번 프레임 첫 갱신 프로브. i번째는 (FirstProbe + i) % 전체
			uint32 UpdateCount = 0u;   // 이번 프레임 갱신 수
			uint32 FreshCount = 0u;    // 앞에서부터 이 수만큼은 처음 갱신하는 프로브다
			uint64 Index = 0u;         // 초기화 뒤 몇 번째 프레임인지(광선 회전에 쓴다)
		};

		Frame Begin(const GridDesc& grid, const void* scene, int32 budget)
		{
			const uint32 total = grid.Counts.Total();
			const uint32 clampedBudget = ClampUpdateBudget(budget, total);
			if (scene != m_Scene || clampedBudget != m_Budget || grid.Counts != m_Grid.Counts ||
				grid.Origin != m_Grid.Origin || grid.Spacing != m_Grid.Spacing)
			{
				m_Valid = false;
				m_Scene = scene;
				m_Budget = clampedBudget;
				m_Grid = grid;
			}
			if (!m_Valid)
				m_Counter = 0u;

			Frame frame;
			frame.Clear = !m_Valid;
			frame.Index = m_Counter;
			frame.UpdateCount = clampedBudget;
			if (total == 0u)
				return frame;
			const uint64 visited = m_Counter * clampedBudget;
			frame.FirstProbe = static_cast<uint32>(visited % total);
			frame.FreshCount = visited < total ? static_cast<uint32>(std::min<uint64>(clampedBudget, total - visited)) : 0u;
			return frame;
		}

		// 이번 프레임의 Dispatch를 모두 기록한 뒤 부른다. 부르지 않으면 다음 프레임이 같은 프로브를 처음부터 다시 갱신한다.
		void Commit()
		{
			m_Valid = true;
			++m_Counter;
		}

		void Reset() { m_Valid = false; }

	private:
		GridDesc m_Grid{};
		const void* m_Scene = nullptr;
		uint32 m_Budget = 0u;
		uint64 m_Counter = 0u;
		bool m_Valid = false;
	};

	// GPU 메모리 계산값(바이트). 조도 RGBA16F, 거리 RGBA16F(평균·제곱 평균. 이전 값을 UAV로 읽어 섞으므로 읽기가 보장되는 형식),
	// 광선 결과 RGBA16F(밝기·거리), 프로브 상태 RGBA16F(4단계, 이동·상태).
	struct MemoryEstimate
	{
		uint64 Irradiance = 0u;
		uint64 Distance = 0u;
		uint64 RayData = 0u;
		uint64 ProbeData = 0u;

		constexpr uint64 Total() const { return Irradiance + Distance + RayData + ProbeData; }
	};

	constexpr MemoryEstimate EstimateMemory(const GridCounts& counts, uint32 raysPerProbe, uint32 updateBudget)
	{
		const AtlasSize irradiance = GetAtlasSize(counts, IrradianceTexels);
		const AtlasSize distance = GetAtlasSize(counts, DistanceTexels);
		MemoryEstimate estimate;
		estimate.Irradiance = static_cast<uint64>(irradiance.Width) * irradiance.Height * 8u;
		estimate.Distance = static_cast<uint64>(distance.Width) * distance.Height * 8u;
		estimate.RayData = static_cast<uint64>(raysPerProbe) * updateBudget * 8u;
		estimate.ProbeData = static_cast<uint64>(counts.Total()) * 8u;
		return estimate;
	}
}
