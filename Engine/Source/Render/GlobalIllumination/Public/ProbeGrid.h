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

	// GPU 메모리 계산값(바이트). 조도 RGBA16F, 거리 RG16F(평균·제곱 평균), 광선 결과 RGBA16F(밝기·거리), 프로브 상태 RGBA16F(이동·상태).
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
		estimate.Distance = static_cast<uint64>(distance.Width) * distance.Height * 4u;
		estimate.RayData = static_cast<uint64>(raysPerProbe) * updateBudget * 8u;
		estimate.ProbeData = static_cast<uint64>(counts.Total()) * 8u;
		return estimate;
	}
}
