#pragma once

#include "Engine_API.h"
#include "Types.h"
#include "BoundingBox.h"
#include <glm/glm.hpp>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

BEGIN(Engine)

class Scene;

struct ENGINE_API NavMeshBuildSettings
{
	f32 maxSlopeDegrees = 45.0f;
	f32 agentRadius = 0.4f;
	f32 agentHeight = 1.8f;
	f32 maxStepHeight = 0.5f;
	f32 vertexWeldTolerance = 1e-3f;
	f32 edgeMatchTolerance = 0.05f;
};

struct ENGINE_API NavMeshBuildInput
{
	std::vector<vec3> vertices;
	std::vector<uint32> indices;
	std::vector<f32> triangleCosts;
	NavMeshBuildSettings settings;

	void Clear();
	void AddTriangle(const vec3& a, const vec3& b, const vec3& c, f32 cost = 1.0f);
	void AddQuad(const vec3& a, const vec3& b, const vec3& c, const vec3& d, f32 cost = 1.0f);
	void AddBox(const vec3& center, const vec3& halfExtents, const mat4& transform = mat4(1.0f), f32 cost = 1.0f);
	bool CollectFromScene(const Scene& scene);
};

struct ENGINE_API NavMeshPortalLink
{
	int32 targetPoly = -1;
	vec3 left = vec3(0.0f);
	vec3 right = vec3(0.0f);
};

struct ENGINE_API NavMeshPoly
{
	std::vector<uint32> vertexIndices;
	std::vector<int32> neighbors;
	std::vector<NavMeshPortalLink> extraLinks;
	vec3 center = vec3(0.0f);
	vec3 normal = vec3(0.0f, 1.0f, 0.0f);
	f32 cost = 1.0f;
	uint8 area = 0;
};

struct ENGINE_API NavMeshData
{
	NavMeshBuildSettings settings;
	std::vector<vec3> vertices;
	std::vector<NavMeshPoly> polygons;
	BoundingBox bounds;
};

// 향후 recastnavigation(Recast/Detour) 빌더로 교체·위임할 수 있는 추상 빌더 인터페이스
class ENGINE_API INavMeshBuilder
{
public:
	virtual ~INavMeshBuilder() = default;
	[[nodiscard]] virtual bool Build(const NavMeshBuildInput& input, NavMeshData& outData) const = 0;
};

// 외부 의존성 없이 동작하는 자체 3D 다층 NavMesh 빌더
class ENGINE_API DefaultNavMeshBuilder : public INavMeshBuilder
{
public:
	[[nodiscard]] bool Build(const NavMeshBuildInput& input, NavMeshData& outData) const override;
};

class ENGINE_API NavMesh
{
public:
	NavMesh() = default;
	~NavMesh() = default;

	[[nodiscard]] bool Build(const NavMeshBuildInput& input, const INavMeshBuilder* builder = nullptr);
	[[nodiscard]] bool BuildFromScene(
		const Scene& scene,
		const NavMeshBuildSettings& settings = {},
		const INavMeshBuilder* builder = nullptr);

	void SetData(NavMeshData data);
	[[nodiscard]] const NavMeshData& GetData() const { return m_Data; }
	[[nodiscard]] bool IsValid() const { return !m_Data.polygons.empty() && !m_Data.vertices.empty(); }
	[[nodiscard]] size_t GetPolygonCount() const { return m_Data.polygons.size(); }
	[[nodiscard]] size_t GetVertexCount() const { return m_Data.vertices.size(); }
	[[nodiscard]] const BoundingBox& GetBounds() const { return m_Data.bounds; }
	void Clear();

	[[nodiscard]] int32 FindClosestPolygon(const vec3& worldPos, vec3* outClosestPoint = nullptr) const;
	[[nodiscard]] bool FindPath(
		const vec3& startWorld,
		const vec3& goalWorld,
		std::vector<vec3>& outWaypoints) const;

	[[nodiscard]] bool SaveToFile(const std::filesystem::path& path) const;
	[[nodiscard]] bool LoadFromFile(const std::filesystem::path& path);
	[[nodiscard]] static std::filesystem::path MakeDefaultPath(const std::filesystem::path& scenePath);
	[[nodiscard]] static std::filesystem::path ResolveDataPath(const std::string& navMeshUri);

private:
	[[nodiscard]] bool GetPortalEdge(int32 polyA, int32 polyB, vec3& outLeft, vec3& outRight) const;
	[[nodiscard]] vec3 ClosestPointOnPolygon(int32 polyIdx, const vec3& point) const;

private:
	NavMeshData m_Data;
};

END
