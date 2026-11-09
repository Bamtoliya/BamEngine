#include "SpriteAtlas.h"
#include "SpriteBatcher.h"
#include <iostream>
#include <cmath>

namespace
{
	using namespace Engine;

	bool Near(f32 a, f32 b, f32 eps = 0.0001f)
	{
		return std::fabs(a - b) <= eps;
	}

	int Fail(const char* message)
	{
		std::cerr << "[FAIL] " << message << std::endl;
		return 1;
	}
}

int main()
{
	// ── Test 1: MaxRects 2D Bin Packing 기본 배치 성공률 검증 ──
	std::vector<AtlasInputItem> items = {
		{ "Player_Idle_0", 64, 64 },
		{ "Player_Idle_1", 64, 64 },
		{ "Player_Walk_0", 48, 64 },
		{ "Monster_Boss", 128, 64 },
		{ "Coin_0", 16, 16 },
		{ "Coin_1", 16, 16 },
		{ "Sword_Icon", 32, 64 },
		{ "Shield_Icon", 48, 48 },
		{ "Potion_Red", 24, 32 },
		{ "Potion_Blue", 24, 32 }
	};

	const int32 atlasW = 256;
	const int32 atlasH = 256;
	const int32 padding = 2;

	std::vector<AtlasEntry> entries;
	const bool packSuccess = SpriteAtlas::PackMaxRects(atlasW, atlasH, padding, items, entries);
	if (!packSuccess)
		return Fail("PackMaxRects failed to pack items into 256x256 atlas");

	if (entries.size() != items.size())
		return Fail("Entries count does not match input items count");

	// ── Test 2: 패킹된 스프라이트 간 비충돌 (No Overlap) 전수 검증 ──
	for (size_t i = 0; i < entries.size(); ++i)
	{
		const auto& a = entries[i].pixelRegion;
		for (size_t j = i + 1; j < entries.size(); ++j)
		{
			const auto& b = entries[j].pixelRegion;

			// 두 사각형이 겹치는지 검사 (패딩 영역까지 고려)
			const bool overlapX = (a.left < b.left + b.width) && (a.left + a.width > b.left);
			const bool overlapY = (a.top < b.top + b.height) && (a.top + a.height > b.top);

			if (overlapX && overlapY)
			{
				std::cerr << "Overlap detected between " << entries[i].name << " and " << entries[j].name << std::endl;
				return Fail("SpriteAtlas entries overlap with each other!");
			}
		}
	}

	// ── Test 3: UV 좌표 정규화 범위 [0, 1] 검증 ──
	for (const auto& entry : entries)
	{
		const vec4& uv = entry.uvTransform;
		if (uv.x < 0.0f || uv.y < 0.0f || uv.z <= 0.0f || uv.w <= 0.0f)
			return Fail("Invalid UV transform values");

		if (uv.x + uv.z > 1.0001f || uv.y + uv.w > 1.0001f)
			return Fail("UV bounds exceed 1.0 (out of atlas boundaries)");
	}

	// ── Test 4: SpriteBatcher 드로우콜 배칭 및 절감 통계 검증 ──
	SpriteBatcher batcher;
	batcher.Begin();

	// 가상의 머티리얼 및 텍스처 포인터 (주소 식별용)
	MaterialInterface* matA = reinterpret_cast<MaterialInterface*>(0x1000);
	Texture* texA = reinterpret_cast<Texture*>(0x2000);
	Texture* texB = reinterpret_cast<Texture*>(0x3000);

	// Texture A를 사용하는 스프라이트 60개 추가
	for (int i = 0; i < 60; ++i)
	{
		mat4 world = glm::translate(mat4(1.0f), vec3(static_cast<float>(i), 0.0f, 0.0f));
		batcher.DrawQuad(matA, texA, world, vec2(1.0f, 1.0f), vec2(0.5f, 0.5f),
			vec4(0.0f, 0.0f, 1.0f, 1.0f), vec4(1.0f));
	}

	// Texture B를 사용하는 스프라이트 40개 추가
	for (int i = 0; i < 40; ++i)
	{
		mat4 world = glm::translate(mat4(1.0f), vec3(static_cast<float>(i), 5.0f, 0.0f));
		batcher.DrawQuad(matA, texB, world, vec2(1.0f, 1.0f), vec2(0.5f, 0.5f),
			vec4(0.0f, 0.0f, 1.0f, 1.0f), vec4(1.0f));
	}

	if (batcher.GetTotalQuadCount() != 100)
		return Fail("Total quad count should be 100");

	// 100개의 개별 스프라이트가 단 2개의 배치 버킷으로 묶였는지 검증
	if (batcher.GetBucketCount() != 2)
		return Fail("Bucket count should be 2 for 2 distinct textures");

	// 드로우콜 절감 횟수: 100 - 2 = 98회
	if (batcher.GetDrawCallSavings() != 98)
		return Fail("Draw call savings should be 98 (100 - 2)");

	// 각 버킷의 정점 및 인덱스 개수 검증
	const auto& buckets = batcher.GetBuckets();
	const SpriteBatchKey keyA{ matA, texA, 0 };
	const SpriteBatchKey keyB{ matA, texB, 0 };

	auto itA = buckets.find(keyA);
	if (itA == buckets.end() || itA->second.quadCount != 60)
		return Fail("Bucket A should contain 60 quads");
	if (itA->second.vertices.size() != 240 || itA->second.indices.size() != 360)
		return Fail("Bucket A vertex/index count mismatch (expected 240 verts, 360 indices)");

	auto itB = buckets.find(keyB);
	if (itB == buckets.end() || itB->second.quadCount != 40)
		return Fail("Bucket B should contain 40 quads");
	if (itB->second.vertices.size() != 160 || itB->second.indices.size() != 240)
		return Fail("Bucket B vertex/index count mismatch (expected 160 verts, 240 indices)");

	std::cout << "[PASS] All SpriteAtlas and SpriteBatcher tests passed successfully!" << std::endl;
	return 0;
}
