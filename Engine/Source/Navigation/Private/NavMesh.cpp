#include "NavMesh.h"
#include "Scene.h"
#include "ColliderComponent.h"
#include "TerrainComponent.h"
#include "TransformComponent.h"
#include "WorldTransformComponent.h"
#include "FlagComponent.h"
#include "ComponentEnable.h"
#include "PlatformPath.h"
#include "Functions.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <limits>
#include <queue>
#include <system_error>
#include <unordered_map>

namespace
{
	struct QuantizedVertexKey
	{
		int64 x = 0;
		int64 y = 0;
		int64 z = 0;

		bool operator==(const QuantizedVertexKey& other) const = default;
	};

	struct QuantizedVertexHash
	{
		size_t operator()(const QuantizedVertexKey& k) const noexcept
		{
			size_t h = static_cast<size_t>(k.x) * 73856093ULL;
			h ^= static_cast<size_t>(k.y) * 19349663ULL;
			h ^= static_cast<size_t>(k.z) * 83492791ULL;
			return h;
		}
	};

	struct UndirectedEdgeKey
	{
		uint32 a = 0;
		uint32 b = 0;

		UndirectedEdgeKey(uint32 u, uint32 v)
			: a(std::min(u, v)), b(std::max(u, v))
		{
		}

		bool operator==(const UndirectedEdgeKey& other) const = default;
	};

	struct UndirectedEdgeHash
	{
		size_t operator()(const UndirectedEdgeKey& e) const noexcept
		{
			return (static_cast<size_t>(e.a) << 32) ^ static_cast<size_t>(e.b);
		}
	};

	f32 TriArea2DXZ(const vec3& a, const vec3& b, const vec3& c)
	{
		return (b.x - a.x) * (c.z - a.z) - (c.x - a.x) * (b.z - a.z);
	}

	vec3 ClosestPointOnSegment3D(const vec3& p, const vec3& a, const vec3& b)
	{
		const vec3 ab = b - a;
		const f32 lenSq = glm::dot(ab, ab);
		if (lenSq <= 1e-8f)
		{
			return a;
		}
		const f32 t = std::clamp(glm::dot(p - a, ab) / lenSq, 0.0f, 1.0f);
		return a + ab * t;
	}

	vec3 ClosestPointOnTriangle3D(const vec3& p, const vec3& a, const vec3& b, const vec3& c)
	{
		const vec3 ab = b - a;
		const vec3 ac = c - a;
		const vec3 ap = p - a;
		const f32 d1 = glm::dot(ab, ap);
		const f32 d2 = glm::dot(ac, ap);
		if (d1 <= 0.0f && d2 <= 0.0f) return a;

		const vec3 bp = p - b;
		const f32 d3 = glm::dot(ab, bp);
		const f32 d4 = glm::dot(ac, bp);
		if (d3 >= 0.0f && d4 <= d3) return b;

		const f32 vc = d1 * d4 - d3 * d2;
		if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f)
		{
			const f32 v = d1 / (d1 - d3);
			return a + v * ab;
		}

		const vec3 cp = p - c;
		const f32 d5 = glm::dot(ab, cp);
		const f32 d6 = glm::dot(ac, cp);
		if (d6 >= 0.0f && d5 <= d6) return c;

		const f32 vb = d5 * d2 - d1 * d6;
		if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f)
		{
			const f32 w = d2 / (d2 - d6);
			return a + w * ac;
		}

		const f32 va = d3 * d6 - d5 * d4;
		if (va <= 0.0f && (d4 - d3) >= 0.0f && (d5 - d6) >= 0.0f)
		{
			const f32 w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
			return b + w * (c - b);
		}

		const f32 denom = 1.0f / (va + vb + vc);
		const f32 v = vb * denom;
		const f32 w = vc * denom;
		return a + ab * v + ac * w;
	}

	bool AreBoundaryEdgesConnectable(
		const vec3& a0, const vec3& a1,
		const vec3& b0, const vec3& b1,
		f32 edgeTolerance, f32 maxStepHeight)
	{
		const vec2 a0xz(a0.x, a0.z);
		const vec2 a1xz(a1.x, a1.z);
		const vec2 b0xz(b0.x, b0.z);
		const vec2 b1xz(b1.x, b1.z);

		const vec2 dirA = a1xz - a0xz;
		const f32 lenA = glm::length(dirA);
		const vec2 dirB = b1xz - b0xz;
		const f32 lenB = glm::length(dirB);
		if (lenA <= 1e-4f || lenB <= 1e-4f)
		{
			return false;
		}

		const vec2 uA = dirA / lenA;
		const vec2 uB = dirB / lenB;
		if (std::abs(glm::dot(uA, uB)) < 0.9f)
		{
			return false;
		}

		// B의 두 끝점을 A 선분(XZ)에 투영해 수직 거리와 겹침 길이를 검사한다.
		const auto projectOnA = [&](const vec2& p, f32& outAlong, f32& outPerp)
		{
			const vec2 rel = p - a0xz;
			outAlong = glm::dot(rel, uA);
			outPerp = std::abs(rel.x * uA.y - rel.y * uA.x);
		};

		f32 t0 = 0.0f, d0 = 0.0f;
		f32 t1 = 0.0f, d1 = 0.0f;
		projectOnA(b0xz, t0, d0);
		projectOnA(b1xz, t1, d1);
		if (d0 > edgeTolerance || d1 > edgeTolerance)
		{
			return false;
		}

		const f32 overlapMin = std::max(0.0f, std::min(t0, t1));
		const f32 overlapMax = std::min(lenA, std::max(t0, t1));
		if (overlapMax - overlapMin <= std::max(1e-3f, edgeTolerance * 0.25f))
		{
			return false;
		}

		// 겹치는 구간의 중앙에서 높이 차이를 비교한다.
		const f32 midAlong = 0.5f * (overlapMin + overlapMax);
		const f32 fracA = midAlong / lenA;
		const f32 yA = glm::mix(a0.y, a1.y, fracA);

		const f32 spanB = t1 - t0;
		const f32 fracB = std::abs(spanB) > 1e-5f ? std::clamp((midAlong - t0) / spanB, 0.0f, 1.0f) : 0.5f;
		const f32 yB = glm::mix(b0.y, b1.y, fracB);

		return std::abs(yA - yB) <= maxStepHeight;
	}

	constexpr std::array<char, 8> NavMeshMagic{ 'B', 'A', 'M', 'N', 'A', 'V', 'M', '\0' };
	constexpr uint32 NavMeshFileVersion = 2;

	struct NavMeshFileHeader
	{
		std::array<char, 8> Magic{};
		uint32 Version = 0;
		NavMeshBuildSettings Settings{};
		f32 BoundsMin[3]{};
		f32 BoundsMax[3]{};
		uint32 VertexCount = 0;
		uint32 PolygonCount = 0;
	};
}

void NavMeshBuildInput::Clear()
{
	vertices.clear();
	indices.clear();
	triangleCosts.clear();
}

void NavMeshBuildInput::AddTriangle(const vec3& a, const vec3& b, const vec3& c, f32 cost)
{
	const uint32 base = static_cast<uint32>(vertices.size());
	vertices.push_back(a);
	vertices.push_back(b);
	vertices.push_back(c);
	indices.push_back(base);
	indices.push_back(base + 1);
	indices.push_back(base + 2);
	triangleCosts.push_back(std::max(1.0f, cost));
}

void NavMeshBuildInput::AddQuad(const vec3& a, const vec3& b, const vec3& c, const vec3& d, f32 cost)
{
	const uint32 base = static_cast<uint32>(vertices.size());
	vertices.push_back(a);
	vertices.push_back(b);
	vertices.push_back(c);
	vertices.push_back(d);
	indices.push_back(base);
	indices.push_back(base + 1);
	indices.push_back(base + 2);
	indices.push_back(base);
	indices.push_back(base + 2);
	indices.push_back(base + 3);
	const f32 clampedCost = std::max(1.0f, cost);
	triangleCosts.push_back(clampedCost);
	triangleCosts.push_back(clampedCost);
}

void NavMeshBuildInput::AddBox(const vec3& center, const vec3& halfExtents, const mat4& transform, f32 cost)
{
	const vec3 corners[8] = {
		center + vec3(-halfExtents.x, -halfExtents.y, -halfExtents.z),
		center + vec3( halfExtents.x, -halfExtents.y, -halfExtents.z),
		center + vec3( halfExtents.x, -halfExtents.y,  halfExtents.z),
		center + vec3(-halfExtents.x, -halfExtents.y,  halfExtents.z),
		center + vec3(-halfExtents.x,  halfExtents.y, -halfExtents.z),
		center + vec3( halfExtents.x,  halfExtents.y, -halfExtents.z),
		center + vec3( halfExtents.x,  halfExtents.y,  halfExtents.z),
		center + vec3(-halfExtents.x,  halfExtents.y,  halfExtents.z)
	};

	vec3 worldCorners[8]{};
	for (int i = 0; i < 8; ++i)
	{
		worldCorners[i] = vec3(transform * vec4(corners[i], 1.0f));
	}

	// 윗면 (+Y, 외향 CCW)
	AddQuad(worldCorners[4], worldCorners[7], worldCorners[6], worldCorners[5], cost);
	// 아랫면 (-Y)
	AddQuad(worldCorners[0], worldCorners[1], worldCorners[2], worldCorners[3], cost);
	// 앞면 (+Z)
	AddQuad(worldCorners[3], worldCorners[2], worldCorners[6], worldCorners[7], cost);
	// 뒷면 (-Z)
	AddQuad(worldCorners[1], worldCorners[0], worldCorners[4], worldCorners[5], cost);
	// 오른면 (+X)
	AddQuad(worldCorners[2], worldCorners[1], worldCorners[5], worldCorners[6], cost);
	// 왼면 (-X)
	AddQuad(worldCorners[0], worldCorners[3], worldCorners[7], worldCorners[4], cost);
}

bool NavMeshBuildInput::CollectFromScene(const Scene& scene)
{
	Clear();
	const auto& world = scene.GetRegistry();

	// 1. 지형(TerrainComponent) 메시 수집
	auto terrainView = world.view<TerrainComponent>();
	for (auto entity : terrainView)
	{
		if (!IsComponentEnabled<TerrainComponent>(world, entity))
		{
			continue;
		}
		if (const auto* flag = world.try_get<FlagComponent>(entity))
		{
			if (!flag->IsActiveInHierarchy() || HasFlag(flag->effectiveFlags, EEntityFlag::Dead))
			{
				continue;
			}
		}

		const auto& terrain = terrainView.get<TerrainComponent>(entity);
		const uint32 res = terrain.Resolution;
		const uint32 axisCount = res + 1;
		if (res == 0 || terrain.HeightData.size() < static_cast<size_t>(axisCount) * axisCount)
		{
			continue;
		}

		mat4 worldMatrix = mat4(1.0f);
		if (const auto* wt = world.try_get<WorldTransformComponent>(entity))
		{
			worldMatrix = wt->worldMatrix;
		}
		else if (const auto* t = world.try_get<TransformComponent>(entity))
		{
			worldMatrix = t->GetLocalMatrix();
		}

		const f32 halfW = terrain.Width * 0.5f;
		const f32 halfL = terrain.Length * 0.5f;
		const f32 stepX = terrain.Width / static_cast<f32>(res);
		const f32 stepZ = terrain.Length / static_cast<f32>(res);

		const uint32 baseVertex = static_cast<uint32>(vertices.size());
		vertices.reserve(vertices.size() + static_cast<size_t>(axisCount) * axisCount);

		for (uint32 z = 0; z < axisCount; ++z)
		{
			const f32 lz = -halfL + static_cast<f32>(z) * stepZ;
			for (uint32 x = 0; x < axisCount; ++x)
			{
				const f32 lx = -halfW + static_cast<f32>(x) * stepX;
				const f32 ly = terrain.HeightData[static_cast<size_t>(z) * axisCount + x] * terrain.MaxHeight;
				vertices.push_back(vec3(worldMatrix * vec4(lx, ly, lz, 1.0f)));
			}
		}

		const bool hasAttr = terrain.AttributeData.size() == static_cast<size_t>(axisCount) * axisCount;
		for (uint32 z = 0; z < res; ++z)
		{
			for (uint32 x = 0; x < res; ++x)
			{
				const uint32 i00 = baseVertex + z * axisCount + x;
				const uint32 i10 = i00 + 1;
				const uint32 i01 = i00 + axisCount;
				const uint32 i11 = i01 + 1;

				if (hasAttr)
				{
					const size_t a00 = static_cast<size_t>(z) * axisCount + x;
					const uint8 attr = terrain.AttributeData[a00];
					if (attr == static_cast<uint8>(ETerrainAttribute::Blocked) ||
						attr == static_cast<uint8>(ETerrainAttribute::Hole))
					{
						continue;
					}
				}

				indices.push_back(i00);
				indices.push_back(i01);
				indices.push_back(i11);
				indices.push_back(i00);
				indices.push_back(i11);
				indices.push_back(i10);
				triangleCosts.push_back(1.0f);
				triangleCosts.push_back(1.0f);
			}
		}
	}

	// 2. 콜라이더(BoxShape) 메시 수집
	auto colliderView = world.view<ColliderComponent>();
	for (auto entity : colliderView)
	{
		if (!IsComponentEnabled<ColliderComponent>(world, entity))
		{
			continue;
		}
		const auto& col = colliderView.get<ColliderComponent>(entity);
		const auto& s = col.GetSettings();
		if (!s.Enabled || s.IsTrigger)
		{
			continue;
		}
		if (const auto* flag = world.try_get<FlagComponent>(entity))
		{
			if (!flag->IsActiveInHierarchy() || HasFlag(flag->effectiveFlags, EEntityFlag::Dead))
			{
				continue;
			}
		}

		mat4 worldMatrix = mat4(1.0f);
		if (const auto* wt = world.try_get<WorldTransformComponent>(entity))
		{
			worldMatrix = wt->worldMatrix;
		}
		else if (const auto* t = world.try_get<TransformComponent>(entity))
		{
			worldMatrix = t->GetLocalMatrix();
		}

		if (const auto* box = std::get_if<BoxShape>(&col.GetShape()))
		{
			AddBox(col.GetOffset(), box->Size * 0.5f, worldMatrix, 1.0f);
		}
	}

	return !indices.empty();
}

bool DefaultNavMeshBuilder::Build(const NavMeshBuildInput& input, NavMeshData& outData) const
{
	if (input.vertices.empty() || input.indices.size() < 3 || (input.indices.size() % 3) != 0)
	{
		return false;
	}

	outData = NavMeshData{};
	outData.settings = input.settings;

	const f32 weldTol = std::max(1e-5f, input.settings.vertexWeldTolerance);
	const f32 invWeldTol = 1.0f / weldTol;
	const f32 cosMaxSlope = std::cos(glm::radians(std::clamp(input.settings.maxSlopeDegrees, 0.0f, 89.9f)));

	std::unordered_map<QuantizedVertexKey, uint32, QuantizedVertexHash> vertexMap;
	vertexMap.reserve(input.vertices.size());

	const auto getOrInsertVertex = [&](const vec3& v) -> uint32
	{
		const QuantizedVertexKey key{
			static_cast<int64>(std::llround(v.x * invWeldTol)),
			static_cast<int64>(std::llround(v.y * invWeldTol)),
			static_cast<int64>(std::llround(v.z * invWeldTol))
		};
		const auto [it, inserted] = vertexMap.emplace(key, static_cast<uint32>(outData.vertices.size()));
		if (inserted)
		{
			outData.vertices.push_back(v);
		}
		return it->second;
	};

	const size_t triCount = input.indices.size() / 3;
	outData.polygons.reserve(triCount);

	for (size_t t = 0; t < triCount; ++t)
	{
		const uint32 raw0 = input.indices[t * 3 + 0];
		const uint32 raw1 = input.indices[t * 3 + 1];
		const uint32 raw2 = input.indices[t * 3 + 2];
		if (raw0 >= input.vertices.size() || raw1 >= input.vertices.size() || raw2 >= input.vertices.size())
		{
			continue;
		}

		const vec3& p0 = input.vertices[raw0];
		const vec3& p1 = input.vertices[raw1];
		const vec3& p2 = input.vertices[raw2];

		const vec3 crossVec = glm::cross(p1 - p0, p2 - p0);
		const f32 doubleArea = glm::length(crossVec);
		if (doubleArea <= 1e-6f)
		{
			continue;
		}

		const vec3 normal = crossVec / doubleArea;
		if (normal.y < cosMaxSlope)
		{
			continue;
		}

		const uint32 w0 = getOrInsertVertex(p0);
		const uint32 w1 = getOrInsertVertex(p1);
		const uint32 w2 = getOrInsertVertex(p2);
		if (w0 == w1 || w1 == w2 || w2 == w0)
		{
			continue;
		}

		NavMeshPoly poly;
		poly.vertexIndices = { w0, w1, w2 };
		poly.neighbors = { -1, -1, -1 };
		poly.center = (outData.vertices[w0] + outData.vertices[w1] + outData.vertices[w2]) / 3.0f;
		poly.normal = normal;
		poly.cost = (t < input.triangleCosts.size()) ? std::max(1.0f, input.triangleCosts[t]) : 1.0f;
		outData.polygons.push_back(std::move(poly));
	}

	if (outData.polygons.empty())
	{
		return false;
	}

	// 1차 인접성 연결: 정확히 공유된 정점 에지 매칭
	struct EdgeRef
	{
		int32 polyIdx = -1;
		int32 edgeIdx = -1;
	};
	std::unordered_map<UndirectedEdgeKey, EdgeRef, UndirectedEdgeHash> edgeMap;
	edgeMap.reserve(outData.polygons.size() * 3);

	for (int32 p = 0; p < static_cast<int32>(outData.polygons.size()); ++p)
	{
		auto& poly = outData.polygons[static_cast<size_t>(p)];
		const size_t n = poly.vertexIndices.size();
		for (size_t e = 0; e < n; ++e)
		{
			const uint32 u = poly.vertexIndices[e];
			const uint32 v = poly.vertexIndices[(e + 1) % n];
			const UndirectedEdgeKey key(u, v);

			const auto it = edgeMap.find(key);
			if (it == edgeMap.end())
			{
				edgeMap.emplace(key, EdgeRef{ p, static_cast<int32>(e) });
			}
			else if (it->second.polyIdx != p)
			{
				auto& otherPoly = outData.polygons[static_cast<size_t>(it->second.polyIdx)];
				if (otherPoly.neighbors[static_cast<size_t>(it->second.edgeIdx)] == -1 && poly.neighbors[e] == -1)
				{
					poly.neighbors[e] = it->second.polyIdx;
					otherPoly.neighbors[static_cast<size_t>(it->second.edgeIdx)] = p;
				}
			}
		}
	}

	// 2차 인접성 연결: 다층 다리/계단/오버패스 등 분리된 메시 간 허용 단차(maxStepHeight) 경계 에지 포털 연결
	if (input.settings.edgeMatchTolerance > 0.0f)
	{
		const int32 polyCount = static_cast<int32>(outData.polygons.size());
		for (int32 pA = 0; pA < polyCount; ++pA)
		{
			auto& polyA = outData.polygons[static_cast<size_t>(pA)];
			const size_t nA = polyA.vertexIndices.size();
			for (size_t eA = 0; eA < nA; ++eA)
			{
				if (polyA.neighbors[eA] != -1)
				{
					continue;
				}
				const vec3& a0 = outData.vertices[polyA.vertexIndices[eA]];
				const vec3& a1 = outData.vertices[polyA.vertexIndices[(eA + 1) % nA]];

				for (int32 pB = pA + 1; pB < polyCount; ++pB)
				{
					auto& polyB = outData.polygons[static_cast<size_t>(pB)];
					if (std::find(polyA.neighbors.begin(), polyA.neighbors.end(), pB) != polyA.neighbors.end())
					{
						continue;
					}

					const size_t nB = polyB.vertexIndices.size();
					bool connected = false;
					for (size_t eB = 0; eB < nB; ++eB)
					{
						if (polyB.neighbors[eB] != -1)
						{
							continue;
						}
						const vec3& b0 = outData.vertices[polyB.vertexIndices[eB]];
						const vec3& b1 = outData.vertices[polyB.vertexIndices[(eB + 1) % nB]];

						if (AreBoundaryEdgesConnectable(
							a0, a1, b0, b1,
							input.settings.edgeMatchTolerance,
							input.settings.maxStepHeight))
						{
							polyA.neighbors[eA] = pB;
							polyB.neighbors[eB] = pA;
							connected = true;
							break;
						}
					}
					if (connected)
					{
						break;
					}
				}
			}
		}
	}

	// 3차 인접성 연결: 미연결 경계 에지가 다른 폴리곤 면 내부에 maxStepHeight 이내로 착지하는 경우 포털 링크 연결
	{
		const int32 polyCount = static_cast<int32>(outData.polygons.size());
		const f32 maxStep = input.settings.maxStepHeight;

		auto isPointInsideTriXZ = [](const vec3& p, const vec3& a, const vec3& b, const vec3& c) -> bool
			{
				const f32 c0 = TriArea2DXZ(a, b, p);
				const f32 c1 = TriArea2DXZ(b, c, p);
				const f32 c2 = TriArea2DXZ(c, a, p);
				const bool hasNeg = (c0 < -1e-4f) || (c1 < -1e-4f) || (c2 < -1e-4f);
				const bool hasPos = (c0 > 1e-4f) || (c1 > 1e-4f) || (c2 > 1e-4f);
				return !(hasNeg && hasPos);
			};

		auto hasConnection = [](const NavMeshPoly& poly, int32 target) -> bool
			{
				if (std::find(poly.neighbors.begin(), poly.neighbors.end(), target) != poly.neighbors.end())
					return true;
				return std::any_of(poly.extraLinks.begin(), poly.extraLinks.end(),
					[target](const NavMeshPortalLink& link) { return link.targetPoly == target; });
			};

		for (int32 pA = 0; pA < polyCount; ++pA)
		{
			auto& polyA = outData.polygons[static_cast<size_t>(pA)];
			const size_t nA = polyA.vertexIndices.size();
			for (size_t eA = 0; eA < nA; ++eA)
			{
				if (polyA.neighbors[eA] != -1) continue;
				const vec3& a0 = outData.vertices[polyA.vertexIndices[eA]];
				const vec3& a1 = outData.vertices[polyA.vertexIndices[(eA + 1) % nA]];
				const vec3 mid = 0.5f * (a0 + a1);

				for (int32 pB = 0; pB < polyCount; ++pB)
				{
					if (pA == pB || hasConnection(polyA, pB)) continue;
					auto& polyB = outData.polygons[static_cast<size_t>(pB)];
					if (polyB.vertexIndices.size() < 3) continue;

					const vec3& b0 = outData.vertices[polyB.vertexIndices[0]];
					const vec3& b1 = outData.vertices[polyB.vertexIndices[1]];
					const vec3& b2 = outData.vertices[polyB.vertexIndices[2]];
					if (!isPointInsideTriXZ(mid, b0, b1, b2)) continue;

					const vec3 closestOnB = ClosestPointOnTriangle3D(mid, b0, b1, b2);
					if (std::abs(mid.y - closestOnB.y) > maxStep) continue;

					polyA.extraLinks.push_back({ pB, a0, a1 });
					polyB.extraLinks.push_back({ pA, a0, a1 });
					break;
				}
			}
		}
	}

	// 전체 바운딩 박스 계산
	vec3 bMin(std::numeric_limits<f32>::max());
	vec3 bMax(std::numeric_limits<f32>::lowest());
	for (const auto& poly : outData.polygons)
	{
		for (uint32 idx : poly.vertexIndices)
		{
			const vec3& v = outData.vertices[idx];
			bMin = glm::min(bMin, v);
			bMax = glm::max(bMax, v);
		}
	}
	outData.bounds = BoundingBox(bMin, bMax);
	return true;
}

bool NavMesh::Build(const NavMeshBuildInput& input, const INavMeshBuilder* builder)
{
	NavMeshData newData;
	const bool ok = builder ? builder->Build(input, newData) : DefaultNavMeshBuilder{}.Build(input, newData);
	if (!ok)
	{
		return false;
	}
	m_Data = std::move(newData);
	return true;
}

bool NavMesh::BuildFromScene(const Scene& scene, const NavMeshBuildSettings& settings, const INavMeshBuilder* builder)
{
	NavMeshBuildInput input;
	input.settings = settings;
	if (!input.CollectFromScene(scene))
	{
		return false;
	}
	return Build(input, builder);
}

void NavMesh::SetData(NavMeshData data)
{
	m_Data = std::move(data);
}

void NavMesh::Clear()
{
	m_Data = NavMeshData{};
}

vec3 NavMesh::ClosestPointOnPolygon(int32 polyIdx, const vec3& point) const
{
	const auto& poly = m_Data.polygons[static_cast<size_t>(polyIdx)];
	const size_t n = poly.vertexIndices.size();
	if (n < 3)
	{
		return poly.center;
	}

	const vec3& v0 = m_Data.vertices[poly.vertexIndices[0]];
	vec3 bestPoint = ClosestPointOnTriangle3D(
		point, v0, m_Data.vertices[poly.vertexIndices[1]], m_Data.vertices[poly.vertexIndices[2]]);
	vec3 diff = point - bestPoint;
	f32 bestDistSq = glm::dot(diff, diff);

	for (size_t i = 2; i + 1 < n; ++i)
	{
		const vec3 candidate = ClosestPointOnTriangle3D(
			point, v0, m_Data.vertices[poly.vertexIndices[i]], m_Data.vertices[poly.vertexIndices[i + 1]]);
		diff = point - candidate;
		const f32 distSq = glm::dot(diff, diff);
		if (distSq < bestDistSq)
		{
			bestDistSq = distSq;
			bestPoint = candidate;
		}
	}
	return bestPoint;
}

int32 NavMesh::FindClosestPolygon(const vec3& worldPos, vec3* outClosestPoint) const
{
	if (!IsValid())
	{
		return -1;
	}

	int32 bestPoly = -1;
	vec3 bestPoint(0.0f);
	f32 bestDistSq = std::numeric_limits<f32>::max();

	for (int32 i = 0; i < static_cast<int32>(m_Data.polygons.size()); ++i)
	{
		const vec3 closest = ClosestPointOnPolygon(i, worldPos);
		const vec3 diff = worldPos - closest;
		const f32 distSq = glm::dot(diff, diff);
		if (distSq < bestDistSq)
		{
			bestDistSq = distSq;
			bestPoly = i;
			bestPoint = closest;
		}
	}

	if (outClosestPoint && bestPoly != -1)
	{
		*outClosestPoint = bestPoint;
	}
	return bestPoly;
}

bool NavMesh::GetPortalEdge(int32 polyA, int32 polyB, vec3& outLeft, vec3& outRight) const
{
	const auto& pA = m_Data.polygons[static_cast<size_t>(polyA)];
	const size_t n = pA.vertexIndices.size();
	for (size_t e = 0; e < n; ++e)
	{
		if (pA.neighbors[e] == polyB)
		{
			const vec3& v0 = m_Data.vertices[pA.vertexIndices[e]];
			const vec3& v1 = m_Data.vertices[pA.vertexIndices[(e + 1) % n]];
			if (TriArea2DXZ(pA.center, v0, v1) < 0.0f)
			{
				outLeft = v0;
				outRight = v1;
			}
			else
			{
				outLeft = v1;
				outRight = v0;
			}
			return true;
		}
	}

	for (const auto& link : pA.extraLinks)
	{
		if (link.targetPoly == polyB)
		{
			if (TriArea2DXZ(pA.center, link.left, link.right) < 0.0f)
			{
				outLeft = link.left;
				outRight = link.right;
			}
			else
			{
				outLeft = link.right;
				outRight = link.left;
			}
			return true;
		}
	}

	return false;
}

bool NavMesh::FindPath(const vec3& startWorld, const vec3& goalWorld, std::vector<vec3>& outWaypoints) const
{
	outWaypoints.clear();
	if (!IsValid())
	{
		return false;
	}

	vec3 startSnapped(0.0f);
	vec3 goalSnapped(0.0f);
	const int32 startPoly = FindClosestPolygon(startWorld, &startSnapped);
	const int32 goalPoly = FindClosestPolygon(goalWorld, &goalSnapped);
	if (startPoly < 0 || goalPoly < 0)
	{
		return false;
	}

	if (startPoly == goalPoly)
	{
		outWaypoints.push_back(startSnapped);
		if (glm::distance(startSnapped, goalSnapped) > 1e-4f)
		{
			outWaypoints.push_back(goalSnapped);
		}
		return true;
	}

	// 1. 3D 폴리곤 그래프 A* 탐색
	const size_t polyCount = m_Data.polygons.size();
	std::vector<f32> gCost(polyCount, std::numeric_limits<f32>::max());
	std::vector<int32> parent(polyCount, -1);
	std::vector<uint8> closed(polyCount, 0);

	struct PQNode
	{
		int32 poly = -1;
		f32 f = 0.0f;
		bool operator>(const PQNode& other) const { return f > other.f; }
	};
	std::priority_queue<PQNode, std::vector<PQNode>, std::greater<PQNode>> openSet;

	gCost[static_cast<size_t>(startPoly)] = 0.0f;
	const f32 startH = glm::distance(m_Data.polygons[static_cast<size_t>(startPoly)].center, goalSnapped);
	openSet.push({ startPoly, startH });

	bool found = false;
	while (!openSet.empty())
	{
		const PQNode top = openSet.top();
		openSet.pop();
		const int32 curr = top.poly;

		if (curr == goalPoly)
		{
			found = true;
			break;
		}
		if (closed[static_cast<size_t>(curr)])
		{
			continue;
		}
		closed[static_cast<size_t>(curr)] = 1;

		const auto& currPoly = m_Data.polygons[static_cast<size_t>(curr)];
		auto tryRelaxNeighbor = [&](int32 nb)
		{
			if (nb < 0 || static_cast<size_t>(nb) >= polyCount || closed[static_cast<size_t>(nb)])
			{
				return;
			}
			const auto& nbPoly = m_Data.polygons[static_cast<size_t>(nb)];
			const f32 stepCost = glm::distance(currPoly.center, nbPoly.center) * std::max(1.0f, nbPoly.cost);
			const f32 tentativeG = gCost[static_cast<size_t>(curr)] + stepCost;
			if (tentativeG < gCost[static_cast<size_t>(nb)])
			{
				gCost[static_cast<size_t>(nb)] = tentativeG;
				parent[static_cast<size_t>(nb)] = curr;
				const f32 h = glm::distance(nbPoly.center, goalSnapped);
				openSet.push({ nb, tentativeG + h });
			}
		};
		for (int32 nb : currPoly.neighbors)
		{
			tryRelaxNeighbor(nb);
		}
		for (const auto& link : currPoly.extraLinks)
		{
			tryRelaxNeighbor(link.targetPoly);
		}
	}

	if (!found)
	{
		return false;
	}

	std::vector<int32> polyPath;
	for (int32 p = goalPoly; p != -1; p = parent[static_cast<size_t>(p)])
	{
		polyPath.push_back(p);
		if (p == startPoly)
		{
			break;
		}
	}
	std::reverse(polyPath.begin(), polyPath.end());

	// 2. 3D 퍼널(Simple Stupid Funnel) 경로 스무딩
	std::vector<vec3> portalLeft;
	std::vector<vec3> portalRight;
	portalLeft.reserve(polyPath.size() + 1);
	portalRight.reserve(polyPath.size() + 1);

	portalLeft.push_back(startSnapped);
	portalRight.push_back(startSnapped);

	for (size_t i = 0; i + 1 < polyPath.size(); ++i)
	{
		vec3 left(0.0f);
		vec3 right(0.0f);
		if (GetPortalEdge(polyPath[i], polyPath[i + 1], left, right))
		{
			portalLeft.push_back(left);
			portalRight.push_back(right);
		}
	}

	portalLeft.push_back(goalSnapped);
	portalRight.push_back(goalSnapped);

	outWaypoints.push_back(startSnapped);

	vec3 portalApex = startSnapped;
	vec3 leftBound = startSnapped;
	vec3 rightBound = startSnapped;
	size_t apexIndex = 0;
	size_t leftIndex = 0;
	size_t rightIndex = 0;

	auto pushWaypointUnique = [&](const vec3& wp)
	{
		if (outWaypoints.empty() || glm::distance(outWaypoints.back(), wp) > 1e-3f)
		{
			outWaypoints.push_back(wp);
		}
	};

	const auto sameXZ = [](const vec3& a, const vec3& b) -> bool
	{
		return std::abs(a.x - b.x) <= 1e-5f && std::abs(a.z - b.z) <= 1e-5f;
	};

	for (size_t i = 1; i < portalLeft.size(); ++i)
	{
		const vec3& newLeft = portalLeft[i];
		const vec3& newRight = portalRight[i];

		// 오른쪽 경계 갱신
		if (TriArea2DXZ(portalApex, rightBound, newRight) >= 0.0f)
		{
			if (sameXZ(portalApex, rightBound) || TriArea2DXZ(portalApex, leftBound, newRight) < 0.0f)
			{
				rightBound = newRight;
				rightIndex = i;
			}
			else
			{
				pushWaypointUnique(leftBound);
				portalApex = leftBound;
				apexIndex = leftIndex;
				leftBound = portalApex;
				rightBound = portalApex;
				leftIndex = apexIndex;
				rightIndex = apexIndex;
				i = apexIndex;
				continue;
			}
		}

		// 왼쪽 경계 갱신
		if (TriArea2DXZ(portalApex, leftBound, newLeft) <= 0.0f)
		{
			if (sameXZ(portalApex, leftBound) || TriArea2DXZ(portalApex, rightBound, newLeft) > 0.0f)
			{
				leftBound = newLeft;
				leftIndex = i;
			}
			else
			{
				pushWaypointUnique(rightBound);
				portalApex = rightBound;
				apexIndex = rightIndex;
				leftBound = portalApex;
				rightBound = portalApex;
				leftIndex = apexIndex;
				rightIndex = apexIndex;
				i = apexIndex;
				continue;
			}
		}
	}

	pushWaypointUnique(goalSnapped);
	return !outWaypoints.empty();
}

bool NavMesh::SaveToFile(const std::filesystem::path& path) const
{
	if (path.empty() || !IsValid())
	{
		return false;
	}

	std::error_code ec;
	if (path.has_parent_path())
	{
		std::filesystem::create_directories(path.parent_path(), ec);
	}

	auto tempPath = path;
	tempPath += L".tmp";
	{
		std::ofstream file(tempPath, std::ios::binary | std::ios::trunc);
		if (!file)
		{
			return false;
		}

		NavMeshFileHeader header{
			NavMeshMagic,
			NavMeshFileVersion,
			m_Data.settings,
			{ m_Data.bounds.min.x, m_Data.bounds.min.y, m_Data.bounds.min.z },
			{ m_Data.bounds.max.x, m_Data.bounds.max.y, m_Data.bounds.max.z },
			static_cast<uint32>(m_Data.vertices.size()),
			static_cast<uint32>(m_Data.polygons.size())
		};
		file.write(reinterpret_cast<const char*>(&header), sizeof(header));
		file.write(
			reinterpret_cast<const char*>(m_Data.vertices.data()),
			static_cast<std::streamsize>(m_Data.vertices.size() * sizeof(vec3)));

		for (const auto& poly : m_Data.polygons)
		{
			const uint32 vCount = static_cast<uint32>(poly.vertexIndices.size());
			file.write(reinterpret_cast<const char*>(&vCount), sizeof(vCount));
			file.write(
				reinterpret_cast<const char*>(poly.vertexIndices.data()),
				static_cast<std::streamsize>(vCount * sizeof(uint32)));
			file.write(
				reinterpret_cast<const char*>(poly.neighbors.data()),
				static_cast<std::streamsize>(vCount * sizeof(int32)));
			file.write(reinterpret_cast<const char*>(&poly.center), sizeof(vec3));
			file.write(reinterpret_cast<const char*>(&poly.normal), sizeof(vec3));
			file.write(reinterpret_cast<const char*>(&poly.cost), sizeof(f32));
			file.write(reinterpret_cast<const char*>(&poly.area), sizeof(uint8));
			const uint32 extraCount = static_cast<uint32>(poly.extraLinks.size());
			file.write(reinterpret_cast<const char*>(&extraCount), sizeof(extraCount));
			for (const auto& link : poly.extraLinks)
			{
				file.write(reinterpret_cast<const char*>(&link.targetPoly), sizeof(int32));
				file.write(reinterpret_cast<const char*>(&link.left), sizeof(vec3));
				file.write(reinterpret_cast<const char*>(&link.right), sizeof(vec3));
			}
		}

		if (!file.flush())
		{
			std::filesystem::remove(tempPath, ec);
			return false;
		}
	}

	std::filesystem::rename(tempPath, path, ec);
	if (ec)
	{
		std::filesystem::remove(path, ec);
		ec.clear();
		std::filesystem::rename(tempPath, path, ec);
		if (ec)
		{
			std::filesystem::remove(tempPath, ec);
			return false;
		}
	}
	return true;
}

bool NavMesh::LoadFromFile(const std::filesystem::path& path)
{
	if (path.empty())
	{
		return false;
	}

	std::error_code ec;
	const auto fileSize = std::filesystem::file_size(path, ec);
	if (ec || fileSize < sizeof(NavMeshFileHeader))
	{
		return false;
	}

	std::ifstream file(path, std::ios::binary);
	if (!file)
	{
		return false;
	}

	NavMeshFileHeader header{};
	if (!file.read(reinterpret_cast<char*>(&header), sizeof(header)))
	{
		return false;
	}

	if (header.Magic != NavMeshMagic || (header.Version != 1 && header.Version != NavMeshFileVersion) ||
		header.VertexCount == 0 || header.PolygonCount == 0 ||
		header.VertexCount > 5000000 || header.PolygonCount > 5000000)
	{
		return false;
	}

	NavMeshData loaded;
	loaded.settings = header.Settings;
	loaded.bounds = BoundingBox(
		vec3(header.BoundsMin[0], header.BoundsMin[1], header.BoundsMin[2]),
		vec3(header.BoundsMax[0], header.BoundsMax[1], header.BoundsMax[2]));

	loaded.vertices.resize(header.VertexCount);
	if (!file.read(
		reinterpret_cast<char*>(loaded.vertices.data()),
		static_cast<std::streamsize>(loaded.vertices.size() * sizeof(vec3))))
	{
		return false;
	}

	loaded.polygons.resize(header.PolygonCount);
	for (uint32 p = 0; p < header.PolygonCount; ++p)
	{
		uint32 vCount = 0;
		if (!file.read(reinterpret_cast<char*>(&vCount), sizeof(vCount)) || vCount < 3 || vCount > 64)
		{
			return false;
		}
		auto& poly = loaded.polygons[p];
		poly.vertexIndices.resize(vCount);
		poly.neighbors.resize(vCount);
		if (!file.read(reinterpret_cast<char*>(poly.vertexIndices.data()), static_cast<std::streamsize>(vCount * sizeof(uint32))) ||
			!file.read(reinterpret_cast<char*>(poly.neighbors.data()), static_cast<std::streamsize>(vCount * sizeof(int32))) ||
			!file.read(reinterpret_cast<char*>(&poly.center), sizeof(vec3)) ||
			!file.read(reinterpret_cast<char*>(&poly.normal), sizeof(vec3)) ||
			!file.read(reinterpret_cast<char*>(&poly.cost), sizeof(f32)) ||
			!file.read(reinterpret_cast<char*>(&poly.area), sizeof(uint8)))
		{
			return false;
		}

		for (uint32 idx : poly.vertexIndices)
		{
			if (idx >= header.VertexCount)
			{
				return false;
			}
		}
		for (int32 nb : poly.neighbors)
		{
			if (nb < -1 || nb >= static_cast<int32>(header.PolygonCount))
			{
				return false;
			}
		}
		if (header.Version >= 2)
		{
			uint32 extraCount = 0;
			if (!file.read(reinterpret_cast<char*>(&extraCount), sizeof(extraCount)) || extraCount > 64)
			{
				return false;
			}
			poly.extraLinks.resize(extraCount);
			for (uint32 l = 0; l < extraCount; ++l)
			{
				auto& link = poly.extraLinks[l];
				if (!file.read(reinterpret_cast<char*>(&link.targetPoly), sizeof(int32)) ||
					!file.read(reinterpret_cast<char*>(&link.left), sizeof(vec3)) ||
					!file.read(reinterpret_cast<char*>(&link.right), sizeof(vec3)) ||
					link.targetPoly < 0 || link.targetPoly >= static_cast<int32>(header.PolygonCount))
				{
					return false;
				}
			}
		}
	}

	m_Data = std::move(loaded);
	return true;
}

std::filesystem::path NavMesh::MakeDefaultPath(const std::filesystem::path& scenePath)
{
	auto name = scenePath.filename().wstring();
	for (const auto* suffix : { L".bamscene.json", L".json", L".bamscene" })
	{
		const std::wstring_view view(suffix);
		if (name.size() > view.size() && name.ends_with(view))
		{
			name.resize(name.size() - view.size());
			break;
		}
	}
	return scenePath.parent_path() / (name + L"_NavMesh.bamnavmesh");
}

std::filesystem::path NavMesh::ResolveDataPath(const std::string& navMeshUri)
{
	const auto path = Utf8ToPath(navMeshUri);
	return PlatformPath::IsVirtualUri(navMeshUri) ? PlatformPath::ResolveUri(path) : PlatformPath::ResolveProjectRelative(path);
}