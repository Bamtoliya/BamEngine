#include "CameraSystem.h"
#include "Components/Render/CameraComponent.h"
#include "Components/WorldTransformComponent.h"

#include <glm/ext/matrix_clip_space.hpp>
#include <cmath>

namespace
{
	bool IsFiniteCameraMatrix(const glm::mat4& matrix)
	{
		for (int column = 0; column < 4; ++column)
		{
			for (int row = 0; row < 4; ++row)
			{
				if (!std::isfinite(matrix[column][row]))
					return false;
			}
		}

		return true;
	}

	bool IsValidCameraProjection(const Engine::CameraComponent& camera)
	{
		if (!std::isfinite(camera.AspectRatio) || camera.AspectRatio <= 0.f)
			return false;

		if (!std::isfinite(camera.NearPlane) || !std::isfinite(camera.FarPlane))
			return false;

		if (camera.FarPlane <= camera.NearPlane)
			return false;

		switch (camera.CameraType)
		{
		case Engine::ECameraType::Perspective:
			return camera.NearPlane > 0.f && std::isfinite(camera.FOV) &&
				camera.FOV > 0.f && camera.FOV < 180.f;

		case Engine::ECameraType::Orthographic:
			return std::isfinite(camera.OrthographicSize) && camera.OrthographicSize > 0.f;

		default:
			return false;
		}
	}
}

BEGIN(Engine)

CameraSystem* CameraSystem::Create()
{
	return new CameraSystem();
}

void CameraSystem::OnLateUpdate(entt::registry& globalRegistry, const vector<Scene*> activeScenes, f32 dt)
{
	UpdateCameras(globalRegistry);

	for (Scene* scene : activeScenes)
	{
		if (scene != nullptr)
			UpdateCameras(scene->GetRegistry());
	}
}

void CameraSystem::UpdateCameras(entt::registry& registry)
{
	auto view = registry.view<CameraComponent>();

	for (auto entity : view)
	{
		auto& camera = view.get<CameraComponent>(entity);
		camera.InvalidateCache();

		const auto* worldTransform = registry.try_get<WorldTransformComponent>(entity);

		if (worldTransform == nullptr || !IsValidCameraProjection(camera))
			continue;

		const mat4& worldMatrix = worldTransform->worldMatrix;

		if (!IsFiniteCameraMatrix(worldMatrix))
			continue;

		const f32 determinant = glm::determinant(worldMatrix);

		if (!std::isfinite(determinant) || determinant == 0.f)
			continue;

		// 계산 성공을 확인하기 전까지 결과를 로컬 변수에 보관합니다.
		CameraBuffer result{};
		result.viewMatrix = glm::inverse(worldMatrix);
		result.invViewMatrix = worldMatrix;
		result.cameraPosition = vec3(worldMatrix[3]);

		if (camera.CameraType == ECameraType::Perspective)
		{
			result.projMatrix = glm::perspectiveLH_ZO(
				glm::radians(camera.FOV), camera.AspectRatio, camera.NearPlane, camera.FarPlane);
		}
		else
		{
			const f32 halfHeight = camera.OrthographicSize * 0.5f;
			const f32 halfWidth = halfHeight * camera.AspectRatio;

			result.projMatrix = glm::orthoLH_ZO(
				-halfWidth, halfWidth, -halfHeight, halfHeight, camera.NearPlane, camera.FarPlane);
		}

		result.invProjMatrix = glm::inverse(result.projMatrix);
		result.viewProjMatrix = result.projMatrix * result.viewMatrix;
		result.invViewProjMatrix = result.invViewMatrix * result.invProjMatrix;

		if (!IsFiniteCameraMatrix(result.viewMatrix) || !IsFiniteCameraMatrix(result.projMatrix) ||
			!IsFiniteCameraMatrix(result.invProjMatrix) || !IsFiniteCameraMatrix(result.viewProjMatrix) ||
			!IsFiniteCameraMatrix(result.invViewProjMatrix))
		{
			continue;
		}

		camera.m_ViewMatrix = result.viewMatrix;
		camera.m_ViewMatrixInv = result.invViewMatrix;
		camera.m_ProjMatrix = result.projMatrix;
		camera.m_ProjMatrixInv = result.invProjMatrix;
		camera.m_ViewProjMatrix = result.viewProjMatrix;
		camera.m_ViewProjMatrixInv = result.invViewProjMatrix;
		camera.m_CameraPosition = result.cameraPosition;
		camera.m_IsCacheValid = true;
	}
}

END