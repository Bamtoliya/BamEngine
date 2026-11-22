#include "SpriteBatcher.h"
#include "Renderer.h"
#include <algorithm>

namespace Engine
{
	void SpriteBatcher::Begin()
	{
		for (auto& [key, bucket] : m_Buckets)
		{
			bucket.vertices.clear();
			bucket.indices.clear();
			bucket.commands.clear();
			bucket.quadCount = 0;
		}
		m_TotalQuadCount = 0;
		m_FlushedCommands.clear();
	}

	void SpriteBatcher::DrawQuad(
		MaterialInterface* material,
		Texture* texture,
		const mat4& worldMatrix,
		const vec2& size,
		const vec2& pivot,
		const vec4& uvTransform,
		const vec4& color,
		float pickingId,
		uint32 passID,
		int32 sortingLayerID,
		int32 orderInLayer,
		Mesh* mesh)
	{
		if (!material || !texture) return;

		const SpriteBatchKey key{ sortingLayerID, orderInLayer, material, texture, mesh, passID };
		auto& bucket = m_Buckets[key];

		const vec2 origin = -pivot * size;
		const vec3 localCorners[4] = {
			vec3(origin.x, origin.y, 0.0f),
			vec3(origin.x + size.x, origin.y, 0.0f),
			vec3(origin.x + size.x, origin.y + size.y, 0.0f),
			vec3(origin.x, origin.y + size.y, 0.0f)
		};

		const vec2 uvs[4] = {
			vec2(uvTransform.x, uvTransform.y + uvTransform.w),
			vec2(uvTransform.x + uvTransform.z, uvTransform.y + uvTransform.w),
			vec2(uvTransform.x + uvTransform.z, uvTransform.y),
			vec2(uvTransform.x, uvTransform.y)
		};

		const uint32 baseIndex = static_cast<uint32>(bucket.vertices.size());

		for (int i = 0; i < 4; ++i)
		{
			const vec4 worldPos = worldMatrix * vec4(localCorners[i], 1.0f);
			SpriteBatchVertex v;
			v.position = vec3(worldPos);
			v.color = color;
			v.texCoord = uvs[i];
			v.pickingId = pickingId;
			bucket.vertices.push_back(v);
		}

		bucket.indices.push_back(baseIndex + 0);
		bucket.indices.push_back(baseIndex + 1);
		bucket.indices.push_back(baseIndex + 2);
		bucket.indices.push_back(baseIndex + 2);
		bucket.indices.push_back(baseIndex + 3);
		bucket.indices.push_back(baseIndex + 0);

		// 인스턴스 렌더링용 SpriteDrawCommand 생성
		const vec2 center = origin + size * 0.5f;
		const mat4 localQuad = glm::translate(mat4(1.0f), vec3(center, 0.0f)) *
		                       glm::scale(mat4(1.0f), vec3(size * 0.5f, 1.0f));

		SpriteDrawCommand cmd{};
		cmd.mesh = mesh;
		cmd.texture = texture;
		cmd.material = material;
		cmd.worldMatrix = worldMatrix * localQuad;
		cmd.color = color;
		cmd.uvTransform = uvTransform;
		cmd.pickingId = static_cast<uint32>(pickingId);
		cmd.sortingLayerID = sortingLayerID;
		cmd.orderInLayer = orderInLayer;
		bucket.commands.push_back(cmd);

		bucket.quadCount++;
		m_TotalQuadCount++;
	}

	void SpriteBatcher::DrawSprite(const SpriteDrawCommand& command, uint32 passID)
	{
		if (!command.material || !command.texture) return;

		const SpriteBatchKey key{
			command.sortingLayerID,
			command.orderInLayer,
			command.material,
			command.texture,
			command.mesh,
			passID
		};
		auto& bucket = m_Buckets[key];
		bucket.commands.push_back(command);
		bucket.quadCount++;
		m_TotalQuadCount++;
	}

	void SpriteBatcher::Flush()
	{
		if (m_Buckets.empty())
		{
			m_TotalQuadCount = 0;
			return;
		}

		struct BucketEntry
		{
			SpriteBatchKey key;
			SpriteBatchBucket bucket;
		};

		std::vector<BucketEntry> sortedBuckets;
		sortedBuckets.reserve(m_Buckets.size());
		for (auto& [key, bucket] : m_Buckets)
		{
			sortedBuckets.push_back({ key, std::move(bucket) });
		}

		// (sortingLayerID, orderInLayer, material, texture, mesh) 순서로 버킷 정렬
		std::sort(sortedBuckets.begin(), sortedBuckets.end(), [](const BucketEntry& a, const BucketEntry& b) {
			if (a.key.sortingLayerID != b.key.sortingLayerID)
				return a.key.sortingLayerID < b.key.sortingLayerID;
			if (a.key.orderInLayer != b.key.orderInLayer)
				return a.key.orderInLayer < b.key.orderInLayer;
			if (a.key.material != b.key.material)
				return a.key.material < b.key.material;
			if (a.key.texture != b.key.texture)
				return a.key.texture < b.key.texture;
			return a.key.mesh < b.key.mesh;
		});

		m_FlushedCommands.clear();
		const bool hasRenderer = Renderer::IsCreated();

		for (auto& entry : sortedBuckets)
		{
			for (auto& cmd : entry.bucket.commands)
			{
				m_FlushedCommands.push_back(cmd);
				if (hasRenderer)
				{
					Renderer::Get().SubmitSprite(cmd, entry.key.passID);
				}
			}
		}

		m_Buckets.clear();
		m_TotalQuadCount = 0;
	}
}
