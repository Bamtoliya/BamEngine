#pragma once

#include "ComponentAnnotations.h"
#include "ResourceHandle.h"
#include "Sprite.h"
#include <unordered_map>
#include <cstdint>
#include <vector>
#include <limits>

namespace Engine
{
	class Mesh;
	class MaterialInterface;

	struct TileCell
	{
		ResourceHandle<Sprite> sprite;
		vec4 color = vec4(1.0f);
		bool isAutotile = false;
		uint8 connectionMask = 0;
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

	STRUCT()
	struct SavedTile
	{
		REFLECT_BODY()

		PROPERTY(EDITABLE)
		int32 x = 0;

		PROPERTY(EDITABLE)
		int32 y = 0;

		PROPERTY(EDITABLE)
		ResourceHandle<Sprite> sprite;

		PROPERTY(EDITABLE)
		vec4 color = vec4(1.0f);

		PROPERTY(ALLOW_MISSING)
		bool isAutotile = false;
	};

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

		PROPERTY()
		std::vector<SavedTile> savedTiles;

		PROPERTY(ALLOW_MISSING)
		std::vector<ResourceHandle<Sprite>> autotileSprites;

		// 런타임 청크 맵 (청크 좌표 -> 16x16 청크)
		std::unordered_map<uint64_t, TilemapChunk> chunks;

	public:
		void SyncSavedTiles()
		{
			savedTiles.clear();
			for (const auto& [chunkKey, chunk] : chunks)
			{
				if (!chunk.hasTiles) continue;
				const int32 cx = static_cast<int32>(chunkKey >> 32);
				const int32 cy = static_cast<int32>(chunkKey & 0xFFFFFFFF);

				for (int32 ly = 0; ly < CHUNK_SIZE; ++ly)
				{
					for (int32 lx = 0; lx < CHUNK_SIZE; ++lx)
					{
						const int32 idx = ly * CHUNK_SIZE + lx;
						const auto& cell = chunk.tiles[idx];
						if (cell.isAutotile || cell.sprite.IsValid())
						{
							savedTiles.push_back(SavedTile{
								cx * CHUNK_SIZE + lx, cy * CHUNK_SIZE + ly, cell.sprite, cell.color, cell.isAutotile
								});
						}
					}
				}
			}
		}

		void RestoreFromSavedTiles()
		{
			chunks.clear();
			for (const auto& tile : savedTiles)
			{
				if (tile.isAutotile || tile.sprite.IsValid())
					WriteTile(tile.x, tile.y, TileCell{ tile.sprite, tile.color, tile.isAutotile });
			}

			for (const auto& tile : savedTiles)
			{
				if (tile.isAutotile) RefreshAutotile(tile.x, tile.y);
			}
		}
		bool SetTile(int32 x, int32 y, ResourceHandle<Sprite> sprite, vec4 color = vec4(1.0f))
		{
			if (!sprite.IsValid()) return ClearTile(x, y);

			const auto* previous = GetTile(x, y);
			if (previous && !previous->isAutotile && previous->sprite == sprite && previous->color == color)
				return false;

			const bool wasAutotile = previous && previous->isAutotile;
			WriteTile(x, y, TileCell{ sprite, color });
			if (wasAutotile) RefreshAutotileNeighbors(x, y);
			return true;
		}

		bool SetAutotile(int32 x, int32 y)
		{
			if (autotileSprites.empty() || !autotileSprites.front().IsValid()) return false;

			const auto* previous = GetTile(x, y);
			if (previous && previous->isAutotile) return false;

			WriteTile(x, y, TileCell{ {}, vec4(1.0f), true });
			RefreshAutotile(x, y);
			RefreshAutotileNeighbors(x, y);
			return true;
		}

		const TileCell* GetTile(int32 x, int32 y) const
		{
			const auto it = chunks.find(TileChunkKey(x, y));
			if (it == chunks.end()) return nullptr;

			const auto& cell = it->second.tiles[TileIndex(x, y)];
			return (cell.isAutotile || cell.sprite.IsValid()) ? &cell : nullptr;
		}

		bool ClearTile(int32 x, int32 y)
		{
			const auto* previous = GetTile(x, y);
			if (!previous) return false;

			const bool wasAutotile = previous->isAutotile;
			WriteTile(x, y, {});
			if (wasAutotile) RefreshAutotileNeighbors(x, y);
			return true;
		}

		const ResourceHandle<Sprite>& ResolveTileSprite(const TileCell& cell) const
		{
			if (!cell.isAutotile || autotileSprites.empty()) return cell.sprite;

			const auto mask = cell.connectionMask;
			if (mask < autotileSprites.size() && autotileSprites[mask].IsValid())
				return autotileSprites[mask];

			return autotileSprites.front();
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

	private:
		static int32 ChunkCoordinate(int32 coordinate)
		{
			return coordinate / CHUNK_SIZE - (coordinate % CHUNK_SIZE < 0 ? 1 : 0);
		}

		static uint64_t TileChunkKey(int32 x, int32 y)
		{
			return MakeChunkKey(ChunkCoordinate(x), ChunkCoordinate(y));
		}

		static int32 TileIndex(int32 x, int32 y)
		{
			const int32 lx = (x % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;
			const int32 ly = (y % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;
			return ly * CHUNK_SIZE + lx;
		}

		void WriteTile(int32 x, int32 y, const TileCell& cell)
		{
			auto& chunk = chunks[TileChunkKey(x, y)];
			chunk.tiles[TileIndex(x, y)] = cell;
			chunk.hasTiles = true;
			chunk.isDirty = true;
		}

		template<typename F>
		static void ForEachNeighbor(int32 x, int32 y, F visit)
		{
			constexpr int32 dx[] = { 0, 1, 0, -1 };
			constexpr int32 dy[] = { 1, 0, -1, 0 };
			constexpr auto min = (std::numeric_limits<int32>::min)();
			constexpr auto max = (std::numeric_limits<int32>::max)();

			for (int32 i = 0; i < 4; ++i)
			{
				const auto nx = static_cast<std::int64_t>(x) + dx[i];
				const auto ny = static_cast<std::int64_t>(y) + dy[i];
				if (nx < min || nx > max || ny < min || ny > max) continue;
				visit(static_cast<int32>(nx), static_cast<int32>(ny), static_cast<uint8>(1u << i));
			}
		}

		void RefreshAutotile(int32 x, int32 y)
		{
			const auto* cell = GetTile(x, y);
			if (!cell || !cell->isAutotile) return;

			uint8 mask = 0;
			ForEachNeighbor(x, y, [&](int32 nx, int32 ny, uint8 bit)
				{
					const auto* neighbor = GetTile(nx, ny);
					if (neighbor && neighbor->isAutotile) mask |= bit;
				});

			if (mask == cell->connectionMask) return;
			auto updated = *cell;
			updated.connectionMask = mask;
			WriteTile(x, y, updated);
		}

		void RefreshAutotileNeighbors(int32 x, int32 y)
		{
			ForEachNeighbor(x, y, [this](int32 nx, int32 ny, uint8) { RefreshAutotile(nx, ny); });
		}
	};
}
