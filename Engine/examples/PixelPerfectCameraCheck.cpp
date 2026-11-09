#include "CameraSystem.h"
#include "CameraComponent.h"
#include "PixelPerfectCameraComponent.h"
#include "WorldTransformComponent.h"
#include <entt/entt.hpp>
#include <iostream>
#include <cmath>

namespace
{
	using namespace Engine;

	bool Near(f32 a, f32 b, f32 eps = 0.0001f)
	{
		return std::fabs(a - b) <= eps;
	}

	int Fail(const char* message)
	{
		std::cerr << "[FAIL] " << message << std::endl;
		return 1;
	}
}

int main()
{
	entt::registry registry;
	const auto entity = registry.create();

	auto& camera = registry.emplace<CameraComponent>(entity);
	camera.CameraType = ECameraType::Orthographic;
	camera.AspectRatio = 16.0f / 9.0f;
	camera.NearPlane = 0.1f;
	camera.FarPlane = 100.0f;
	camera.OrthographicSize = 10.0f;

	auto& transform = registry.emplace<WorldTransformComponent>(entity);
	transform.worldMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(5.12345f, 3.87654f, -10.0f));

	auto& pixelPerfect = registry.emplace<PixelPerfectCameraComponent>(entity);
	pixelPerfect.assetsPPU = 16.0f;
	pixelPerfect.refResolutionX = 320;
	pixelPerfect.refResolutionY = 180;
	pixelPerfect.snapSubpixel = true;

	// ── Test 1: 1920x1080 뷰포트에서 6배 정수 줌 및 OrthographicSize 계산 검증 ──
	{
		const bool ok = CameraSystem::UpdateCamera(registry, entity, 1920, 1080);
		if (!ok) return Fail("CameraSystem::UpdateCamera failed for 1920x1080");

		if (pixelPerfect.currentZoom != 6)
			return Fail("currentZoom should be 6 for 1920x1080 with 320x180 reference");

		// OrthographicSize = 1080 / (16 * 6) = 11.25
		if (!Near(camera.OrthographicSize, 11.25f))
			return Fail("OrthographicSize should be exactly 11.25 for 1080p with Zoom=6, PPU=16");
	}

	// ── Test 2: 1280x720 뷰포트에서 4배 정수 줌 검증 ──
	{
		camera.AspectRatio = 1280.0f / 720.0f;
		const bool ok = CameraSystem::UpdateCamera(registry, entity, 1280, 720);
		if (!ok) return Fail("CameraSystem::UpdateCamera failed for 1280x720");

		if (pixelPerfect.currentZoom != 4)
			return Fail("currentZoom should be 4 for 1280x720 with 320x180 reference");

		// OrthographicSize = 720 / (16 * 4) = 11.25
		if (!Near(camera.OrthographicSize, 11.25f))
			return Fail("OrthographicSize should be exactly 11.25 for 720p with Zoom=4, PPU=16");
	}

	// ── Test 3: 비정규 1400x900 뷰포트 (종횡비 불일치)에서 보수적 정수 줌 검증 ──
	{
		camera.AspectRatio = 1400.0f / 900.0f;
		const bool ok = CameraSystem::UpdateCamera(registry, entity, 1400, 900);
		if (!ok) return Fail("CameraSystem::UpdateCamera failed for 1400x900");

		// 1400/320 = 4, 900/180 = 5 -> min(4, 5) = 4
		if (pixelPerfect.currentZoom != 4)
			return Fail("currentZoom should be min(4, 5) = 4 for 1400x900");

		// OrthographicSize = 900 / (16 * 4) = 14.0625
		if (!Near(camera.OrthographicSize, 14.0625f))
			return Fail("OrthographicSize should be 14.0625 for 900 height with Zoom=4, PPU=16");
	}

	// ── Test 4: 서브픽셀 카메라 위치 스냅 검증 ──
	{
		// effectivePPU = assetsPPU * currentZoom = 16 * 4 = 64
		// 카메라 위치: (5.12345, 3.87654)
		// snapX = round(5.12345 * 64) / 64 = round(327.9008) / 64 = 328 / 64 = 5.125
		// snapY = round(3.87654 * 64) / 64 = round(248.09856) / 64 = 248 / 64 = 3.875
		CameraSystem::UpdateCamera(registry, entity, 1280, 720); // Zoom = 4
		auto buffer = camera.GetCameraBuffer();
		if (!buffer.has_value()) return Fail("CameraBuffer is empty");

		const f32 expectedX = 328.0f / 64.0f; // 5.125
		const f32 expectedY = 248.0f / 64.0f; // 3.875
		if (!Near(buffer->cameraPosition.x, expectedX))
			return Fail("cameraPosition.x was not snapped correctly to screen pixel grid");
		if (!Near(buffer->cameraPosition.y, expectedY))
			return Fail("cameraPosition.y was not snapped correctly to screen pixel grid");

		// Z축은 스냅 없이 그대로 유지되어야 함
		if (!Near(buffer->cameraPosition.z, -10.0f))
			return Fail("cameraPosition.z should be preserved");
	}

	// ── Test 5: 크롭 프레임 (Crop Frame X, Y) 투영 바운드 고정 검증 ──
	{
		pixelPerfect.cropFrameX = true;
		pixelPerfect.cropFrameY = true;
		CameraSystem::UpdateCamera(registry, entity, 1400, 900);

		auto buffer = camera.GetCameraBuffer();
		if (!buffer.has_value()) return Fail("CameraBuffer is empty with crop frames");

		// 투영 행렬에서 ortho bounds 확인:
		// halfHeight = 180 / 16 * 0.5 = 5.625
		// halfWidth = 320 / 16 * 0.5 = 10.0
		// glm::orthoLH_ZO에서 proj[0][0] = 1 / halfWidth = 1 / 10 = 0.1
		// proj[1][1] = 1 / halfHeight = 1 / 5.625 = 0.1777777...
		if (!Near(buffer->projMatrix[0][0], 1.0f / 10.0f))
			return Fail("Cropped ortho projMatrix[0][0] does not match refResolutionX");
		if (!Near(buffer->projMatrix[1][1], 1.0f / 5.625f))
			return Fail("Cropped ortho projMatrix[1][1] does not match refResolutionY");
	}

	std::cout << "[PASS] All PixelPerfectCamera tests passed successfully!" << std::endl;
	return 0;
}
