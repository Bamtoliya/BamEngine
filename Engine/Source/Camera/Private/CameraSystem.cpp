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

void CameraSystem::UpdateCameras(entt::registry& registry, uint32 viewportWidth, uint32 viewportHeight)
{
	for (const auto entity : registry.view<CameraComponent>())
		UpdateCamera(registry, entity, viewportWidth, viewportHeight);
}

bool CameraSystem::UpdateCamera(entt::registry& registry, entt::entity entity, uint32 viewportWidth, uint32 viewportHeight)
{
	if (!registry.valid(entity)) return false;

	auto* component = registry.try_get<CameraComponent>(entity);
	if (!component) return false;

	auto& camera = *component;
	camera.InvalidateCache();

	auto* pixelPerfect = registry.try_get<PixelPerfectCameraComponent>(entity);

	// ── 픽셀 퍼펙트 카메라 연동 1단계: 정수 줌(Integer Zoom) 및 OrthographicSize 자동 계산 ──
	if (pixelPerfect && camera.CameraType == ECameraType::Orthographic && pixelPerfect->assetsPPU > 0.0f)
	{
		uint32 vpW = viewportWidth;
		uint32 vpH = viewportHeight;

		if (vpW == 0 || vpH == 0)
		{
			if (camera.AspectRatio > 0.0f)
			{
				vpH = 1080;
				vpW = static_cast<uint32>(std::round(1080.0f * camera.AspectRatio));
			}
			else
			{
				vpW = 1920;
				vpH = 1080;
			}
		}

		const int32 refX = std::max(1, pixelPerfect->refResolutionX);
		const int32 refY = std::max(1, pixelPerfect->refResolutionY);

		const int32 zoomX = std::max(1, static_cast<int32>(vpW) / refX);
		const int32 zoomY = std::max(1, static_cast<int32>(vpH) / refY);

		int32 calculatedZoom = std::min(zoomX, zoomY);
		if (calculatedZoom < 1) calculatedZoom = 1;

		pixelPerfect->currentZoom = calculatedZoom;

		// 뷰포트 세로 픽셀을 정수 줌 배율로 균등 분할하여 1:Zoom 정수배 픽셀 매칭 보장
		camera.OrthographicSize = static_cast<f32>(vpH) / (pixelPerfect->assetsPPU * static_cast<f32>(calculatedZoom));
	}

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

	// ── 픽셀 퍼펙트 카메라 연동 2단계: 서브픽셀 카메라 스냅 (Sub-pixel Position Snapping) ──
	if (pixelPerfect && camera.CameraType == ECameraType::Orthographic && pixelPerfect->assetsPPU > 0.0f)
	{
		if (pixelPerfect->snapSubpixel)
		{
			// 화면 픽셀 단위 격자로 카메라 위치를 스냅하여 서브픽셀 떨림(Jittering & Shimmering) 0% 달성
			const f32 effectivePPU = pixelPerfect->assetsPPU * static_cast<f32>(pixelPerfect->currentZoom);
			if (effectivePPU > 0.0f)
			{
				camPos.x = std::floor(camPos.x * effectivePPU + 0.5f) / effectivePPU;
				camPos.y = std::floor(camPos.y * effectivePPU + 0.5f) / effectivePPU;

				finalWorld[3][0] = camPos.x;
				finalWorld[3][1] = camPos.y;
			}
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
		f32 halfHeight = camera.OrthographicSize * 0.5f;
		f32 halfWidth = halfHeight * camera.AspectRatio;

		if (pixelPerfect && pixelPerfect->assetsPPU > 0.0f)
		{
			if (pixelPerfect->cropFrameY)
			{
				halfHeight = (static_cast<f32>(pixelPerfect->refResolutionY) / pixelPerfect->assetsPPU) * 0.5f;
			}
			if (pixelPerfect->cropFrameX)
			{
				halfWidth = (static_cast<f32>(pixelPerfect->refResolutionX) / pixelPerfect->assetsPPU) * 0.5f;
			}
		}

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
