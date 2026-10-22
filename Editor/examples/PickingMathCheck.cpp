#include "PickingManager.h"
#include "Collision.h"
#include "Mesh.h"
#include "Functions.h"
#include <iostream>
#include <limits>

namespace
{
	bool Near(float actual, float expected, float tolerance = 0.001f)
	{
		return std::isfinite(actual) && std::abs(actual - expected) < tolerance;
	}
	bool TryGetViewportUV(const glm::vec2& mouse, const glm::vec2& origin, const glm::vec2& size, glm::vec2& uv)
	{
		return Engine::PickingManager::TryGetViewportUV(mouse, origin, size, uv);
	}
	bool TryBuildRay(const glm::mat4& inverseViewProjection, const glm::vec2& uv, Engine::Ray& ray, float* maxDistance = nullptr)
	{
		return Engine::PickingManager::TryBuildRay(inverseViewProjection, uv, ray, maxDistance);
	}
	bool RaycastBounds(const Engine::Ray& ray, const Engine::AABB& bounds, const glm::mat4& world, float maxDistance, float& distance)
	{
		Engine::HitResult hit{};
		if (!Engine::Collision::Raycast(ray, bounds, world, hit, maxDistance)) return false;
		distance = hit.distance;
		return hit.hasHit;
	}
	int Fail(const char* message)
	{
		std::cerr << message << '\n';
		return 1;
	}
	bool CheckMeshBVH()
	{
		std::array<glm::vec3, 48> positions{};
		for (Engine::uint32 i = 0; i < 16; ++i)
		{
			const float x = i % 2 == 0 ? -10.0f : 10.0f;
			positions[i * 3] = { x, 0.0f, 0.0f };
			positions[i * 3 + 1] = { x + 1.0f, 0.0f, 0.0f };
			positions[i * 3 + 2] = { x, 1.0f, 0.0f };
		}
		Engine::MeshCreateDesc desc{};
		desc.Streams[(Engine::uint32)Engine::EMeshStream::Position] = {
			positions.data(), static_cast<Engine::uint32>(positions.size()), sizeof(glm::vec3)
		};
		auto* mesh = Engine::Mesh::Create(&desc);
		if (!mesh) return false;

		const auto& initial = mesh->GetRaycastBVH();
		bool ok = initial.Triangles.size() == 16 && initial.Nodes.size() > 1;
		if (ok)
		{
			const auto root = initial.Nodes.front();
			ok = root.Count == 0 && root.Left < initial.Nodes.size() && root.Right < initial.Nodes.size();
			if (ok)
				ok = initial.Nodes[root.Left].BoundsMax.x < initial.Nodes[root.Right].BoundsMin.x;
			for (const auto& triangle : initial.Triangles)
				ok = ok && triangle.SourceIndex < 16 &&
				triangle.Indices[0] == triangle.SourceIndex * 3 &&
				triangle.Indices[1] == triangle.SourceIndex * 3 + 1 &&
				triangle.Indices[2] == triangle.SourceIndex * 3 + 2;
		}

		for (auto& position : positions) position.x += 30.0f;
		const auto moved = mesh->SetStreamBuffer(Engine::EMeshStream::Position,
			positions.data(), static_cast<Engine::uint32>(positions.size()), sizeof(glm::vec3));
		const auto& rebuilt = mesh->GetRaycastBVH();
		ok = ok && Engine::IsSuccess(moved) && !rebuilt.Nodes.empty() &&
			Near(rebuilt.Nodes.front().BoundsMin.x, 20.0f) && Near(rebuilt.Nodes.front().BoundsMax.x, 41.0f) &&
			Near(mesh->GetMin().x, 20.0f) && Near(mesh->GetMax().x, 41.0f);

		const Engine::AABB bounds(mesh->GetMin(), mesh->GetMax());
		const Engine::Ray movedRay({ 20.25f, 0.25f, -5.0f }, { 0.0f, 0.0f, 1.0f });
		const Engine::Ray oldRay({ -9.75f, 0.25f, -5.0f }, { 0.0f, 0.0f, 1.0f });
		Engine::HitResult boundsHit{};
		ok = ok && Engine::Collision::Raycast(movedRay, bounds, boundsHit, 5.0f) &&
			Near(boundsHit.distance, 5.0f);
		ok = ok && !Engine::Collision::Raycast(oldRay, bounds, boundsHit, 5.0f);

		const auto previousVersion = mesh->GetGeometryVersion();
		auto invalidPositions = positions;
		invalidPositions[0].x = std::numeric_limits<float>::quiet_NaN();
		const auto invalidPosition = mesh->SetStreamBuffer(Engine::EMeshStream::Position,
			invalidPositions.data(), static_cast<Engine::uint32>(invalidPositions.size()), sizeof(glm::vec3));
		ok = ok && invalidPosition == Engine::EResult::InvalidArgument &&
			mesh->GetGeometryVersion() == previousVersion &&
			Near(mesh->GetMin().x, 20.0f) && Near(mesh->GetMax().x, 41.0f);

		std::array<Engine::uint32, 3> indices32{ 0, 1, 2 };
		const auto indexed32 = mesh->SetIndexBuffer(indices32.data(), 3, sizeof(Engine::uint32));
		const auto& first = mesh->GetRaycastBVH();
		ok = ok && Engine::IsSuccess(indexed32) && first.Triangles.size() == 1 &&
			!first.Nodes.empty() && Near(first.Nodes.front().BoundsMin.x, 20.0f);

		std::array<Engine::uint16, 3> indices16{ 45, 46, 47 };
		const auto indexed16 = mesh->SetIndexBuffer(indices16.data(), 3, sizeof(Engine::uint16));
		const auto& last = mesh->GetRaycastBVH();
		ok = ok && Engine::IsSuccess(indexed16) && last.Triangles.size() == 1 &&
			!last.Nodes.empty() && Near(last.Nodes.front().BoundsMin.x, 40.0f);

		indices16[2] = 9999;
		const auto indexVersion = mesh->GetGeometryVersion();
		const auto indexRaw = mesh->GetRawIndexData();
		const Engine::Ray retainedRay({ 40.25f, 0.25f, -5.0f }, { 0.0f, 0.0f, 1.0f });

		const auto rejectsIndexUpdate = [&](Engine::uint32 count, Engine::uint32 stride) -> bool
			{
				const auto result = mesh->SetIndexBuffer(indices16.data(), count, stride);
				Engine::HitResult hit{};
				return result == Engine::EResult::InvalidArgument &&
					mesh->GetGeometryVersion() == indexVersion &&
					mesh->GetIndexCount() == 3 && mesh->GetIndexStride() == sizeof(Engine::uint16) &&
					mesh->GetRawIndexData() == indexRaw &&
					Engine::Collision::Raycast(retainedRay, *mesh, glm::mat4(1.0f), hit, 5.0f) &&
					Near(hit.distance, 5.0f);
			};

		constexpr Engine::uint32 overflowCount =
			std::numeric_limits<Engine::uint32>::max() / sizeof(Engine::uint32) + 1;

		ok = ok && rejectsIndexUpdate(3, 0) && rejectsIndexUpdate(3, 1) &&
			rejectsIndexUpdate(3, 3) && rejectsIndexUpdate(overflowCount, sizeof(Engine::uint32));
		const auto invalid = mesh->SetIndexBuffer(indices16.data(), 3, sizeof(Engine::uint16));
		const auto& rejected = mesh->GetRaycastBVH();
		ok = ok && Engine::IsSuccess(invalid) && rejected.Nodes.empty() && rejected.Triangles.empty();

		mesh->Free();
		delete mesh;
		return ok;
	}

	bool CheckMeshCreateValidation()
	{
		std::array<glm::vec3, 3> positions{
			glm::vec3(0.0f, 0.0f, 0.0f),
			glm::vec3(1.0f, 0.0f, 0.0f),
			glm::vec3(0.0f, 1.0f, 0.0f)
		};
		std::array<Engine::uint32, 3> indices{ 0, 1, 2 };
		constexpr auto positionSlot = static_cast<Engine::uint32>(Engine::EMeshStream::Position);

		Engine::MeshCreateDesc desc{};
		desc.Streams[positionSlot] = { positions.data(), 3, sizeof(glm::vec3) };
		desc.IndexData = indices.data();
		desc.IndexCount = 3;
		desc.IndexStride = sizeof(Engine::uint32);

		auto* mesh = Engine::Mesh::Create(&desc);
		if (!mesh) return false;
		mesh->Free();
		delete mesh;

		const auto rejects = [](Engine::MeshCreateDesc invalid) -> bool
			{
				auto* created = Engine::Mesh::Create(&invalid);
				if (!created) return true;
				created->Free();
				delete created;
				return false;
			};

		auto invalid = desc;
		invalid.IndexStride = 3;
		bool ok = rejects(invalid);

		invalid = desc;
		invalid.IndexCount = std::numeric_limits<Engine::uint32>::max() / sizeof(Engine::uint32) + 1;
		ok = ok && rejects(invalid);

		invalid = desc;
		invalid.Streams[positionSlot].Stride = sizeof(float);
		ok = ok && rejects(invalid);

		invalid = desc;
		invalid.Streams[positionSlot].Count = std::numeric_limits<Engine::uint32>::max() / sizeof(glm::vec3) + 1;
		return ok && rejects(invalid);
	}

	bool CheckBVHRaycast()
	{
		std::array<glm::vec3, 48> positions{};
		for (Engine::uint32 i = 0; i < 16; ++i)
		{
			const float x = i % 2 == 0 ? -10.0f : 10.0f;
			const float z = i % 2 == 0 ? 0.0f : 1.0f;
			positions[i * 3] = { x, 0.0f, z };
			positions[i * 3 + 1] = { x + 1.0f, 0.0f, z };
			positions[i * 3 + 2] = { x, 1.0f, z };
		}
		std::swap(positions[43], positions[44]);

		Engine::MeshCreateDesc desc{};
		desc.Streams[(Engine::uint32)Engine::EMeshStream::Position] = {
			positions.data(), static_cast<Engine::uint32>(positions.size()), sizeof(glm::vec3)
		};
		auto* mesh = Engine::Mesh::Create(&desc);
		if (!mesh) return false;

		const auto verify = [&](const Engine::Ray& ray, const glm::mat4& matrix, float limit) -> bool
			{
				Engine::HitResult expected{};
				expected.Reset();
				for (std::size_t i = 0; i < positions.size(); i += 3)
				{
					Engine::HitResult hit{};
					const auto a = glm::vec3(matrix * glm::vec4(positions[i], 1.0f));
					const auto b = glm::vec3(matrix * glm::vec4(positions[i + 1], 1.0f));
					const auto c = glm::vec3(matrix * glm::vec4(positions[i + 2], 1.0f));
					if (Engine::Collision::Raycast(ray, a, b, c, hit, expected.hasHit ? expected.distance : limit))
						expected = hit;
				}

				Engine::HitResult actual{};
				const bool hit = Engine::Collision::Raycast(ray, *mesh, matrix, actual, limit);
				if (hit != expected.hasHit) return false;
				if (!hit) return !actual.hasHit && actual.distance == 0.0f;
				return Near(actual.distance, expected.distance) &&
					glm::length(actual.point - expected.point) < 0.001f &&
					glm::length(actual.normal - expected.normal) < 0.001f &&
					glm::length(actual.barycentric - expected.barycentric) < 0.001f;
			};

		const glm::mat4 identity(1.0f);
		const Engine::Ray ray({ -9.75f, 0.25f, -5.0f }, { 0.0f, 0.0f, 2.0f });
		bool ok = verify(ray, identity, 20.0f) && verify(ray, identity, 5.0f) &&
			verify(ray, identity, 4.0f) &&
			verify({ { -10.0f, 0.0f, -5.0f }, { 0.0f, 0.0f, 1.0f } }, identity, 20.0f) &&
			verify({ { -9.1f, 0.9f, -5.0f }, { 0.0f, 0.0f, 1.0f } }, identity, 20.0f) &&
			verify({ { -9.75f, 0.25f, 0.0f }, { 0.0f, 0.0f, 1.0f } }, identity, 0.0f);

		const glm::mat4 scaled = glm::translate(identity, { 2.0f, -1.0f, 7.0f }) *
			glm::scale(identity, { -2.0f, 3.0f, 0.5f });
		const Engine::Ray transformed(glm::vec3(scaled * glm::vec4(ray.origin, 1.0f)),
			glm::vec3(scaled * glm::vec4(ray.direction, 0.0f)));
		ok = ok && verify(transformed, scaled, 20.0f);

		for (auto& position : positions) position.x += 30.0f;
		const auto moved = mesh->SetStreamBuffer(Engine::EMeshStream::Position,
			positions.data(), static_cast<Engine::uint32>(positions.size()), sizeof(glm::vec3));
		ok = ok && Engine::IsSuccess(moved) && verify(ray, identity, 20.0f) &&
			verify({ { 20.25f, 0.25f, -5.0f }, { 0.0f, 0.0f, 1.0f } }, identity, 20.0f);

		std::array<Engine::uint16, 3> indices{ 45, 46, 47 };
		const auto indexed = mesh->SetIndexBuffer(indices.data(), 3, sizeof(Engine::uint16));
		Engine::HitResult indexedHit{};
		const Engine::Ray indexedRay({ 40.25f, 0.25f, -5.0f }, { 0.0f, 0.0f, 1.0f });
		ok = ok && Engine::IsSuccess(indexed) &&
			Engine::Collision::Raycast(indexedRay, *mesh, identity, indexedHit, 20.0f) &&
			Near(indexedHit.distance, 6.0f);

		mesh->Free();
		delete mesh;
		return ok;
	}
}

int main()
{
	if (!CheckMeshCreateValidation()) return Fail("Mesh creation input validation failed.");
	if (!CheckMeshBVH()) return Fail("Mesh BVH build or rebuild failed.");
	if (!CheckBVHRaycast()) return Fail("BVH raycast differs from triangle reference.");
	const float nan = std::numeric_limits<float>::quiet_NaN();
	glm::vec2 uv;
	if (!TryGetViewportUV({ 300.0f, 150.0f }, { 100.0f, 50.0f }, { 400.0f, 200.0f }, uv) ||
		!Near(uv.x, 0.5f) || !Near(uv.y, 0.5f)) return Fail("Letterboxed image coordinates failed.");
	if (TryGetViewportUV({ 99.0f, 150.0f }, { 100.0f, 50.0f }, { 400.0f, 200.0f }, uv) ||
		TryGetViewportUV({ 500.0f, 150.0f }, { 100.0f, 50.0f }, { 400.0f, 200.0f }, uv) ||
		TryGetViewportUV({ 300.0f, 250.0f }, { 100.0f, 50.0f }, { 400.0f, 200.0f }, uv) ||
		TryGetViewportUV({ 300.0f, 150.0f }, { 100.0f, 50.0f }, { 0.0f, 200.0f }, uv) ||
		TryGetViewportUV({ nan, 150.0f }, { 100.0f, 50.0f }, { 400.0f, 200.0f }, uv))
		return Fail("Image edge/padding/invalid-coordinate rejection failed.");

	Engine::Ray center;
	float maxDistance = 0.0f;
	const glm::mat4 perspective = glm::perspective(glm::radians(60.0f), 2.0f, 0.1f, 100.0f);
	if (!TryBuildRay(glm::inverse(perspective), { 0.5f, 0.5f }, center, &maxDistance) ||
		!Near(center.origin.z, 0.1f) || !Near(center.direction.z, 1.0f) || !Near(maxDistance, 99.9f, 0.02f))
		return Fail("Perspective ray must unproject depth zero at the near plane.");
	Engine::Ray topLeft;
	if (!TryBuildRay(glm::inverse(perspective), { 0.0f, 0.0f }, topLeft) ||
		topLeft.direction.x >= 0.0f || topLeft.direction.y <= 0.0f || topLeft.direction.z <= 0.0f)
		return Fail("Viewport Y direction failed.");
	const glm::mat4 ortho = glm::ortho(-2.0f, 2.0f, -1.0f, 1.0f, 0.1f, 100.0f);
	Engine::Ray orthoA;
	Engine::Ray orthoB;
	if (!TryBuildRay(glm::inverse(ortho), { 0.25f, 0.5f }, orthoA) ||
		!TryBuildRay(glm::inverse(ortho), { 0.75f, 0.5f }, orthoB) ||
		!Near(orthoA.origin.x, -1.0f) || !Near(orthoB.origin.x, 1.0f) ||
		glm::length(orthoA.direction - orthoB.direction) > 0.001f)
		return Fail("Orthographic rays must have different origins and parallel directions.");
	if (TryBuildRay(glm::mat4(0.0f), { 0.5f, 0.5f }, center) ||
		TryBuildRay(glm::mat4(1.0f), { nan, 0.5f }, center)) return Fail("Invalid unprojection was accepted.");

	const Engine::AABB bounds(glm::vec3(-1.0f), glm::vec3(1.0f));
	const glm::mat4 identity(1.0f);
	float distance = 0.0f;
	const Engine::Ray forward({ 0.0f, 0.0f, -5.0f }, { 0.0f, 0.0f, 2.0f });
	if (!RaycastBounds(forward, bounds, identity, 20.0f, distance) || !Near(distance, 4.0f))
		return Fail("Ray distance must be in world units even for an unnormalized input ray.");
	if (!RaycastBounds({ { 1.0f, 0.0f, -5.0f }, { 0.0f, 0.0f, 1.0f } }, bounds, identity, 20.0f, distance) ||
		!Near(distance, 4.0f)) return Fail("Ray on a parallel slab boundary was lost.");
	if (RaycastBounds({ { 2.0f, 0.0f, -5.0f }, { 0.0f, 0.0f, 1.0f } }, bounds, identity, 20.0f, distance) ||
		RaycastBounds(forward, bounds, identity, 3.0f, distance) ||
		RaycastBounds({ { 0.0f, 0.0f, 5.0f }, { 0.0f, 0.0f, 1.0f } }, bounds, identity, 20.0f, distance))
		return Fail("Miss, range limit or behind-origin rejection failed.");
	if (!RaycastBounds({ glm::vec3(0.0f), { 0.0f, 0.0f, 1.0f } }, bounds, identity, 20.0f, distance) ||
		!Near(distance, 0.0f)) return Fail("Ray starting inside bounds should enter at zero distance.");
	const glm::mat4 scaled = glm::translate(identity, { 0.0f, 0.0f, 10.0f }) *
		glm::scale(identity, { -2.0f, 3.0f, 0.5f });
	if (!RaycastBounds(forward, bounds, scaled, 20.0f, distance) || !Near(distance, 14.5f))
		return Fail("Nonuniform/negative scale distance failed.");
	const glm::mat4 rotated = glm::translate(identity, { 0.0f, 0.0f, 10.0f }) *
		glm::rotate(identity, glm::radians(90.0f), { 0.0f, 1.0f, 0.0f }) *
		glm::scale(identity, { 2.0f, 1.0f, 0.5f });
	if (!RaycastBounds(forward, bounds, rotated, 20.0f, distance) || !Near(distance, 13.0f))
		return Fail("Rotated mesh bounds failed.");
	const Engine::AABB quad({ -1.0f, -1.0f, 0.0f }, { 1.0f, 1.0f, 0.0f });
	if (!RaycastBounds(forward, quad, identity, 20.0f, distance) || !Near(distance, 5.0f))
		return Fail("Zero-thickness sprite bounds failed.");
	if (RaycastBounds(forward, bounds, glm::scale(identity, { 1.0f, 0.0f, 1.0f }), 20.0f, distance) ||
		RaycastBounds({ glm::vec3(0.0f), glm::vec3(0.0f) }, bounds, identity, 20.0f, distance) ||
		RaycastBounds(forward, bounds, identity, nan, distance))
		return Fail("Singular transform or invalid ray/range was accepted.");

	Engine::HitResult triHit{};
	const glm::vec3 v0(-1.0f, -1.0f, 0.0f), v1(1.0f, -1.0f, 0.0f), v2(0.0f, 1.0f, 0.0f);
	if (!Engine::Collision::Raycast(forward, v0, v1, v2, triHit, 20.0f) || !Near(triHit.distance, 5.0f) ||
		!Near(triHit.point.z, 0.0f) || !Near(triHit.barycentric.x + triHit.barycentric.y + triHit.barycentric.z, 1.0f))
		return Fail("Triangle raycast hit or barycentric coordinates failed.");
	if (Engine::Collision::Raycast({ { 0.9f, 0.9f, -5.0f }, { 0.0f, 0.0f, 1.0f } }, v0, v1, v2, triHit, 20.0f) ||
		Engine::Collision::Raycast(forward, v0, v1, v2, triHit, 4.0f) ||
		Engine::Collision::Raycast(forward, v0, v0, v2, triHit, 20.0f))
		return Fail("Triangle outside-barycentric, range limit or degenerate triangle rejection failed.");

	const std::array<Engine::VertexPosition, 3> meshPositions = {
		Engine::VertexPosition({ -1.0f, -1.0f, 0.0f }),
		Engine::VertexPosition({  1.0f, -1.0f, 0.0f }),
		Engine::VertexPosition({ -1.0f,  1.0f, 0.0f }),
	};
	const std::array<Engine::uint32, 3> meshIndices = { 0, 1, 2 };
	Engine::MeshCreateDesc meshDesc{};
	meshDesc.Streams[(Engine::uint32)Engine::EMeshStream::Position] = {
		const_cast<Engine::VertexPosition*>(meshPositions.data()),
		static_cast<Engine::uint32>(meshPositions.size()),
		sizeof(Engine::VertexPosition)
	};
	meshDesc.IndexData = const_cast<Engine::uint32*>(meshIndices.data());
	meshDesc.IndexCount = static_cast<Engine::uint32>(meshIndices.size());
	meshDesc.IndexStride = sizeof(Engine::uint32);
	Engine::Mesh* triangleMesh = Engine::Mesh::Create(&meshDesc);
	if (!triangleMesh) return Fail("Failed to create CPU test mesh.");
	Engine::HitResult meshHit{};
	const glm::vec3 hitPoint = glm::vec3(scaled * glm::vec4(-0.5f, -0.5f, 0.0f, 1.0f));
	const Engine::Ray hitCornerRay({ hitPoint.x, hitPoint.y, -5.0f }, { 0.0f, 0.0f, 1.0f });
	const Engine::Ray emptyCornerRay({ 0.5f, 0.5f, -5.0f }, { 0.0f, 0.0f, 1.0f });
	const bool hitTriangle = Engine::Collision::Raycast(hitCornerRay, *triangleMesh, scaled, meshHit, 20.0f);
	Engine::HitResult emptyHit{};
	const bool hitEmptyCorner = Engine::Collision::Raycast(emptyCornerRay, *triangleMesh, identity, emptyHit, 20.0f);
	if (!hitTriangle || !Near(meshHit.distance, 15.0f) || !Near(std::abs(meshHit.normal.z), 1.0f) || hitEmptyCorner)
	{
		triangleMesh->Free();
		triangleMesh = nullptr;
		return Fail("Mesh triangle narrow-phase raycast failed.");
	}
	const glm::mat4 backTransform = glm::translate(identity, { 1.0f, 1.0f, 8.0f });
	if (!Engine::Collision::Raycast(emptyCornerRay, *triangleMesh, backTransform, meshHit, 20.0f) ||
		!Near(meshHit.distance, 13.0f))
	{
		triangleMesh->Free();
		triangleMesh = nullptr;
		return Fail("Back mesh behind empty AABB corner should be hit.");
	}
	triangleMesh->Free();
	triangleMesh = nullptr;

	std::cout << "Picking math checks passed.\n";
	return 0;
}