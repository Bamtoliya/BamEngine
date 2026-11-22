#include "TilemapComponent.h"
#include "SpriteAtlas.h"
#include "SpriteBatcher.h"
#include "RenderCommands.h"
#include <iostream>
#include <cmath>
#include <vector>
#include "ResourceManager.h"

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
	if (!ResourceManager::Create())
		return Fail("Resource manager initialization failed.");

	struct ResourceScope
	{
		~ResourceScope() { ResourceManager::Destroy(); }
	} resourceScope;

	SpriteCreateDesc desc{};
	desc.Key = L"TilemapCheck.Sprite";

	auto* sprite = Sprite::Create(&desc);
	if (!sprite)
		return Fail("Sprite creation failed.");

	const auto dummySprite = ResourceManager::Get().AddResource<Sprite>(desc.Key, sprite);
	if (!dummySprite.IsValid())
		return Fail("Sprite registration failed.");

	TilemapComponent tilemap;
	tilemap.cellSize = vec2(1.0f, 1.0f);

	// ── Test 1: CellToWorld 및 WorldToCell 좌표 변환 검증 ──
	{
		vec2 w0 = tilemap.CellToWorld(0, 0);
		if (!Near(w0.x, 0.0f) || !Near(w0.y, 0.0f))
			return Fail("CellToWorld(0, 0) should be (0, 0)");

		vec2 w1 = tilemap.CellToWorld(10, -5);
		if (!Near(w1.x, 10.0f) || !Near(w1.y, -5.0f))
			return Fail("CellToWorld(10, -5) should be (10, -5)");

		vec2 c0 = tilemap.WorldToCell(vec2(10.2f, -4.8f));
		if (static_cast<int32>(c0.x) != 10 || static_cast<int32>(c0.y) != -5)
			return Fail("WorldToCell(10.2, -4.8) should be (10, -5)");

		vec2 c1 = tilemap.WorldToCell(vec2(-0.1f, -0.9f));
		if (static_cast<int32>(c1.x) != -1 || static_cast<int32>(c1.y) != -1)
			return Fail("WorldToCell(-0.1, -0.9) should be (-1, -1)");
	}

	// ── Test 2: SetTile, GetTile 및 청크 분할 검증 (양수 & 음수 좌표) ──
	{
		tilemap.SetTile(5, 5, dummySprite, vec4(1.0f, 0.0f, 0.0f, 1.0f));
		tilemap.SetTile(25, 30, dummySprite, vec4(0.0f, 1.0f, 0.0f, 1.0f));
		tilemap.SetTile(-5, 10, dummySprite, vec4(0.0f, 0.0f, 1.0f, 1.0f));
		tilemap.SetTile(-20, -25, dummySprite, vec4(1.0f, 1.0f, 1.0f, 1.0f));

		// 청크 키 검증: 4개 영역이 서로 다른 청크여야 함
		// (5, 5) -> chunk(0, 0)
		// (25, 30) -> chunk(1, 1)
		// (-5, 10) -> chunk(-1, 0)
		// (-20, -25) -> chunk(-2, -2)
		if (tilemap.chunks.size() != 4)
			return Fail("Tilemap should have exactly 4 chunks for the 4 spread positions");

		const TileCell* cell1 = tilemap.GetTile(5, 5);
		if (!cell1 || !cell1->sprite.IsValid() || !Near(cell1->color.r, 1.0f))
			return Fail("GetTile(5, 5) failed to return valid cell data");

		const TileCell* cell2 = tilemap.GetTile(25, 30);
		if (!cell2 || !cell2->sprite.IsValid() || !Near(cell2->color.g, 1.0f))
			return Fail("GetTile(25, 30) failed to return valid cell data");

		const TileCell* cell3 = tilemap.GetTile(-5, 10);
		if (!cell3 || !cell3->sprite.IsValid() || !Near(cell3->color.b, 1.0f))
			return Fail("GetTile(-5, 10) failed to return valid cell data in negative chunk");

		const TileCell* cell4 = tilemap.GetTile(-20, -25);
		if (!cell4 || !cell4->sprite.IsValid())
			return Fail("GetTile(-20, -25) failed to return valid cell data in (-2, -2) chunk");

		// 빈 타일 조회는 nullptr 반환
		if (tilemap.GetTile(0, 0) != nullptr)
			return Fail("GetTile on empty cell should return nullptr");
	}

	// ── Test 3: ClearTile 검증 ──
	{
		tilemap.ClearTile(5, 5);
		if (tilemap.GetTile(5, 5) != nullptr)
			return Fail("GetTile(5, 5) after ClearTile should return nullptr");
	}

	// ── Test 4: 직렬화 동기화(SyncSavedTiles) 및 복원(RestoreFromSavedTiles) 검증 ──
	{
		tilemap.SyncSavedTiles();
		// 현재 남은 타일: (25, 30), (-5, 10), (-20, -25) 총 3개
		if (tilemap.savedTiles.size() != 3)
			return Fail("SyncSavedTiles should produce exactly 3 saved tiles");

		// 런타임 청크 초기화
		tilemap.ClearAll();
		if (tilemap.GetTile(25, 30) != nullptr)
			return Fail("ClearAll failed to clear chunks");

		// 복원
		tilemap.RestoreFromSavedTiles();
		if (tilemap.chunks.size() != 3)
			return Fail("RestoreFromSavedTiles should restore into 3 chunks");

		const TileCell* restoredCell = tilemap.GetTile(25, 30);
		if (!restoredCell || !restoredCell->sprite.IsValid() || !Near(restoredCell->color.g, 1.0f))
			return Fail("Restored cell (25, 30) data is invalid");
	}

	// ── Test 5: SpriteAtlas MaxRects Packing 및 RGBA8 Pixel Compositing 검증 ──
	{
		// 4x4 빨강 (RGBA = 255, 0, 0, 255)
		std::vector<uint8> redPixels(4 * 4 * 4);
		for (size_t i = 0; i < redPixels.size(); i += 4)
		{
			redPixels[i + 0] = 255;
			redPixels[i + 1] = 0;
			redPixels[i + 2] = 0;
			redPixels[i + 3] = 255;
		}

		// 4x4 초록 (RGBA = 0, 255, 0, 255)
		std::vector<uint8> greenPixels(4 * 4 * 4);
		for (size_t i = 0; i < greenPixels.size(); i += 4)
		{
			greenPixels[i + 0] = 0;
			greenPixels[i + 1] = 255;
			greenPixels[i + 2] = 0;
			greenPixels[i + 3] = 255;
		}

		// 2x2 파랑 (RGBA = 0, 0, 255, 255)
		std::vector<uint8> bluePixels(2 * 2 * 4);
		for (size_t i = 0; i < bluePixels.size(); i += 4)
		{
			bluePixels[i + 0] = 0;
			bluePixels[i + 1] = 0;
			bluePixels[i + 2] = 255;
			bluePixels[i + 3] = 255;
		}

		std::vector<AtlasInputItem> items;
		items.push_back({ "Red", 4, 4, redPixels.data() });
		items.push_back({ "Green", 4, 4, greenPixels.data() });
		items.push_back({ "Blue", 2, 2, bluePixels.data() });

		const int32 atlasW = 32;
		const int32 atlasH = 32;
		const int32 padding = 1;

		std::vector<AtlasEntry> entries;
		if (!SpriteAtlas::PackMaxRects(atlasW, atlasH, padding, items, entries))
			return Fail("SpriteAtlas::PackMaxRects failed to pack items");

		if (entries.size() != 3)
			return Fail("SpriteAtlas should pack exactly 3 entries");

		std::vector<uint8> bakedBuffer;
		if (!SpriteAtlas::BakeAtlasPixels(atlasW, atlasH, padding, items, entries, bakedBuffer))
			return Fail("SpriteAtlas::BakeAtlasPixels failed");

		if (bakedBuffer.size() != static_cast<size_t>(atlasW * atlasH * 4))
			return Fail("Baked atlas buffer size does not match atlas dimensions");

		// 각 엔트리의 픽셀 색상 검증
		for (size_t idx = 0; idx < items.size(); ++idx)
		{
			const auto& item = items[idx];
			const auto& entry = entries[idx];
			const int32 x = static_cast<int32>(entry.pixelRegion.left);
			const int32 y = static_cast<int32>(entry.pixelRegion.top);

			const size_t pixelOffset = (static_cast<size_t>(y) * static_cast<size_t>(atlasW) + static_cast<size_t>(x)) * 4;
			const uint8 r = bakedBuffer[pixelOffset + 0];
			const uint8 g = bakedBuffer[pixelOffset + 1];
			const uint8 b = bakedBuffer[pixelOffset + 2];
			const uint8 a = bakedBuffer[pixelOffset + 3];

			if (item.name == "Red" && (r != 255 || g != 0 || b != 0 || a != 255))
				return Fail("Red sprite pixel in baked atlas does not match expected RGBA");
			if (item.name == "Green" && (r != 0 || g != 255 || b != 0 || a != 255))
				return Fail("Green sprite pixel in baked atlas does not match expected RGBA");
			if (item.name == "Blue" && (r != 0 || g != 0 || b != 255 || a != 255))
				return Fail("Blue sprite pixel in baked atlas does not match expected RGBA");

			// 패딩 영역 복제 확인 (상단 패딩 픽셀)
			if (y - padding >= 0)
			{
				const size_t padOffset = (static_cast<size_t>(y - padding) * static_cast<size_t>(atlasW) + static_cast<size_t>(x)) * 4;
				if (bakedBuffer[padOffset + 0] != r || bakedBuffer[padOffset + 1] != g ||
					bakedBuffer[padOffset + 2] != b || bakedBuffer[padOffset + 3] != a)
				{
					return Fail("Atlas padding edge replication failed");
				}
			}
		}
	}

	// ── Test 6: SpriteBatcher 정렬, 버킷 그룹화 및 드로우콜 절감 통계 검증 ──
	{
		auto* matA = reinterpret_cast<MaterialInterface*>(0x1000);
		auto* matB = reinterpret_cast<MaterialInterface*>(0x2000);
		auto* texA = reinterpret_cast<Texture*>(0x3000);
		auto* texB = reinterpret_cast<Texture*>(0x4000);

		SpriteBatcher batcher;
		batcher.Begin();

		// 무작위 순서로 스프라이트 제출
		SpriteDrawCommand c1{};
		c1.sortingLayerID = 2; c1.orderInLayer = 0; c1.material = matA; c1.texture = texA;
		batcher.DrawSprite(c1);

		SpriteDrawCommand c2{};
		c2.sortingLayerID = 0; c2.orderInLayer = 10; c2.material = matB; c2.texture = texB;
		batcher.DrawSprite(c2);

		SpriteDrawCommand c3{};
		c3.sortingLayerID = 0; c3.orderInLayer = 0; c3.material = matA; c3.texture = texA;
		batcher.DrawSprite(c3);

		SpriteDrawCommand c4{}; // c3과 동일한 버킷
		c4.sortingLayerID = 0; c4.orderInLayer = 0; c4.material = matA; c4.texture = texA;
		batcher.DrawSprite(c4);

		SpriteDrawCommand c5{};
		c5.sortingLayerID = 1; c5.orderInLayer = -1; c5.material = matA; c5.texture = texB;
		batcher.DrawSprite(c5);

		SpriteDrawCommand c6{};
		c6.sortingLayerID = 0; c6.orderInLayer = 0; c6.material = matA; c6.texture = texB;
		batcher.DrawSprite(c6);

		if (batcher.GetTotalQuadCount() != 6)
			return Fail("Total quad count should be 6");

		if (batcher.GetBucketCount() != 5)
			return Fail("Bucket count should be 5 for 5 unique keys");

		if (batcher.GetDrawCallSavings() != 1)
			return Fail("Draw call savings should be 1 (6 quads - 5 buckets)");

		batcher.Flush();

		const auto& flushed = batcher.GetFlushedCommands();
		if (flushed.size() != 6)
			return Fail("Flushed commands count should be 6");

		// 정렬 순서 검증: (sortingLayerID, orderInLayer, material, texture)
		for (size_t i = 1; i < flushed.size(); ++i)
		{
			const auto& prev = flushed[i - 1];
			const auto& curr = flushed[i];

			if (curr.sortingLayerID < prev.sortingLayerID)
				return Fail("Flushed commands are not sorted by sortingLayerID");
			if (curr.sortingLayerID == prev.sortingLayerID)
			{
				if (curr.orderInLayer < prev.orderInLayer)
					return Fail("Flushed commands are not sorted by orderInLayer");
				if (curr.orderInLayer == prev.orderInLayer)
				{
					if (curr.material < prev.material)
						return Fail("Flushed commands are not sorted by material");
				}
			}
		}
	}

	std::cout << "[PASS] All Tilemap tests passed successfully!" << std::endl;
	return 0;
}
