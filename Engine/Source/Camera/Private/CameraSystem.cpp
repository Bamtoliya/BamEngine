#include "CameraSystem.h"
#include "CameraComponent.h"
#include "PixelPerfectCameraComponent.h"
#include "WorldTransformComponent.h"

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

EResult CameraSystem::Initialize(void* arg)
{
	(void)arg;
	return EResult::Success;
}

CameraSystem* CameraSystem::Create(void* arg)
{
	CameraSystem* instance = new CameraSystem();
	if (IsFailure(instance->Initialize(arg)))
	{
		Safe_Release(instance);
		return nullptr;
	}
	return instance;
}

void CameraSystem::Free()
{
}

void CameraSystem::OnLateUpdate(entt::registry& globalRegistry, const vector<Scene*>& activeScenes, f32 dt)
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
	for (const auto entity : registry.view<CameraComponent>())
		UpdateCamera(registry, entity);
}

bool CameraSystem::UpdateCamera(entt::registry& registry, entt::entity entity)
{
	if (!registry.valid(entity)) return false;

	auto* component = registry.try_get<CameraComponent>(entity);
	if (!component) return false;

	auto& camera = *component;
	camera.InvalidateCache();

	const auto* worldTransform = registry.try_get<WorldTransformComponent>(entity);
	if (!worldTransform || !IsValidCameraProjection(camera))
		return false;

	const mat4& worldMatrix = worldTransform->worldMatrix;
	if (!IsFiniteCameraMatrix(worldMatrix))
		return false;

	const f32 determinant = glm::determinant(worldMatrix);
	if (!std::isfinite(determinant) || determinant == 0.f)
		return false;

	CameraBuffer result{};
	mat4 finalWorld = worldMatrix;
	vec3 camPos = vec3(worldMatrix[3]);

	// ── 픽셀 퍼펙트 카메라 연동 (Sub-pixel Snapping) ──
	const auto* pixelPerfect = registry.try_get<PixelPerfectCameraComponent>(entity);
	if (pixelPerfect && camera.CameraType == ECameraType::Orthographic && pixelPerfect->assetsPPU > 0.0f)
	{
		if (pixelPerfect->snapSubpixel)
		{
			const f32 ppu = pixelPerfect->assetsPPU;
			camPos.x = std::floor(camPos.x * ppu + 0.5f) / ppu;
			camPos.y = std::floor(camPos.y * ppu + 0.5f) / ppu;

			finalWorld[3][0] = camPos.x;
			finalWorld[3][1] = camPos.y;
		}
	}

	result.viewMatrix = glm::inverse(finalWorld);
	result.invViewMatrix = finalWorld;
	result.cameraPosition = camPos;

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
		return false;

	camera.m_ViewMatrix = result.viewMatrix;
	camera.m_ViewMatrixInv = result.invViewMatrix;
	camera.m_ProjMatrix = result.projMatrix;
	camera.m_ProjMatrixInv = result.invProjMatrix;
	camera.m_ViewProjMatrix = result.viewProjMatrix;
	camera.m_ViewProjMatrixInv = result.invViewProjMatrix;
	camera.m_CameraPosition = result.cameraPosition;
	camera.m_IsCacheValid = true;
	return true;
}

END
