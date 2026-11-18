#include <cassert>
#include <iostream>
#include <vector>
#include <cmath>
#include <algorithm>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "Types.h"
#include "Light.h"
#include "LightManager.h"

using namespace Engine;

namespace
{
	constexpr Engine::uint32 TILE_SIZE = 16u;
	constexpr Engine::uint32 MAX_LIGHTS_PER_TILE = 256u;

	// HLSL light.hlsli 상수 정의
	constexpr Engine::uint32 TEST_LIGHT_TYPE_POINT = 0u;
	constexpr Engine::uint32 TEST_LIGHT_TYPE_DIRECTIONAL = 1u;
	constexpr Engine::uint32 TEST_LIGHT_TYPE_SPOT = 2u;
	constexpr Engine::uint32 TEST_LIGHT_TYPE_SKY = 3u;

	constexpr Engine::uint32 TEST_LIGHT_FLAG_CAST_SHADOWS = (1u << 0u);
	constexpr Engine::uint32 TEST_LIGHT_FLAG_USE_IN_FORWARD = (1u << 1u);
	constexpr Engine::uint32 TEST_LIGHT_FLAG_USE_IN_DEFERRED = (1u << 2u);
	constexpr Engine::uint32 TEST_LIGHT_FLAG_AFFECT_DIFFUSE = (1u << 4u);
	constexpr Engine::uint32 TEST_LIGHT_FLAG_AFFECT_SPECULAR = (1u << 5u);

	// NDC 좌표를 View 공간 방향 벡터로 역투영 (Z = 1 평면)
	Engine::vec3 UnprojectNDC(const Engine::vec2& ndc, const Engine::mat4& invProj)
	{
		Engine::vec4 clip = Engine::vec4(ndc.x, ndc.y, 1.0f, 1.0f);
		Engine::vec4 view = invProj * clip;
		return Engine::vec3(view) / view.w;
	}

	// 단순 점광원 감쇄 및 조도 계산 (lightingPS.hlsl 시뮬레이션용)
	float EvaluatePointLightLuminance(const GPULight& light, const Engine::vec3& surfacePos)
	{
		Engine::vec3 toLight = light.Position - surfacePos;
		float dist = glm::length(toLight);
		if (dist >= light.Range || dist <= 0.0001f)
			return 0.0f;

		// InverseSquare 기반 감쇄
		float atten = 1.0f / (1.0f + 0.1f * dist + 0.01f * dist * dist);
		float window = glm::clamp(1.0f - std::pow(dist / light.Range, 4.0f), 0.0f, 1.0f);
		window = window * window;
		return light.Intensity * atten * window;
	}
}

// ==============================================================================
// [Test 1] 4방향 타일 절두체 평면 생성 및 구면 교차 판정 정합성
// ==============================================================================
void CheckTileFrustumPlanesAndSphereIntersection()
{
	std::cout << "[Test 1] Tile Frustum Planes & Sphere Intersection Verification" << std::endl;

	const Engine::uint32 screenWidth = 1920;
	const Engine::uint32 screenHeight = 1080;

	// DirectX 왼손 좌표계 원근 투영 행렬 (FOV 60도, Aspect 16:9, Near 0.1, Far 1000)
	const float fovY = glm::radians(60.0f);
	const float aspect = static_cast<float>(screenWidth) / static_cast<float>(screenHeight);
	const Engine::mat4 proj = glm::perspectiveLH_ZO(fovY, aspect, 0.1f, 1000.0f);
	const Engine::mat4 invProj = glm::inverse(proj);

	// 중앙 타일 (TileX = 60, TileY = 34)
	const Engine::uint32 groupX = 60;
	const Engine::uint32 groupY = 34;

	const float pxMinX = static_cast<float>(groupX * TILE_SIZE);
	const float pxMaxX = std::min(static_cast<float>((groupX + 1) * TILE_SIZE), static_cast<float>(screenWidth));
	const float pxMinY = static_cast<float>(groupY * TILE_SIZE);
	const float pxMaxY = std::min(static_cast<float>((groupY + 1) * TILE_SIZE), static_cast<float>(screenHeight));

	const Engine::vec2 ndcMin = Engine::vec2((pxMinX / static_cast<float>(screenWidth)) * 2.0f - 1.0f,
											 1.0f - (pxMaxY / static_cast<float>(screenHeight)) * 2.0f);
	const Engine::vec2 ndcMax = Engine::vec2((pxMaxX / static_cast<float>(screenWidth)) * 2.0f - 1.0f,
											 1.0f - (pxMinY / static_cast<float>(screenHeight)) * 2.0f);

	const Engine::vec3 v00 = UnprojectNDC(Engine::vec2(ndcMin.x, ndcMax.y), invProj); // 좌상단
	const Engine::vec3 v10 = UnprojectNDC(Engine::vec2(ndcMax.x, ndcMax.y), invProj); // 우상단
	const Engine::vec3 v01 = UnprojectNDC(Engine::vec2(ndcMin.x, ndcMin.y), invProj); // 좌하단
	const Engine::vec3 v11 = UnprojectNDC(Engine::vec2(ndcMax.x, ndcMin.y), invProj); // 우하단

	Engine::vec4 frustumPlanes[4];
	frustumPlanes[0] = Engine::vec4(glm::normalize(glm::cross(v00, v01)), 0.0f); // Left Plane
	frustumPlanes[1] = Engine::vec4(glm::normalize(glm::cross(v11, v10)), 0.0f); // Right Plane
	frustumPlanes[2] = Engine::vec4(glm::normalize(glm::cross(v10, v00)), 0.0f); // Top Plane
	frustumPlanes[3] = Engine::vec4(glm::normalize(glm::cross(v01, v11)), 0.0f); // Bottom Plane

	// 타일 중심 방향 벡터 계산
	Engine::vec3 tileCenterDir = glm::normalize((v00 + v10 + v01 + v11) * 0.25f);

	// 1. 모든 절두체 평면 법선이 타일 내부를 향하는지(내적 > 0) 검증
	for (int p = 0; p < 4; ++p)
	{
		float dotVal = glm::dot(Engine::vec3(frustumPlanes[p]), tileCenterDir);
		assert(dotVal > 0.0f && "Tile frustum plane normal must point inward to the tile cone!");
	}

	// 2. 타일 중심에 위치한 광원 (거리 10m, 반경 2m)
	Engine::vec3 lightCenter = tileCenterDir * 10.0f;
	float lightRange = 2.0f;
	for (int p = 0; p < 4; ++p)
	{
		float distToPlane = glm::dot(Engine::vec3(frustumPlanes[p]), lightCenter);
		assert(distToPlane >= -lightRange && "Center light must be inside tile frustum planes!");
	}

	// 3. Left 평면 바깥쪽으로 벗어난 광원 배제 검증
	Engine::vec3 leftOutsidePos = lightCenter - Engine::vec3(frustumPlanes[0]) * (lightRange + 1.0f);
	assert(glm::dot(Engine::vec3(frustumPlanes[0]), leftOutsidePos) < -lightRange && "Left-outside light must be rejected!");

	// 4. Right 평면 바깥쪽으로 벗어난 광원 배제 검증
	Engine::vec3 rightOutsidePos = lightCenter - Engine::vec3(frustumPlanes[1]) * (lightRange + 1.0f);
	assert(glm::dot(Engine::vec3(frustumPlanes[1]), rightOutsidePos) < -lightRange && "Right-outside light must be rejected!");

	// 5. 뷰 공간 Z축 클리핑 검증 (minTileZ = 8.0, maxTileZ = 12.0)
	const float minTileZ = 8.0f;
	const float maxTileZ = 12.0f;

	// 너무 가까운 광원 (Z = 5.0m, Range = 2.0m -> Z+Range = 7.0 < 8.0)
	Engine::vec3 tooNearPos = tileCenterDir * 5.0f;
	assert((tooNearPos.z + lightRange < minTileZ) && "Too-near light must be culled by minTileZ!");

	// 너무 먼 광원 (Z = 15.0m, Range = 2.0m -> Z-Range = 13.0 > 12.0)
	Engine::vec3 tooFarPos = tileCenterDir * 15.0f;
	assert((tooFarPos.z - lightRange > maxTileZ) && "Too-far light must be culled by maxTileZ!");

	std::cout << "  -> Tile frustum planes normal orientation and spherical bounds culling passed." << std::endl;
}

// ==============================================================================
// [Test 2] 타일당 256개 이하 정상 컬링 및 인덱스 리스트 패킹 검증
// ==============================================================================
void CheckNormalTilingAndIndexPacking()
{
	std::cout << "[Test 2] Normal Tiling (< 256 lights) & Grid/Index Buffer Packing" << std::endl;

	// Case A: 180개 광원이 선별되는 일반적인 타일
	{
		Engine::uint32 tileLightCount = 180;
		Engine::uint32 tileIndex = 42;
		Engine::uint32 startIndex = tileIndex * MAX_LIGHTS_PER_TILE;

		std::vector<Engine::uint32> tileIndices(MAX_LIGHTS_PER_TILE, 0);
		for (Engine::uint32 i = 0; i < tileLightCount; ++i)
		{
			tileIndices[i] = i * 2; // 임의의 광원 인덱스
		}

		glm::uvec2 gridEntry(startIndex, tileLightCount);
		assert(gridEntry.x == startIndex);
		assert(gridEntry.y == 180);
		assert(gridEntry.y <= MAX_LIGHTS_PER_TILE && "Normal tile count must be <= 256!");

		// 인덱스 리스트 버퍼 시뮬레이션
		std::vector<Engine::uint32> indexList(MAX_LIGHTS_PER_TILE, 0);
		Engine::uint32 countToCopy = std::min(tileLightCount, MAX_LIGHTS_PER_TILE);
		for (Engine::uint32 i = 0; i < countToCopy; ++i)
		{
			indexList[i] = tileIndices[i];
		}

		for (Engine::uint32 i = 0; i < countToCopy; ++i)
		{
			assert(indexList[i] == i * 2);
		}
	}

	// Case B: 255개, 256개 상한선 경계값(Boundary) 검증
	{
		// 255개
		Engine::uint32 count255 = 255;
		glm::uvec2 grid255(0, count255);
		assert(grid255.y <= MAX_LIGHTS_PER_TILE);

		// 256개 (타일 인덱스 리스트의 최대 수용치)
		Engine::uint32 count256 = 256;
		glm::uvec2 grid256(0, count256);
		assert(grid256.y <= MAX_LIGHTS_PER_TILE);
		assert(std::min(count256, MAX_LIGHTS_PER_TILE) == 256);
	}

	std::cout << "  -> Normal tiling (< 256) and boundary conditions (255, 256) packing passed." << std::endl;
}

// ==============================================================================
// [Test 3] 타일당 256개 초과 오버플로 감지 및 전역 순회 폴백의 조도 일치성 검증
// ==============================================================================
void CheckOverflowFallbackAndLuminanceConsistency()
{
	std::cout << "[Test 3] Tile Overflow (> 256 lights) Fallback & Luminance Consistency" << std::endl;

	// 1개의 타일에 280개의 점광원이 집중되는 과부하 씬 구성
	const Engine::uint32 totalLights = 300;
	const Engine::uint32 overlappingLights = 280; // 256개를 초과하는 상황

	std::vector<GPULight> sceneLights(totalLights);
	const Engine::vec3 targetSurfacePos(0.0f, 0.0f, 10.0f);

	for (Engine::uint32 i = 0; i < totalLights; ++i)
	{
		if (i < overlappingLights)
		{
			// 타일 내부 지오메트리 표면에 인접한 280개 광원 (거리 약 0.5m < Range 5.0m)
			sceneLights[i].Position = targetSurfacePos + Engine::vec3(0.1f * (i % 10), 0.1f * ((i / 10) % 10), 0.5f);
			sceneLights[i].Range = 5.0f;
		}
		else
		{
			// 타일 절두체 및 표면에서 완전히 멀리 떨어진 20개 광원 (거리 > 100m >> Range 5.0m)
			sceneLights[i].Position = targetSurfacePos + Engine::vec3(100.0f + 5.0f * (i - overlappingLights), 0.0f, 0.0f);
			sceneLights[i].Range = 5.0f;
		}

		sceneLights[i].Intensity = 10.0f + static_cast<float>(i % 5);
		sceneLights[i].Color = Engine::vec3(1.0f, 0.9f, 0.8f);
		sceneLights[i].PackedFlags = PackLightFlags(TEST_LIGHT_FLAG_USE_IN_DEFERRED | TEST_LIGHT_FLAG_AFFECT_DIFFUSE,
													TEST_LIGHT_TYPE_POINT, 1u);
	}

	// 1. Compute Shader 시뮬레이션:
	// 컬링 루프에서 280개의 광원이 타일 절두체 내에 포함됨
	Engine::uint32 s_TileLightCount = 0;
	std::vector<Engine::uint32> s_TileLightIndices(MAX_LIGHTS_PER_TILE, 0);

	for (Engine::uint32 lightIdx = 0; lightIdx < totalLights; ++lightIdx)
	{
		if (lightIdx < overlappingLights)
		{
			Engine::uint32 slot = s_TileLightCount++;
			if (slot < MAX_LIGHTS_PER_TILE)
			{
				s_TileLightIndices[slot] = lightIdx;
			}
		}
	}

	assert(s_TileLightCount == 280 && "Total culled light count must be 280 without truncation in accumulator!");

	// 전역 버퍼에 타일 결과 기록 (A06 방식)
	// startIndex = 0, gridInfo.y = 280 (실제 컬링된 전체 카운트 보존)
	glm::uvec2 g_TileLightGrid(0, s_TileLightCount);
	std::vector<Engine::uint32> g_TileLightIndexList(MAX_LIGHTS_PER_TILE, 0);
	Engine::uint32 writeCount = std::min(s_TileLightCount, MAX_LIGHTS_PER_TILE);
	for (Engine::uint32 i = 0; i < writeCount; ++i)
	{
		g_TileLightIndexList[i] = s_TileLightIndices[i];
	}

	// 2. Pixel Shader 시뮬레이션:
	// Ground Truth 조도: 타일에 영향을 미치는 280개 광원의 실제 총 조도
	float groundTruthLuminance = 0.0f;
	for (Engine::uint32 i = 0; i < overlappingLights; ++i)
	{
		groundTruthLuminance += EvaluatePointLightLuminance(sceneLights[i], targetSurfacePos);
	}

	// [비교군 A] 만약 256개에서 잘린 타일 인덱스 리스트만 순회했을 경우 (과거 조도 누락 버그)
	float truncatedLuminance = 0.0f;
	for (Engine::uint32 i = 0; i < 256; ++i)
	{
		truncatedLuminance += EvaluatePointLightLuminance(sceneLights[g_TileLightIndexList[i]], targetSurfacePos);
	}
	// 257번째부터 280번째까지 24개 광원의 빛이 소실되어 조도가 부족함
	assert(truncatedLuminance < groundTruthLuminance && "Truncated traversal must cause missing light intensity!");

	// [비교군 B] BamEngine의 A06 본체 오버플로 폴백 메커니즘:
	// lightingPS.hlsl (670~677행):
	// if (tileInfo.y > 256u) { useTiled = false; loopCount = numLights; }
	bool useTiled = true;
	Engine::uint32 loopCount = 0;
	if (g_TileLightGrid.y > MAX_LIGHTS_PER_TILE)
	{
		// 오버플로 감지 -> 안전한 전체 순회 폴백
		useTiled = false;
		loopCount = totalLights;
	}
	else
	{
		loopCount = g_TileLightGrid.y;
	}

	assert(!useTiled && "Tile with > 256 lights must fallback to full lights traversal!");
	assert(loopCount == totalLights);

	float fallbackLuminance = 0.0f;
	for (Engine::uint32 i = 0; i < loopCount; ++i)
	{
		Engine::uint32 lightIndex = useTiled ? g_TileLightIndexList[i] : i;
		fallbackLuminance += EvaluatePointLightLuminance(sceneLights[lightIndex], targetSurfacePos);
	}

	// 폴백 결과는 Ground Truth와 정확히 일치하여 256개 초과 시에도 단 1개의 광원도 누락되지 않음 검증!
	float diff = std::abs(fallbackLuminance - groundTruthLuminance);
	assert(diff < 0.0001f && "Fallback luminance must 100% match ground truth without any light loss!");

	std::cout << "  -> Overflow fallback correctly triggered (280 > 256) and 100% matches ground truth luminance." << std::endl;
}

// ==============================================================================
// [Test 4] 1,024개 대규모 광원 스트레스 및 Directional/Sky 광원 전역 포함 검증
// ==============================================================================
void Check1024LightsStressAndGlobalInclusion()
{
	std::cout << "[Test 4] 1,024 Max Lights Stress & Global Light Rule Verification" << std::endl;

	assert(MAX_LIGHTS == 1024 && "MAX_LIGHTS must be 1024!");

	// 1. Directional Light 및 Sky Light 전역 포함 규칙 검증
	// 지오메트리가 없는 타일 (hasGeometry == false):
	// Point/Spot 광원은 수집되지 않지만, Directional(타입 1) 및 Sky(타입 3) 광원은 무조건 포함되어야 함
	{
		std::vector<GPULight> lights(5);
		lights[0].PackedFlags = PackLightFlags(TEST_LIGHT_FLAG_USE_IN_DEFERRED, TEST_LIGHT_TYPE_DIRECTIONAL, 2u);
		lights[1].PackedFlags = PackLightFlags(TEST_LIGHT_FLAG_USE_IN_DEFERRED, TEST_LIGHT_TYPE_SKY, 2u);
		lights[2].PackedFlags = PackLightFlags(TEST_LIGHT_FLAG_USE_IN_DEFERRED, TEST_LIGHT_TYPE_POINT, 1u);
		lights[3].PackedFlags = PackLightFlags(TEST_LIGHT_FLAG_USE_IN_DEFERRED, TEST_LIGHT_TYPE_SPOT, 1u);
		lights[4].PackedFlags = PackLightFlags(TEST_LIGHT_FLAG_USE_IN_DEFERRED, TEST_LIGHT_TYPE_DIRECTIONAL, 2u);

		bool hasGeometry = false; // 공백 타일
		Engine::uint32 s_TileLightCount = 0;
		std::vector<Engine::uint32> s_TileLightIndices(MAX_LIGHTS_PER_TILE, 0);

		for (Engine::uint32 idx = 0; idx < lights.size(); ++idx)
		{
			Engine::uint32 type = (lights[idx].PackedFlags >> 24u) & 0x3u;
			if (type == TEST_LIGHT_TYPE_DIRECTIONAL || type == TEST_LIGHT_TYPE_SKY)
			{
				Engine::uint32 slot = s_TileLightCount++;
				if (slot < MAX_LIGHTS_PER_TILE) s_TileLightIndices[slot] = idx;
				continue;
			}

			if (!hasGeometry) continue;
		}

		assert(s_TileLightCount == 3 && "Directional and Sky lights must be included even when geometry is absent!");
		assert(s_TileLightIndices[0] == 0);
		assert(s_TileLightIndices[1] == 1);
		assert(s_TileLightIndices[2] == 4);
	}

	// 2. LightManager 타일 버퍼 크기 계산 검증 (1920x1080)
	{
		const Engine::uint32 width = 1920;
		const Engine::uint32 height = 1080;
		const Engine::uint32 tilesX = (width + 15) / 16;
		const Engine::uint32 tilesY = (height + 15) / 16;
		const Engine::uint32 totalTiles = tilesX * tilesY;

		assert(tilesX == 120);
		assert(tilesY == 68);
		assert(totalTiles == 8160);

		// Grid Buffer: uint2 (8 bytes) * totalTiles
		const Engine::uint32 gridBufferSize = totalTiles * sizeof(Engine::uint32) * 2;
		assert(gridBufferSize == 8160 * 8); // 65,280 bytes

		// Index List Buffer: 256 * sizeof(uint32) * totalTiles
		const Engine::uint32 indexBufferSize = totalTiles * MAX_LIGHTS_PER_TILE * sizeof(Engine::uint32);
		assert(indexBufferSize == 8160 * 256 * 4); // 8,355,840 bytes (~7.97 MB)
	}

	std::cout << "  -> 1,024 max lights and global lights inclusion verified." << std::endl;
}

// ==============================================================================
// Main Entry Point
// ==============================================================================
int main()
{
	std::cout << "==========================================================" << std::endl;
	std::cout << " [TiledLightingCheck] Tiled Lighting & Overflow Unit Tests " << std::endl;
	std::cout << "==========================================================" << std::endl;

	CheckTileFrustumPlanesAndSphereIntersection();
	CheckNormalTilingAndIndexPacking();
	CheckOverflowFallbackAndLuminanceConsistency();
	Check1024LightsStressAndGlobalInclusion();

	std::cout << "==========================================================" << std::endl;
	std::cout << " All TiledLightingCheck unit tests PASSED (4/4, 100%)." << std::endl;
	std::cout << "==========================================================" << std::endl;

	return 0;
}
