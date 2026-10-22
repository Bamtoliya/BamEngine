#include "TerrainUtility.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <vector>

namespace
{
	int Fail(const char* message) { std::cerr << message << '\n'; return 1; }
	bool Near(float left, float right) { return std::abs(left - right) < 0.00001f; }
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

	std::cout << "Terrain height resampling checks passed.\n";
	return 0;
}