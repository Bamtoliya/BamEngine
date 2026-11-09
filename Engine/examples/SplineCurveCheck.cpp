#include "SplineCurve.h"
#include "SplineDistanceTable.h"
#include "SplineComponent.h"
#include "SplineFollowerComponent.h"
#include "SplineFollowSystem.h"
#include "TransformComponent.h"
#include "WorldTransformComponent.h"
#include "HierarchyComponent.h"
#include "FlagComponent.h"
#include "SplinePlacement.h"
#include "SplineFrameTable.h"
#include "SplineMeshComponent.h"
#include "SplineMeshDeformer.h"

namespace
{
	using namespace Engine;

	int Fail(const char* message)
	{
		std::cerr << message << '\n';
		return 1;
	}

	bool Near(const vec3& left, const vec3& right, f32 tolerance = 0.0001f)
	{
		return glm::length(left - right) <= tolerance;
	}
	int CheckSplineDistanceTable()
	{
		SplineDistanceTable table;
		f64 parameter = 123.0;

		if (table.TryGetParameter(0.0, parameter) || parameter != 123.0)
			return Fail("An unbuilt distance table accepted a query.");

		// 제어점 간격이 달라도 거리는 실제 위치에 대응해야 합니다.
		const std::array<vec3, 3> uneven{
			vec3(0.0f), vec3(1.0f, 0.0f, 0.0f), vec3(3.0f, 0.0f, 0.0f)
		};

		if (!table.Build(uneven, false, mat4(1.0f), 64) || std::abs(table.GetLength() - 3.0) > 0.0001)
			return Fail("Uneven straight-path length failed.");

		SplineSample sample;
		if (!table.TryGetParameter(1.5, parameter) ||
			!SplineCurve::Evaluate(uneven, false, parameter, sample) ||
			!Near(sample.Position, vec3(1.5f, 0.0f, 0.0f), 0.001f))
			return Fail("Distance-to-parameter conversion failed.");

		if (!table.TryGetParameter(-1.0, parameter) || parameter != 0.0 ||
			!table.TryGetParameter(4.0, parameter) || parameter != 1.0)
			return Fail("Distance endpoint clamping failed.");

		// 비균일 스케일에서는 각 축의 다른 배율을 거리에 반영합니다.
		const std::array<vec3, 2> diagonal{ vec3(0.0f), vec3(3.0f, 4.0f, 0.0f) };
		const mat4 scale = glm::scale(mat4(1.0f), vec3(2.0f, 3.0f, 1.0f));
		const f64 expectedLength = std::sqrt(180.0);

		if (!table.Build(diagonal, false, scale) || std::abs(table.GetLength() - expectedLength) > 0.0001)
			return Fail("Nonuniform world-scale length failed.");

		const mat4 translated = glm::translate(mat4(1.0f), vec3(1.0e20f)) * scale;
		if (!table.Build(diagonal, false, translated) ||
			std::abs(table.GetLength() - expectedLength) > 0.0001 ||
			!table.TryGetParameter(expectedLength * 0.5, parameter) || std::abs(parameter - 0.5) > 0.0001)
			return Fail("Translation changed the distance metric.");

		const f64 previousLength = table.GetLength();
		mat4 invalidTransform(1.0f);
		invalidTransform[0][3] = 0.5f;

		if (table.Build(diagonal, false, invalidTransform) ||
			table.Build(diagonal, false, scale, 0) || table.GetLength() != previousLength)
			return Fail("A failed build modified the previous distance table.");

		parameter = 123.0;
		if (table.TryGetParameter(std::numeric_limits<double>::infinity(), parameter) || parameter != 123.0)
			return Fail("An invalid distance modified the output parameter.");

		const std::array<vec3, 3> repeated{ vec3(3.0f), vec3(3.0f), vec3(3.0f) };
		if (!table.Build(repeated, true, mat4(1.0f)) || table.GetLength() != 0.0 ||
			!table.TryGetParameter(10.0, parameter) || parameter != 0.0)
			return Fail("Zero-length path handling failed.");

		return 0;
	}
	int CheckSplineFollowing()
	{
		struct Scope
		{
			Scene* SceneValue = Scene::Create();
			SplineFollowSystem* System = SplineFollowSystem::Create();

			~Scope()
			{
				if (System) System->Release();
				if (SceneValue) SceneValue->Release();
			}
		} scope;

		if (!scope.SceneValue || !scope.System)
			return Fail("Spline following setup failed.");

		auto& world = scope.SceneValue->GetRegistry();
		const auto addSpatial = [&]()
			{
				const auto entity = scope.SceneValue->CreateEntity().GetEntityHandle();
				world.emplace<TransformComponent>(entity);
				world.emplace<WorldTransformComponent>(entity);
				world.emplace<HierarchyComponent>(entity);
				world.emplace<FlagComponent>(entity);
				return entity;
			};

		const auto path = addSpatial();
		const auto first = addSpatial();
		const auto second = addSpatial();

		world.get<HierarchyComponent>(path).children = { first, second };
		world.get<HierarchyComponent>(first).parent = path;
		world.get<HierarchyComponent>(second).parent = path;

		auto& spline = world.emplace<SplineComponent>(path);
		spline.Points = { SplinePoint{ vec3(0.0f) }, SplinePoint{ vec3(10.0f, 0.0f, 0.0f) } };

		if (!world.get<TransformComponent>(path).SetLocalScale(vec3(2.0f, 1.0f, 1.0f)))
			return Fail("Path scale setup failed.");

		world.emplace<SplineFollowerComponent>(first).Speed = 2.0f;
		world.emplace<SplineFollowerComponent>(second).Speed = 4.0f;

		entt::registry global;
		const std::vector<Scene*> scenes{ scope.SceneValue };
		world.get<SplineFollowerComponent>(first).AlignToMovement = true;
		spline.Points.front().RollDegrees = 90.0f;
		spline.Points.back().RollDegrees = 90.0f;
		scope.System->OnUpdate(global, scenes, 0.5f);

		if (!Near(world.get<TransformComponent>(first).position, vec3(0.5f, 0.0f, 0.0f)) ||
			!Near(world.get<TransformComponent>(second).position, vec3(1.0f, 0.0f, 0.0f)))
			return Fail("Independent followers or world-distance movement failed.");

		const auto& alignedTransform = world.get<TransformComponent>(first);
		const auto& defaultTransform = world.get<TransformComponent>(second);
		if (!Near(alignedTransform.GetLocalForward(), vec3(1.0f, 0.0f, 0.0f)) ||
			!Near(alignedTransform.GetLocalUp(), vec3(0.0f, 0.0f, 1.0f)) ||
			!Near(defaultTransform.GetLocalForward(), vec3(0.0f, 0.0f, 1.0f)))
			return Fail("Spline follower heading, roll, or opt-out behavior failed.");

		world.get<SplineFollowerComponent>(first).Pause();
		scope.System->OnUpdate(global, scenes, 0.5f);
		if (!Near(world.get<TransformComponent>(first).position, vec3(0.5f, 0.0f, 0.0f)) ||
			!Near(world.get<TransformComponent>(second).position, vec3(2.0f, 0.0f, 0.0f)))
			return Fail("Pausing one follower affected another.");

		spline.Points[1].Position = vec3(0.0f, 10.0f, 0.0f);
		scope.System->OnUpdate(global, scenes, 0.5f);
		if (!Near(world.get<TransformComponent>(first).position, vec3(0.0f, 1.0f, 0.0f)))
			return Fail("A changed path reused stale curve data.");

		auto& follower = world.get<SplineFollowerComponent>(second);
		follower.Speed = -3.0f;
		follower.AlignToMovement = true;
		follower.StartDistance = 1.0;
		follower.Loop = true;
		follower.Restart();

		scope.System->OnUpdate(global, scenes, 1.0f);
		if (!Near(world.get<TransformComponent>(second).position, vec3(0.0f, 8.0f, 0.0f)))
			return Fail("Reverse looping failed.");

		if (!Near(world.get<TransformComponent>(second).GetLocalForward(), vec3(0.0f, -1.0f, 0.0f)))
			return Fail("Reverse spline follower did not face its movement direction.");

		follower.Speed = 3.0f;
		follower.StartDistance = 9.0;
		follower.Loop = false;
		follower.Restart();

		scope.System->OnUpdate(global, scenes, 1.0f);
		if (follower.IsPlaying || !Near(world.get<TransformComponent>(second).position, vec3(0.0f, 10.0f, 0.0f)))
			return Fail("Non-looping endpoint handling failed.");

		scope.SceneValue->SetPaused(true);
		const f64 previousDistance = follower.CurrentDistance;
		scope.System->OnUpdate(global, scenes, 1.0f);
		if (follower.CurrentDistance != previousDistance)
			return Fail("A paused scene advanced spline movement.");

		return 0;
	}
	int CheckSplinePlacement()
	{
		SplinePlacementSettings settings;
		std::vector<mat4> matrices;

		const std::array<vec3, 2> line{ vec3(0.0f), vec3(0.0f, 0.0f, 4.0f) };
		if (!SplinePlacement::BuildMatrices(line, false, mat4(1.0f), settings, matrices) || matrices.size() != 5)
			return Fail("Straight spline placement count failed.");

		for (size_t i = 0; i < matrices.size(); ++i)
		{
			if (!Near(vec3(matrices[i][3]), vec3(0.0f, 0.0f, static_cast<float>(i))) ||
				!Near(vec3(matrices[i][0]), vec3(1.0f, 0.0f, 0.0f)) ||
				!Near(vec3(matrices[i][1]), vec3(0.0f, 1.0f, 0.0f)) ||
				!Near(vec3(matrices[i][2]), vec3(0.0f, 0.0f, 1.0f)))
				return Fail("Straight placement position or orientation failed.");
		}

		const std::array<vec3, 2> diagonal{ vec3(0.0f), vec3(1.0f, 0.0f, 1.0f) };
		const mat4 transform = glm::translate(mat4(1.0f), vec3(100.0f, 3.0f, -7.0f)) *
			glm::scale(mat4(1.0f), vec3(2.0f, 1.0f, 1.0f));

		settings.Spacing = std::sqrt(5.0) * 0.5;
		if (!SplinePlacement::BuildMatrices(diagonal, false, transform, settings, matrices) || matrices.size() < 2 ||
			!Near(vec3(matrices[1][3]), vec3(101.0f, 3.0f, -6.5f)) ||
			!Near(vec3(matrices[1][2]), glm::normalize(vec3(2.0f, 0.0f, 1.0f))))
			return Fail("World-space placement under nonuniform scale failed.");

		settings = SplinePlacementSettings{};
		const std::array<vec3, 2> vertical{ vec3(0.0f), vec3(0.0f, 4.0f, 0.0f) };
		if (!SplinePlacement::BuildMatrices(vertical, false, mat4(1.0f), settings, matrices) ||
			matrices.size() != 5 || !Near(vec3(matrices[0][2]), vec3(0.0f, 1.0f, 0.0f)) ||
			std::abs(glm::dot(vec3(matrices[0][1]), vec3(matrices[0][2]))) > 0.0001f)
			return Fail("Vertical placement orientation failed.");

		const std::array<vec3, 4> loop{
			vec3(0.0f), vec3(2.0f, 0.0f, 0.0f), vec3(2.0f, 0.0f, 2.0f), vec3(0.0f, 0.0f, 2.0f)
		};

		SplineDistanceTable distances;
		if (!distances.Build(loop, true, mat4(1.0f)))
			return Fail("Closed placement setup failed.");

		settings.Spacing = distances.GetLength() / 4.0;
		if (!SplinePlacement::BuildMatrices(loop, true, mat4(1.0f), settings, matrices) || matrices.size() != 4)
			return Fail("Closed placement duplicated the endpoint.");

		const auto preserved = matrices;
		settings.Spacing = 0.0;
		if (SplinePlacement::BuildMatrices(line, false, mat4(1.0f), settings, matrices) || matrices != preserved)
			return Fail("Invalid placement settings modified the output.");

		settings.Spacing = 1.0e-9;
		if (SplinePlacement::BuildMatrices(line, false, mat4(1.0f), settings, matrices) || matrices != preserved)
			return Fail("Placement instance budget was not enforced.");

		settings = SplinePlacementSettings{};
		const std::array<vec3, 2> repeated{ vec3(3.0f), vec3(3.0f) };
		if (!SplinePlacement::BuildMatrices(repeated, false, mat4(1.0f), settings, matrices) || !matrices.empty())
			return Fail("Zero-length placement handling failed.");

		return 0;
	}
	int CheckSplineMeshMatrices()
	{
		SplineComponent spline;
		spline.Points = {
			SplinePoint{ vec3(0.0f) },
			SplinePoint{ vec3(0.0f, 0.0f, 4.0f) }
		};

		SplineMeshComponent settings;
		settings.Spacing = 2.0;
		settings.InstanceScale = vec3(2.0f, 3.0f, 4.0f);

		const mat4 world = glm::translate(mat4(1.0f), vec3(10.0f, 20.0f, 30.0f)) *
			glm::scale(mat4(1.0f), vec3(3.0f, 2.0f, 2.0f));

		std::vector<mat4> matrices;
		if (!settings.BuildMatrices(spline, world, matrices) || matrices.size() != 5 ||
			!Near(vec3(matrices.front()[3]), vec3(10.0f, 20.0f, 30.0f)) ||
			!Near(vec3(matrices.back()[3]), vec3(10.0f, 20.0f, 38.0f)))
			return Fail("Spline mesh world-space placement failed.");

		for (const auto& matrix : matrices)
		{
			if (!Near(vec3(matrix[0]), vec3(2.0f, 0.0f, 0.0f)) ||
				!Near(vec3(matrix[1]), vec3(0.0f, 3.0f, 0.0f)) ||
				!Near(vec3(matrix[2]), vec3(0.0f, 0.0f, 4.0f)))
				return Fail("Path scale was incorrectly applied to instance size.");
		}

		const auto preserved = matrices;

		settings.Spacing = 0.0;
		if (settings.BuildMatrices(spline, world, matrices) || matrices != preserved)
			return Fail("Invalid spacing modified spline mesh output.");

		settings.Spacing = 2.0;
		settings.InstanceScale.y = 0.0f;
		if (settings.BuildMatrices(spline, world, matrices) || matrices != preserved)
			return Fail("Invalid instance scale modified spline mesh output.");

		return 0;
	}
	int CheckSplinePointRoll()
	{
		SplineComponent spline;
		spline.Points = {
			SplinePoint{ vec3(0.0f), 90.0f },
			SplinePoint{ vec3(0.0f, 0.0f, 4.0f), 90.0f }
		};

		SplineMeshComponent settings;
		std::vector<mat4> matrices;

		if (!settings.BuildMatrices(spline, mat4(1.0f), matrices) || matrices.size() != 5)
			return Fail("Constant spline roll setup failed.");

		// 모든 조각이 90도여야 합니다. 조각마다 회전이 누적되면 실패합니다.
		for (size_t i = 0; i < matrices.size(); ++i)
		{
			const auto& matrix = matrices[i];
			if (!Near(vec3(matrix[0]), vec3(0.0f, 1.0f, 0.0f)) ||
				!Near(vec3(matrix[1]), vec3(-1.0f, 0.0f, 0.0f)) ||
				!Near(vec3(matrix[2]), vec3(0.0f, 0.0f, 1.0f)) ||
				!Near(vec3(matrix[3]), vec3(0.0f, 0.0f, static_cast<f32>(i))))
				return Fail("Spline roll accumulated or changed the path.");
		}

		spline.Points.front().RollDegrees = 0.0f;
		if (!settings.BuildMatrices(spline, mat4(1.0f), matrices) || matrices.size() != 5)
			return Fail("Interpolated spline roll setup failed.");

		const f32 diagonal = std::sqrt(0.5f);
		if (!Near(vec3(matrices[2][0]), vec3(diagonal, diagonal, 0.0f)) ||
			!Near(vec3(matrices[2][1]), vec3(-diagonal, diagonal, 0.0f)))
			return Fail("Spline roll midpoint interpolation failed.");

		// 0 -> 360은 중간에서 180도가 되어야 합니다.
		spline.Points.back().RollDegrees = 360.0f;
		if (!settings.BuildMatrices(spline, mat4(1.0f), matrices) || matrices.size() != 5 ||
			!Near(vec3(matrices[2][0]), vec3(-1.0f, 0.0f, 0.0f)) ||
			!Near(vec3(matrices[2][1]), vec3(0.0f, -1.0f, 0.0f)))
			return Fail("Spline roll lost an authored full turn.");

		const auto preserved = matrices;
		const std::array<vec3, 2> line{ vec3(0.0f), vec3(0.0f, 0.0f, 4.0f) };
		const std::array<f32, 1> wrongCount{ 0.0f };
		SplinePlacementSettings placement;

		if (SplinePlacement::BuildMatrices(line, false, mat4(1.0f), placement, matrices, wrongCount) ||
			matrices != preserved)
			return Fail("Invalid roll count modified the output.");

		spline.Points.back().RollDegrees = std::numeric_limits<f32>::quiet_NaN();
		if (spline.IsValid() || settings.BuildMatrices(spline, mat4(1.0f), matrices) || matrices != preserved)
			return Fail("Invalid point roll modified the output.");

		return 0;
	}
	int CheckSplineSharedFrames()
	{
		const std::array<vec3, 5> loop{
			vec3(0.0f), vec3(4.0f, 1.0f, 0.0f), vec3(5.0f, 3.0f, 3.0f),
			vec3(2.0f, -1.0f, 6.0f), vec3(-2.0f, 2.0f, 3.0f)
		};
		const std::array<f32, 5> rolls{ 15.0f, 45.0f, 0.0f, -30.0f, 5.0f };
		const mat4 world = glm::scale(mat4(1.0f), vec3(2.0f, 0.75f, 1.25f));

		SplinePlacementSettings settings;
		SplineFrameTable frames;
		if (!frames.Build(loop, true, world, settings.InitialUp, rolls))
			return Fail("Could not build a nonplanar loop frame table.");

		const f64 length = frames.GetLength();
		mat4 first;
		mat4 last;
		mat4 nearLast;
		if (length <= 0.0 || !frames.TryEvaluate(0.0, first) || !frames.TryEvaluate(length, last) ||
			!frames.TryEvaluate(length * (1.0 - 1.0e-4), nearLast))
			return Fail("Could not evaluate the closed loop seam.");

		for (int column = 0; column < 4; ++column)
		{
			if (!Near(vec3(first[column]), vec3(last[column])))
				return Fail("The start and end frames of a closed loop differ.");
		}

		for (int axis = 0; axis < 3; ++axis)
		{
			if (!Near(vec3(first[axis]), vec3(nearLast[axis]), 0.01f))
				return Fail("The frame jumps near the closed loop seam.");
		}

		settings.Spacing = length / 8.0;
		std::vector<mat4> regular;
		if (!SplinePlacement::BuildMatrices(loop, true, world, settings, regular, rolls) || regular.size() <= 3)
			return Fail("Could not build regular spline placements.");

		settings.StartDistance = settings.Spacing * 3.0;
		settings.Spacing *= 2.0;
		std::vector<mat4> offset;
		if (!SplinePlacement::BuildMatrices(loop, true, world, settings, offset, rolls) || offset.empty())
			return Fail("Could not build offset spline placements.");

		for (int column = 0; column < 4; ++column)
		{
			if (!Near(vec3(regular[3][column]), vec3(offset.front()[column])))
				return Fail("Placement settings changed the frame at the same distance.");
		}

		return 0;
	}
	int CheckSplineMeshDeformation()
	{
		SplineFrameTable frames;
		const std::array<vec3, 2> line{ vec3(0.0f), vec3(0.0f, 0.0f, 12.0f) };
		if (!frames.Build(line, false, mat4(1.0f), vec3(0.0f, 1.0f, 0.0f)))
			return Fail("Could not build straight deformation frames.");

		const std::array<VertexPosition, 4> positions{
			VertexPosition{ vec3(-1.0f, 0.0f, 2.0f) },
			VertexPosition{ vec3(1.0f, 0.0f, 2.0f) },
			VertexPosition{ vec3(-1.0f, -4.0f, 6.0f) },
			VertexPosition{ vec3(1.0f, -4.0f, 6.0f) }
		};

		std::array<VertexMaterial, 4> materials{};
		for (auto& material : materials)
		{
			material.normal = glm::normalize(vec3(0.0f, 1.0f, 1.0f));
			material.tangent = glm::normalize(vec3(0.0f, -1.0f, 1.0f));
			material.binormal = vec3(1.0f, 0.0f, 0.0f);
			material.texCoord = vec2(0.25f, 0.75f);
		}

		SplineDeformedMeshData mesh;
		if (!SplineMeshDeformer::Build(frames, positions, materials, 3.0, 11.0, mesh) ||
			mesh.Positions.size() != positions.size() || mesh.Materials.size() != materials.size())
			return Fail("Straight spline mesh deformation failed.");

		if (!Near(mesh.Positions.front().position, vec3(-1.0f, 0.0f, 3.0f)) ||
			!Near(mesh.Positions.back().position, vec3(1.0f, -4.0f, 11.0f)) ||
			!Near(mesh.Bounds.min, vec3(-1.0f, -4.0f, 3.0f)) ||
			!Near(mesh.Bounds.max, vec3(1.0f, 0.0f, 11.0f)))
			return Fail("Spline deformation positions or bounds are incorrect.");

		const auto& material = mesh.Materials.front();
		if (!Near(material.normal, glm::normalize(vec3(0.0f, 2.0f, 1.0f))) ||
			!Near(material.tangent, glm::normalize(vec3(0.0f, -1.0f, 2.0f))) ||
			material.texCoord != materials.front().texCoord)
			return Fail("Spline deformation lost the normal, tangent, or UV.");

		if (!SplineMeshDeformer::Build(frames, positions, materials, 3.0, 11.0, mesh, 2.0) ||
			mesh.Materials.front().texCoord != vec2(0.25f, 0.0f) ||
			mesh.Materials.back().texCoord != vec2(0.25f, 4.0f))
			return Fail("Open spline UVs did not use the deformed section length.");

		const std::array<vec3, 3> curve{
			vec3(0.0f), vec3(0.0f, 0.0f, 4.0f), vec3(4.0f, 0.0f, 4.0f)
		};
		const std::array<VertexPosition, 3> strip{
			VertexPosition{ vec3(0.0f, 0.0f, 2.0f) },
			VertexPosition{ vec3(0.0f, 0.0f, 4.0f) },
			VertexPosition{ vec3(0.25f, 0.0f, 6.0f) }
		};
		const std::array<VertexMaterial, 3> stripMaterials{ materials[0], materials[1], materials[2] };

		if (!frames.Build(curve, false, mat4(1.0f), vec3(0.0f, 1.0f, 0.0f)) ||
			!SplineMeshDeformer::Build(frames, strip, stripMaterials, 0.0, frames.GetLength(), mesh))
			return Fail("Curved spline mesh deformation failed.");

		if (!Near(mesh.Positions[1].position, vec3(0.0f, 0.0f, 4.0f)) ||
			!Near(mesh.Positions.back().position, vec3(4.0f, 0.0f, 3.75f)))
			return Fail("The deformed strip did not follow the curve and its end frame.");

		using DeformFailure = SplineMeshDeformer::Failure;
		DeformFailure failure{};

		auto flat = strip;
		for (auto& vertex : flat)
			vertex.position.z = 0.0f;

		if (SplineMeshDeformer::Build(frames, flat, stripMaterials, 0.0, frames.GetLength(), mesh, 0.0, failure) ||
			failure != DeformFailure::ZeroSourceZLength)
			return Fail("A zero-length source Z axis was not diagnosed.");

		auto wide = strip;
		wide[1].position.x = 2.0f;
		if (SplineMeshDeformer::Build(frames, wide, stripMaterials, 0.0, frames.GetLength(), mesh, 0.0, failure) ||
			failure != DeformFailure::FoldedSection)
			return Fail("A folded spline section was not diagnosed.");

		if (mesh.Positions.size() != strip.size() || !Near(mesh.Positions[1].position, vec3(0.0f, 0.0f, 4.0f)))
			return Fail("Failed deformation changed the previous mesh.");

		return 0;
	}
	int CheckClosedSplineMeshSeam()
	{
		const std::array<vec3, 5> loop{
			vec3(0.0f), vec3(4.0f, 1.0f, 0.0f), vec3(5.0f, 3.0f, 3.0f),
			vec3(2.0f, -1.0f, 6.0f), vec3(-2.0f, 2.0f, 3.0f)
		};
		const std::array<f32, 5> rolls{ 15.0f, 45.0f, 0.0f, -30.0f, 5.0f };

		SplineFrameTable frames;
		if (!frames.Build(loop, true, mat4(1.0f), vec3(0.0f, 1.0f, 0.0f), rolls))
			return Fail("Could not build closed mesh deformation frames.");

		std::array<VertexPosition, 6> positions{};
		std::array<VertexMaterial, 6> materials{};

		for (size_t row = 0; row < 3; ++row)
		{
			for (size_t side = 0; side < 2; ++side)
			{
				const size_t index = row * 2 + side;
				const f32 x = side == 0 ? -0.125f : 0.125f;
				positions[index].position = vec3(x, 0.1f, static_cast<f32>(row) * 2.0f);

				auto& material = materials[index];
				material.normal = glm::normalize(vec3(0.0f, 1.0f, 1.0f));
				material.tangent = glm::normalize(vec3(0.0f, -1.0f, 1.0f));
				material.binormal = vec3(1.0f, 0.0f, 0.0f);
				material.texCoord = vec2(static_cast<f32>(side), static_cast<f32>(row) * 0.5f);
			}
		}

		SplineDeformedMeshData mesh;
		if (!SplineMeshDeformer::Build(frames, positions, materials, 0.0, frames.GetLength(), mesh))
			return Fail("Closed spline mesh deformation failed.");

		for (size_t side = 0; side < 2; ++side)
		{
			const size_t last = side + 4;
			const auto& startMaterial = mesh.Materials[side];
			const auto& endMaterial = mesh.Materials[last];

			if (!Near(mesh.Positions[side].position, mesh.Positions[last].position) ||
				!Near(startMaterial.normal, endMaterial.normal) ||
				!Near(startMaterial.tangent, endMaterial.tangent) ||
				!Near(startMaterial.binormal, endMaterial.binormal))
				return Fail("Closed spline mesh has a geometry or shading seam.");

			if (startMaterial.texCoord.y != 0.0f || endMaterial.texCoord.y != 1.0f)
				return Fail("Closed spline mesh changed seam UVs.");
		}

		const f64 length = frames.GetLength();
		const f64 repeatDistance = length / 3.6;
		if (!SplineMeshDeformer::Build(frames, positions, materials, 0.0, length, mesh, repeatDistance) ||
			mesh.Materials[0].texCoord.y != 0.0f ||
			mesh.Materials[2].texCoord.y != 2.0f ||
			mesh.Materials[4].texCoord.y != 4.0f)
			return Fail("Closed spline UVs did not align the texture repeat at the seam.");

		return 0;
	}
}

int main()
{
	using namespace Engine;

	SplineSample sample;
	const std::array<vec3, 2> line{ vec3(0.0f), vec3(10.0f, 0.0f, 0.0f) };

	if (!SplineCurve::Evaluate(line, false, 0.25, sample) ||
		!Near(sample.Position, vec3(2.5f, 0.0f, 0.0f)) ||
		!Near(sample.Tangent, vec3(10.0f, 0.0f, 0.0f)))
		return Fail("Two-point linear evaluation failed.");

	if (!SplineCurve::Evaluate(line, false, -1.0, sample) || !Near(sample.Position, line.front()) ||
		!SplineCurve::Evaluate(line, false, 2.0, sample) || !Near(sample.Position, line.back()))
		return Fail("Open endpoint clamping failed.");

	const std::array<vec3, 3> curve{
		vec3(0.0f), vec3(1.0f, 1.0f, 0.0f), vec3(2.0f, 0.0f, 0.0f)
	};

	if (!SplineCurve::Evaluate(curve, false, 0.25, sample) ||
		!Near(sample.Position, vec3(0.5f, 0.625f, 0.0f)) ||
		!Near(sample.Tangent, vec3(2.0f, 2.5f, 0.0f)))
		return Fail("Curved segment position or tangent failed.");

	if (!SplineCurve::Evaluate(curve, false, 0.5, sample) || !Near(sample.Position, curve[1]))
		return Fail("The curve did not pass through its control point.");

	const std::array<vec3, 4> loop{
		vec3(0.0f), vec3(2.0f, 0.0f, 0.0f), vec3(2.0f, 2.0f, 0.0f), vec3(0.0f, 2.0f, 0.0f)
	};

	SplineSample start;
	SplineSample end;
	if (!SplineCurve::Evaluate(loop, true, 0.0, start) ||
		!SplineCurve::Evaluate(loop, true, 1.0, end) ||
		!Near(start.Position, loop.front()) || !Near(start.Position, end.Position) ||
		!Near(start.Tangent, vec3(4.0f, -4.0f, 0.0f)) || !Near(start.Tangent, end.Tangent))
		return Fail("Closed seam position or tangent failed.");

	const std::array<vec3, 1> single{ vec3(3.0f) };
	if (!SplineCurve::Evaluate(single, false, 0.5, sample) ||
		!Near(sample.Position, single.front()) || !Near(sample.Tangent, vec3(0.0f)))
		return Fail("Single-point evaluation failed.");

	const std::array<vec3, 3> repeated{ vec3(3.0f), vec3(3.0f), vec3(3.0f) };
	if (!SplineCurve::Evaluate(repeated, true, 0.37, sample) ||
		!Near(sample.Position, repeated.front()) || !Near(sample.Tangent, vec3(0.0f)))
		return Fail("Repeated-point evaluation failed.");

	const SplineSample sentinel{ vec3(123.0f), vec3(456.0f) };
	sample = sentinel;

	auto invalid = curve;
	invalid[2].x = std::numeric_limits<float>::quiet_NaN();
	const double infinity = std::numeric_limits<double>::infinity();

	if (SplineCurve::Evaluate(std::span<const vec3>{}, false, 0.0, sample) ||
		SplineCurve::Evaluate(single, true, 0.0, sample) ||
		SplineCurve::Evaluate(line, true, 0.0, sample) ||
		SplineCurve::Evaluate(invalid, false, 0.0, sample) ||
		SplineCurve::Evaluate(curve, false, infinity, sample) ||
		!Near(sample.Position, sentinel.Position) || !Near(sample.Tangent, sentinel.Tangent))
		return Fail("Invalid input handling or output preservation failed.");

	if (CheckSplineDistanceTable() != 0)
		return 1;

	if (CheckSplineFollowing() != 0)
		return 1;

	if (CheckSplinePlacement() != 0)
		return 1;

	if (CheckSplineMeshMatrices() != 0)
		return 1;

	if (CheckSplinePointRoll() != 0)
		return 1;

	if (CheckSplineSharedFrames() != 0)
		return 1;

	if (CheckSplineMeshDeformation() != 0)
		return 1;

	if (CheckClosedSplineMeshSeam() != 0)
		return 1;

	return 0;
}