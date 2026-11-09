#include "SplineFollowSystem.h"
#include "SplineFollowerComponent.h"
#include "SplineComponent.h"
#include "SplineCurve.h"
#include "SplineDistanceTable.h"
#include "TransformSystem.h"
#include "TransformComponent.h"
#include "HierarchyComponent.h"
#include "FlagComponent.h"
#include "RigidBodyComponent.h"
#include "RigidBody2DComponent.h"
#include "CharacterControllerComponent.h"
#include "SplineFrameTable.h"

namespace
{
	using namespace Engine;

	struct SplinePathCache
	{
		std::vector<SplinePoint> Source;
		std::vector<vec3> Points;
		SplineDistanceTable Distances;
		mat3 LinearTransform{ 1.0f };
		bool Closed = false;
		bool Attempted = false;
		bool Ready = false;
		SplineFrameTable Frames;
		bool FramesAttempted = false;
	};

	struct SplineSceneCache
	{
		std::unordered_map<entt::entity, SplinePathCache> Paths;
	};

	bool PrepareSplinePathCache(SplinePathCache& cache, const SplineComponent& spline, const mat4& worldMatrix)
	{
		if (!spline.IsValid())
			return false;

		const mat3 linear(worldMatrix);
		if (cache.Attempted && cache.Source == spline.Points &&
			cache.Closed == spline.Closed && cache.LinearTransform == linear)
			return cache.Ready;

		cache.Source = spline.Points;
		cache.Closed = spline.Closed;
		cache.LinearTransform = linear;
		cache.Attempted = true;
		cache.Ready = false;
		cache.Frames = {};
		cache.FramesAttempted = false;

		cache.Points.clear();
		cache.Points.reserve(spline.Points.size());
		for (const auto& point : spline.Points)
			cache.Points.push_back(point.Position);

		cache.Ready = cache.Distances.Build(cache.Points, cache.Closed, worldMatrix);
		return cache.Ready;
	}

	void AlignSplineFollower(SplinePathCache& path, f64 distance, const vec3& tangent, bool reverse,
		TransformComponent& candidate)
	{
		if (candidate.absolute != ETransformAbsolute::None)
			return;

		const glm::dmat3 parentLinear(path.LinearTransform);
		const f64 determinant = glm::determinant(parentLinear);
		if (!std::isfinite(determinant) || determinant == 0.0)
			return;

		if (!path.FramesAttempted)
		{
			path.FramesAttempted = true;
			std::array<f32, SplineComponent::MaxControlPoints> rolls{};
			for (size_t i = 0; i < path.Source.size(); ++i)
				rolls[i] = path.Source[i].RollDegrees;

			const auto pointRolls = std::span<const f32>(rolls.data(), path.Source.size());
			const mat4 pathBasis(path.LinearTransform);
			if (!path.Frames.Build(path.Points, path.Closed, pathBasis, vec3(0.0f, 1.0f, 0.0f), pointRolls))
				return;
		}

		mat4 frame;
		if (!path.Frames.TryEvaluate(distance, frame))
			return;

		const glm::dvec3 forward = glm::normalize(glm::dvec3(reverse ? -tangent : tangent));
		const glm::dvec3 localUp = glm::inverse(parentLinear) * glm::dvec3(frame[1]);
		const glm::dvec3 right = glm::cross(localUp, forward);
		const f64 rightLength = glm::length(right);
		if (!std::isfinite(rightLength) || rightLength <= 0.0)
			return;

		const glm::dvec3 unitRight = right / rightLength;
		const glm::dmat3 basis(unitRight, glm::cross(forward, unitRight), forward);
		static_cast<void>(candidate.SetLocalRotation(glm::quat(glm::quat_cast(basis))));
	}

	f64 NormalizeSplineDistance(f64 distance, f64 length, bool loop)
	{
		if (length == 0.0)
			return 0.0;

		if (!loop)
			return std::clamp(distance, 0.0, length);

		distance = std::fmod(distance, length);
		if (distance < 0.0)
			distance += length;

		return distance < length ? distance : 0.0;
	}
}

namespace Engine
{
	SplineFollowSystem* SplineFollowSystem::Create(void*)
	{
		return new SplineFollowSystem();
	}

	void SplineFollowSystem::OnUpdate(entt::registry& registry, const vector<Scene*>& activeScenes, f32 dt)
	{
		(void)registry;
		if (!std::isfinite(dt) || dt <= 0.0f)
			return;

		for (Scene* scene : activeScenes)
		{
			if (!scene || !scene->IsActive() || scene->IsPaused())
				continue;

			scene->UpdateHierarchyFlags();
			auto& world = scene->GetRegistry();
			auto& cache = world.ctx().emplace<SplineSceneCache>();

			std::erase_if(cache.Paths, [&world](const auto& item)
				{
					return !world.valid(item.first) || !world.all_of<SplineComponent>(item.first);
				});

			auto view = world.view<SplineFollowerComponent, TransformComponent>();
			for (auto entity : view)
			{
				auto& follower = view.get<SplineFollowerComponent>(entity);
				const auto& transform = view.get<TransformComponent>(entity);

				const auto fail = [&](const char* reason)
					{
						if (follower.m_LastFailure != reason)
						{
							BAM_LOG(Warn, "Engine", "Spline", "Follower {}: {}", entt::to_integral(entity), reason);
							follower.m_LastFailure = reason;
						}
					};

				const auto* flag = world.try_get<FlagComponent>(entity);
				if (flag && (!flag->IsActiveInHierarchy() ||
					HasFlag(flag->effectiveFlags, EEntityFlag::Paused) ||
					HasFlag(flag->effectiveFlags, EEntityFlag::Dead)))
					continue;

				if (!std::isfinite(follower.Speed) || !std::isfinite(follower.StartDistance) ||
					!std::isfinite(follower.CurrentDistance))
				{
					fail("Non-finite movement settings.");
					continue;
				}

				if (HasFlag(transform.absolute, ETransformAbsolute::Position))
				{
					fail("Absolute position mode is not supported.");
					continue;
				}

				if (world.any_of<RigidBodyComponent, RigidBody2DComponent, CharacterControllerComponent>(entity))
				{
					fail("Another movement component owns this transform.");
					continue;
				}

				const auto* hierarchy = world.try_get<HierarchyComponent>(entity);
				const auto parent = hierarchy ? hierarchy->parent : entt::null;
				const auto* spline = world.valid(parent) ? world.try_get<SplineComponent>(parent) : nullptr;
				if (!spline)
				{
					fail("The direct parent must contain a spline.");
					continue;
				}

				mat4 parentWorld;
				if (!TransformSystem::TryGetWorldMatrix(world, parent, parentWorld))
				{
					fail("The path transform could not be evaluated.");
					continue;
				}

				auto& path = cache.Paths[parent];
				if (!PrepareSplinePathCache(path, *spline, parentWorld))
				{
					fail("The spline or its distance table is invalid.");
					continue;
				}

				const f64 length = path.Distances.GetLength();
				const bool initialize = !follower.m_Initialized || follower.m_BoundPath != parent;
				const f64 sourceDistance = initialize ? follower.StartDistance : follower.CurrentDistance;
				f64 distance = NormalizeSplineDistance(sourceDistance, length, follower.Loop);

				if (follower.IsPlaying)
					distance += static_cast<f64>(dt) * static_cast<f64>(follower.Speed);

				if (!std::isfinite(distance))
				{
					fail("Movement distance overflowed.");
					continue;
				}

				const bool finished = follower.IsPlaying && (length == 0.0 || (!follower.Loop &&
					((follower.Speed > 0.0f && distance >= length) ||
						(follower.Speed < 0.0f && distance <= 0.0))));

				distance = NormalizeSplineDistance(distance, length, follower.Loop);

				f64 parameter = 0.0;
				SplineSample sample;
				if (!path.Distances.TryGetParameter(distance, parameter) ||
					!SplineCurve::Evaluate(path.Points, path.Closed, parameter, sample))
				{
					fail("Distance evaluation failed.");
					continue;
				}

				auto candidate = transform;
				if (!candidate.SetLocalPosition(sample.Position))
				{
					fail("The transform update was rejected.");
					continue;
				}

				if (follower.AlignToMovement)
					AlignSplineFollower(path, distance, sample.Tangent, follower.Speed < 0.0f, candidate);

				if (!TransformSystem::TryCommitTransform(world, entity, candidate))
				{
					fail("The transform update was rejected.");
					continue;
				}

				follower.CurrentDistance = distance;
				follower.m_Initialized = true;
				follower.m_BoundPath = parent;
				follower.m_LastFailure.clear();

				if (finished)
					follower.IsPlaying = false;
			}
		}
	}
}