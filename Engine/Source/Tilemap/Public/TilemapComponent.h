#pragma once

#include "ComponentAnnotations.h"
#include "ResourceHandle.h"
#include "Sprite.h"
#include <unordered_map>
#include <cstdint>

namespace Engine
{
	class Mesh;
	class MaterialInterface;

	struct TileCell
	{
		ResourceHandle<Sprite> sprite;
		vec4 color = vec4(1.0f);
	};

	constexpr int32 CHUNK_SIZE = 16; // 16x16 타일 블록

	struct TilemapChunk
	{
		TileCell tiles[CHUNK_SIZE * CHUNK_SIZE];
		bool hasTiles = false;
		bool isDirty = true;
	};

	inline uint64_t MakeChunkKey(int32 chunkX, int32 chunkY)
	{
		return (static_cast<uint64_t>(static_cast<uint32_t>(chunkX)) << 32) |
		       static_cast<uint64_t>(static_cast<uint32_t>(chunkY));
	}

	STRUCT(COMPONENT, DISPLAY_NAME("STRUCT_TILEMAP"), CATEGORY("CATEGORY_RENDER"))
	struct TilemapComponent
	{
		REFLECT_BODY()

		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_CELL_SIZE"))
		vec2 cellSize = vec2(1.0f, 1.0f); // 1개 타일 칸의 월드 크기

		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_MATERIAL_HANDLE"))
		ResourceHandle<MaterialInterface> materialHandle;

		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_MESH_HANDLE"))
		ResourceHandle<Mesh> meshHandle;

		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_SORTING_LAYER"))
		int32 sortingLayerID = 0;

		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_ORDER_IN_LAYER"))
		int32 orderInLayer = 0;

		// 런타임 청크 맵 (청크 좌표 -> 16x16 청크)
		std::unordered_map<uint64_t, TilemapChunk> chunks;

	public:
		void SetTile(int32 x, int32 y, ResourceHandle<Sprite> sprite, vec4 color = vec4(1.0f))
		{
			const int32 cx = (x >= 0) ? (x / CHUNK_SIZE) : ((x - CHUNK_SIZE + 1) / CHUNK_SIZE);
			const int32 cy = (y >= 0) ? (y / CHUNK_SIZE) : ((y - CHUNK_SIZE + 1) / CHUNK_SIZE);

			const int32 lx = x - cx * CHUNK_SIZE;
			const int32 ly = y - cy * CHUNK_SIZE;

			const uint64_t key = MakeChunkKey(cx, cy);
			auto& chunk = chunks[key];
			const int32 idx = ly * CHUNK_SIZE + lx;

			chunk.tiles[idx].sprite = sprite;
			chunk.tiles[idx].color = color;
			chunk.hasTiles = true;
			chunk.isDirty = true;
		}

		const TileCell* GetTile(int32 x, int32 y) const
		{
			const int32 cx = (x >= 0) ? (x / CHUNK_SIZE) : ((x - CHUNK_SIZE + 1) / CHUNK_SIZE);
			const int32 cy = (y >= 0) ? (y / CHUNK_SIZE) : ((y - CHUNK_SIZE + 1) / CHUNK_SIZE);

			const int32 lx = x - cx * CHUNK_SIZE;
			const int32 ly = y - cy * CHUNK_SIZE;

			const uint64_t key = MakeChunkKey(cx, cy);
			auto it = chunks.find(key);
			if (it == chunks.end()) return nullptr;

			const int32 idx = ly * CHUNK_SIZE + lx;
			if (!it->second.tiles[idx].sprite.IsValid()) return nullptr;
			return &it->second.tiles[idx];
		}

		void ClearTile(int32 x, int32 y)
		{
			SetTile(x, y, {});
		}

		void ClearAll()
		{
			chunks.clear();
		}

		vec2 CellToWorld(int32 x, int32 y) const
		{
			return vec2(static_cast<f32>(x) * cellSize.x, static_cast<f32>(y) * cellSize.y);
		}

		vec2 WorldToCell(const vec2& worldPos) const
		{
			return vec2(
				std::floor(worldPos.x / cellSize.x),
				std::floor(worldPos.y / cellSize.y));
		}
	};
}
