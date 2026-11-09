#include "SpriteBatcher.h"

namespace Engine
{
	void SpriteBatcher::Begin()
	{
		for (auto& [key, bucket] : m_Buckets)
		{
			bucket.vertices.clear();
			bucket.indices.clear();
			bucket.quadCount = 0;
		}
		m_TotalQuadCount = 0;
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
		uint32 passID)
	{
		if (!material || !texture) return;

		const SpriteBatchKey key{ material, texture, passID };
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

		bucket.quadCount++;
		m_TotalQuadCount++;
	}

	void SpriteBatcher::Flush()
	{
		// 프레임 끝에서 버킷 상태 정리
		m_Buckets.clear();
		m_TotalQuadCount = 0;
	}
}
