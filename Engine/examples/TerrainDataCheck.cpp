#include "TerrainUtility.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <vector>
#include "TerrainComponent.h"

namespace
{
	int Fail(const char* message) { std::cerr << message << '\n'; return 1; }
	bool Near(float left, float right) { return std::abs(left - right) < 0.00001f; }
	Engine::TerrainComponent MakeBrushTerrain()
	{
		Engine::TerrainComponent terrain;
		terrain.Width = 16.f;
		terrain.Length = 16.f;
		terrain.MaxHeight = 10.f;
		terrain.Resolution = 16;
		terrain.HeightData.assign(17 * 17, 0.25f);
		return terrain;
	}

	bool ChangedHeightsCovered(const std::vector<Engine::f32>& before, const Engine::TerrainComponent& terrain)
	{
		if (!terrain.IsDirty || before.size() != terrain.HeightData.size())
			return false;

		const auto axis = terrain.Resolution + 1;
		const auto& region = terrain.HeightDirtyRegion;

		if (region && (region->IsEmpty() || region->maxX > axis || region->maxY > axis))
			return false;

		bool changed = false;
		for (std::size_t index = 0; index < before.size(); ++index)
		{
			if (before[index] == terrain.HeightData[index]) continue;
			changed = true;

			const auto x = index % axis;
			const auto y = index / axis;

			if (region && (x < region->minX || x >= region->maxX ||
				y < region->minY || y >= region->maxY))
				return false;
		}

		return changed;
	}

	int CheckHeightDirtyRegions()
	{
		using namespace Engine;

		TerrainBrushParams brush;
		brush.Mode = ETerrainBrushMode::Raise;
		brush.Radius = 1.5f;
		brush.Strength = 2.f;
		const glm::mat4 world(1.f);

		// 한 번의 업로드 전에 떨어진 두 위치를 편집해도 변경이 모두 포함되어야 합니다.
		auto terrain = MakeBrushTerrain();
		const auto before = terrain.HeightData;

		if (!TerrainUtility::ApplyBrush(terrain, vec3(-5.f, 0.f, -5.f), world, brush, 0.25f) ||
			!TerrainUtility::ApplyBrush(terrain, vec3(3.f, 0.f, 4.f), world, brush, 0.25f) ||
			!terrain.HeightDirtyRegion || !ChangedHeightsCovered(before, terrain))
			return Fail("Merged brush region omitted changed heights.");

		// 전체 갱신 요청은 뒤이은 부분 편집으로 축소되면 안 됩니다.
		const auto beforeFull = terrain.HeightData;
		terrain.MarkHeightDirty();

		if (!TerrainUtility::ApplyBrush(terrain, vec3(0.f), world, brush, 0.25f) ||
			terrain.HeightDirtyRegion || !ChangedHeightsCovered(beforeFull, terrain))
			return Fail("Partial edit replaced a pending full upload.");

		// 마지막 행과 열을 포함한 네 모서리에서 최대 좌표 처리 오류를 검사합니다.
		const vec3 corners[] = {
			vec3(-8.f, 0.f, -8.f), vec3(8.f, 0.f, -8.f),
			vec3(-8.f, 0.f, 8.f), vec3(8.f, 0.f, 8.f)
		};

		for (const auto& corner : corners)
		{
			auto edgeTerrain = MakeBrushTerrain();
			const auto edgeBefore = edgeTerrain.HeightData;

			if (!TerrainUtility::ApplyBrush(edgeTerrain, corner, world, brush, 0.25f) ||
				!edgeTerrain.HeightDirtyRegion || !ChangedHeightsCovered(edgeBefore, edgeTerrain))
				return Fail("Corner brush region omitted changed heights.");
		}

		// 이미 최대 높이라 실제 변화가 없다면 업로드를 요청하지 않아야 합니다.
		auto saturated = MakeBrushTerrain();
		std::fill(saturated.HeightData.begin(), saturated.HeightData.end(), 1.f);

		if (TerrainUtility::ApplyBrush(saturated, vec3(0.f), world, brush, 0.25f) ||
			saturated.IsDirty || saturated.HeightDirtyRegion)
			return Fail("Unchanged terrain requested an upload.");

		return 0;
	}
}

int main()
{
	using namespace Engine;

	const std::vector<f32> slope{ 0.0f, 0.5f, 0.5f, 1.0f };
	std::vector<f32> result;
	if (!TerrainUtility::ResampleHeightData(slope, 1, 2, result) || result.size() != 9)
		return Fail("Heightmap enlargement failed.");

	for (uint32 z = 0; z < 3; ++z)
		for (uint32 x = 0; x < 3; ++x)
			if (!Near(result[z * 3 + x], static_cast<float>(x + z) * 0.25f))
				return Fail("Resampling changed a planar slope.");

	const auto enlarged = result;
	if (!TerrainUtility::ResampleHeightData(enlarged, 2, 1, result) || result != slope)
		return Fail("Heightmap reduction lost the corner heights.");

	const std::vector<f32> flat(4, 0.4f);
	if (!TerrainUtility::ResampleHeightData(flat, 1, 4, result) || result.size() != 25 ||
		std::any_of(result.begin(), result.end(), [](f32 value) { return !Near(value, 0.4f); }))
		return Fail("Resampling changed a constant height.");

	auto aliased = slope;
	if (!TerrainUtility::ResampleHeightData(aliased, 1, 2, aliased) || aliased != enlarged)
		return Fail("Aliased source and destination failed.");

	const std::vector<f32> sentinel{ -7.0f };
	result = sentinel;
	const auto rejected = [&](const std::vector<f32>& data, uint32 from, uint32 to)
		{
			return !TerrainUtility::ResampleHeightData(data, from, to, result) && result == sentinel;
		};

	const uint32 invalidResolution = (std::numeric_limits<uint32>::max)();
	if (!rejected({ 0.0f, 0.5f, 1.0f }, 1, 2) ||
		!rejected(slope, 0, 2) || !rejected(slope, 1, 0) ||
		!rejected(slope, invalidResolution, 2) || !rejected(slope, 1, invalidResolution))
		return Fail("Invalid dimensions were accepted or changed the destination.");

	auto invalid = slope;
	invalid[1] = std::numeric_limits<f32>::quiet_NaN();
	if (!rejected(invalid, 1, 2)) return Fail("NaN height was accepted.");
	invalid[1] = std::numeric_limits<f32>::infinity();
	if (!rejected(invalid, 1, 2)) return Fail("Infinite height was accepted.");

	if (const int checkResult = CheckHeightDirtyRegions(); checkResult != 0)
		return checkResult;

	std::cout << "Terrain data checks passed.\n";
	return 0;
}