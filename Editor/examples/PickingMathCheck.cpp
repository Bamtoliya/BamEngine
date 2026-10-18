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
}

int main()
{
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
	const Engine::Ray hitCornerRay({ -0.5f, -0.5f, -5.0f }, { 0.0f, 0.0f, 1.0f });
	const Engine::Ray emptyCornerRay({ 0.5f, 0.5f, -5.0f }, { 0.0f, 0.0f, 1.0f });
	const bool hitTriangle = Engine::Collision::Raycast(hitCornerRay, *triangleMesh, scaled, meshHit, 20.0f);
	const bool hitEmptyCorner = Engine::Collision::Raycast(emptyCornerRay, *triangleMesh, identity, meshHit, 20.0f);
	if (!hitTriangle || !Near(meshHit.distance, 15.0f) || !Near(std::abs(meshHit.normal.z), 1.0f) || hitEmptyCorner)
	{
		Engine::Safe_Release(triangleMesh);
		return Fail("Mesh triangle narrow-phase raycast failed.");
	}
	const glm::mat4 backTransform = glm::translate(identity, { 1.0f, 1.0f, 8.0f });
	if (!Engine::Collision::Raycast(emptyCornerRay, *triangleMesh, backTransform, meshHit, 20.0f) ||
		!Near(meshHit.distance, 13.0f))
	{
		Engine::Safe_Release(triangleMesh);
		return Fail("Back mesh behind empty AABB corner should be hit.");
	}
	Engine::Safe_Release(triangleMesh);

	std::cout << "Picking math checks passed.\n";
	return 0;
}