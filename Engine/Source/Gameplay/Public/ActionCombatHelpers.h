#pragma once

#include "Engine_Includes.h"
#include <optional>
#include <span>

namespace Engine
{
	// ── 히트스톱(Hitstop) 상태 ──
	// 타격 순간의 시간 배율 감속을 비스케일 델타 타임(unscaledDt) 기준으로 관리한다.
	class ENGINE_API HitstopState
	{
	public:
		void Trigger(f32 duration, f32 timeScale = 0.0f);
		void Update(f32 unscaledDt);

		[[nodiscard]] f32 GetEffectiveTimeScale() const;
		[[nodiscard]] bool IsActive() const;
		void Reset();

	private:
		f32 remainingDuration = 0.0f;
		f32 activeTimeScale = 1.0f;
	};

	// ── 카메라 셰이크(Camera Shake) 상태 ──
	// 감쇠 진동 기반 위치/회전(도 단위) 오프셋을 생성하며, 중첩 시 더 강한 셰이크를 우선한다.
	class ENGINE_API CameraShakeState
	{
	public:
		void Trigger(f32 intensity, f32 duration, f32 frequency = 25.0f);
		void Update(f32 dt);

		[[nodiscard]] const vec3& GetPositionOffset() const { return positionOffset; }
		[[nodiscard]] const vec3& GetRotationOffsetDeg() const { return rotationOffsetDeg; }
		[[nodiscard]] bool IsActive() const { return remainingDuration > 0.0f; }
		void Reset();

	private:
		f32 GetCurrentAmplitude() const;

	private:
		f32 intensity = 0.0f;
		f32 totalDuration = 0.0f;
		f32 remainingDuration = 0.0f;
		f32 frequency = 25.0f;
		f32 elapsed = 0.0f;

		vec3 positionOffset{ 0.0f };
		vec3 rotationOffsetDeg{ 0.0f };
	};

	// ── 타깃 락온 후보 ──
	struct ENGINE_API TargetCandidate
	{
		uint64 id = 0;
		vec3 position{ 0.0f };
	};

	// ── 타깃 락온 선택기 ──
	// 카메라 시야각(FOV), 거리 가중치, 좌/우 인접 방향을 기준으로 락온 대상을 선택·전환·검증한다.
	class ENGINE_API TargetLockOnSelector
	{
	public:
		[[nodiscard]] static std::optional<uint64> FindBestTarget(
			std::span<const TargetCandidate> candidates,
			const vec3& cameraPos,
			const vec3& cameraForward,
			f32 maxDistance,
			f32 maxFovDeg,
			f32 distanceWeight = 0.4f,
			f32 angleWeight = 0.6f);

		[[nodiscard]] static std::optional<uint64> SwitchTarget(
			std::span<const TargetCandidate> candidates,
			const vec3& cameraPos,
			const vec3& cameraForward,
			const vec3& cameraRight,
			f32 maxDistance,
			f32 maxFovDeg,
			uint64 currentTargetId,
			bool switchRight);

		[[nodiscard]] static bool IsTargetValid(
			const vec3& targetPos,
			const vec3& cameraPos,
			f32 maxDistance);
	};
}
