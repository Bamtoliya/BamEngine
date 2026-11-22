#pragma once

#include "Types.h"
#include "RenderCommands.h"
#include <vector>
#include <unordered_map>
#include <cstdint>

namespace Engine
{
	class MaterialInterface;
	class Texture;
	class Mesh;
	class Renderer;

	struct SpriteBatchVertex
	{
		vec3 position = vec3(0.0f);
		vec4 color = vec4(1.0f);
		vec2 texCoord = vec2(0.0f);
		float pickingId = 0.0f;
	};

	struct SpriteBatchKey
	{
		MaterialInterface* material = nullptr;
		Texture* texture = nullptr;
		uint32 passID = 0;
		int32 sortingLayerID = 0;
		int32 orderInLayer = 0;
		Mesh* mesh = nullptr;

		bool operator==(const SpriteBatchKey& other) const
		{
			return sortingLayerID == other.sortingLayerID &&
			       orderInLayer == other.orderInLayer &&
			       material == other.material &&
			       texture == other.texture &&
			       mesh == other.mesh &&
			       passID == other.passID;
		}
	};
}

namespace std
{
	template<>
	struct hash<Engine::SpriteBatchKey>
	{
		size_t operator()(const Engine::SpriteBatchKey& k) const noexcept
		{
			const size_t h1 = std::hash<int32_t>{}(k.sortingLayerID);
			const size_t h2 = std::hash<int32_t>{}(k.orderInLayer);
			const size_t h3 = std::hash<void*>{}(k.material);
			const size_t h4 = std::hash<void*>{}(k.texture);
			const size_t h5 = std::hash<void*>{}(k.mesh);
			const size_t h6 = std::hash<Engine::uint32>{}(k.passID);
			return h1 ^ (h2 << 1) ^ (h3 << 2) ^ (h4 << 3) ^ (h5 << 4) ^ (h6 << 5);
		}
	};
}

namespace Engine
{
	struct SpriteBatchBucket
	{
		std::vector<SpriteBatchVertex> vertices;
		std::vector<uint32> indices;
		std::vector<SpriteDrawCommand> commands;
		uint32 quadCount = 0;
	};

	class ENGINE_API SpriteBatcher
	{
	public:
		SpriteBatcher() = default;
		~SpriteBatcher() = default;

		void Begin();

		// 쿼드 1개를 배치 버킷에 추가 (월드 행렬 적용 및 정점 4개, 인덱스 6개, 드로우 커맨드 생성)
		void DrawQuad(
			MaterialInterface* material,
			Texture* texture,
			const mat4& worldMatrix,
			const vec2& size,
			const vec2& pivot,
			const vec4& uvTransform,
			const vec4& color,
			float pickingId = 0.0f,
			uint32 passID = 0,
			int32 sortingLayerID = 0,
			int32 orderInLayer = 0,
			Mesh* mesh = nullptr);

		// 스프라이트 드로우 커맨드를 배치 버킷에 추가
		void DrawSprite(const SpriteDrawCommand& command, uint32 passID = 0);
		void Submit(const SpriteDrawCommand& command, uint32 passID = 0) { DrawSprite(command, passID); }

		// 통계 및 조회
		size_t GetBucketCount() const { return m_Buckets.size(); }
		size_t GetTotalQuadCount() const { return m_TotalQuadCount; }
		size_t GetDrawCallSavings() const
		{
			return (m_TotalQuadCount > m_Buckets.size()) ? (m_TotalQuadCount - m_Buckets.size()) : 0;
		}

		const std::unordered_map<SpriteBatchKey, SpriteBatchBucket>& GetBuckets() const { return m_Buckets; }
		const std::vector<SpriteDrawCommand>& GetFlushedCommands() const { return m_FlushedCommands; }

		// (sortingLayerID, orderInLayer, material, texture, mesh) 순서로 버킷을 정렬하여 제출
		void Flush();

	private:
		std::unordered_map<SpriteBatchKey, SpriteBatchBucket> m_Buckets;
		std::vector<SpriteDrawCommand> m_FlushedCommands;
		size_t m_TotalQuadCount = 0;
	};
}
