#include "TilemapComponent.h"
#include <iostream>
#include <cmath>
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

	std::cout << "[PASS] All Tilemap tests passed successfully!" << std::endl;
	return 0;
}
