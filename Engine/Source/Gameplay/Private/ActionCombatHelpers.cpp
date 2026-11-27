#include "ActionCombatHelpers.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <mutex>

namespace Engine
{
	namespace ActionCombatDetail
	{
		constexpr f32 TwoPi = 6.28318530717958647692f;
		constexpr f32 EpsilonSq = 1e-8f;

		bool IsFiniteVec3(const vec3& v)
		{
			return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
		}

		bool TryNormalize(const vec3& v, vec3& outNormalized)
		{
			if (!IsFiniteVec3(v))
				return false;

			const f32 lenSq = glm::dot(v, v);
			if (lenSq <= EpsilonSq)
				return false;

			outNormalized = v / std::sqrt(lenSq);
			return true;
		}
	}

	// ═══════════════════════════════════════════════════════════════════════════
	// HitstopState
	// ═══════════════════════════════════════════════════════════════════════════

	void HitstopState::Trigger(f32 duration, f32 timeScale)
	{
		if (!std::isfinite(duration) || duration <= 0.0f)
			return;

		const f32 safeScale = std::isfinite(timeScale) ? std::clamp(timeScale, 0.0f, 1.0f) : 0.0f;
		if (IsActive())
		{
			remainingDuration = std::max(remainingDuration, duration);
			activeTimeScale = std::min(activeTimeScale, safeScale);
		}
		else
		{
			remainingDuration = duration;
			activeTimeScale = safeScale;
		}
	}

	void HitstopState::Update(f32 unscaledDt)
	{
		if (!IsActive() || !std::isfinite(unscaledDt) || unscaledDt <= 0.0f)
			return;

		remainingDuration -= unscaledDt;
		if (remainingDuration <= 0.0f)
		{
			Reset();
		}
	}

	f32 HitstopState::GetEffectiveTimeScale() const
	{
		return IsActive() ? activeTimeScale : 1.0f;
	}

	bool HitstopState::IsActive() const
	{
		return remainingDuration > 0.0f;
	}

	void HitstopState::Reset()
	{
		remainingDuration = 0.0f;
		activeTimeScale = 1.0f;
	}

	// ═══════════════════════════════════════════════════════════════════════════
	// CameraShakeState
	// ═══════════════════════════════════════════════════════════════════════════

	void CameraShakeState::Trigger(f32 newIntensity, f32 duration, f32 newFrequency)
	{
		if (!std::isfinite(newIntensity) || newIntensity <= 0.0f || !std::isfinite(duration) || duration <= 0.0f)
			return;

		// 중첩 시 현재 감쇠된 진폭보다 크거나 같을 때만 새 셰이크로 덮어쓴다.
		if (IsActive() && newIntensity < GetCurrentAmplitude())
			return;

		intensity = newIntensity;
		totalDuration = duration;
		remainingDuration = duration;
		frequency = (std::isfinite(newFrequency) && newFrequency > 0.0f) ? newFrequency : 25.0f;
		elapsed = 0.0f;
	}

	void CameraShakeState::Update(f32 dt)
	{
		if (!IsActive())
			return;

		if (!std::isfinite(dt) || dt <= 0.0f)
			return;

		elapsed += dt;
		remainingDuration -= dt;
		if (remainingDuration <= 0.0f)
		{
			Reset();
			return;
		}

		const f32 envelope = GetCurrentAmplitude();
		const f32 omega = ActionCombatDetail::TwoPi * frequency;

		positionOffset.x = envelope * std::sin(omega * elapsed);
		positionOffset.y = envelope * std::sin(omega * elapsed * 1.13f + 1.57f);
		positionOffset.z = envelope * std::cos(omega * elapsed * 0.87f + 0.78f) * 0.5f;

		rotationOffsetDeg.x = envelope * 1.5f * std::sin(omega * elapsed * 1.27f + 0.4f);
		rotationOffsetDeg.y = envelope * 1.5f * std::cos(omega * elapsed * 0.93f + 1.1f);
		rotationOffsetDeg.z = envelope * 2.5f * std::sin(omega * elapsed * 1.41f + 2.3f);
	}

	void CameraShakeState::Reset()
	{
		intensity = 0.0f;
		totalDuration = 0.0f;
		remainingDuration = 0.0f;
		frequency = 25.0f;
		elapsed = 0.0f;
		positionOffset = vec3(0.0f);
		rotationOffsetDeg = vec3(0.0f);
	}

	f32 CameraShakeState::GetCurrentAmplitude() const
	{
		if (remainingDuration <= 0.0f || totalDuration <= 0.0f)
			return 0.0f;

		const f32 t = std::clamp(remainingDuration / totalDuration, 0.0f, 1.0f);
		return intensity * (t * t);
	}

	// ═══════════════════════════════════════════════════════════════════════════
	// CombatEffectService
	// ═══════════════════════════════════════════════════════════════════════════

	namespace
	{
		std::mutex& GetCombatEffectMutex()
		{
			static std::mutex s_mutex;
			return s_mutex;
		}

		HitstopState& GetCombatHitstopState()
		{
			static HitstopState s_hitstop;
			return s_hitstop;
		}

		CameraShakeState& GetCombatCameraShakeState()
		{
			static CameraShakeState s_cameraShake;
			return s_cameraShake;
		}
	}

	void CombatEffectService::TriggerHitstop(f32 duration, f32 timeScale)
	{
		std::lock_guard<std::mutex> lock(GetCombatEffectMutex());
		GetCombatHitstopState().Trigger(duration, timeScale);
	}

	bool CombatEffectService::IsHitstopActive()
	{
		std::lock_guard<std::mutex> lock(GetCombatEffectMutex());
		return GetCombatHitstopState().IsActive();
	}

	void CombatEffectService::TriggerCameraShake(f32 intensity, f32 duration, f32 frequency)
	{
		std::lock_guard<std::mutex> lock(GetCombatEffectMutex());
		GetCombatCameraShakeState().Trigger(intensity, duration, frequency);
	}

	bool CombatEffectService::IsCameraShakeActive()
	{
		std::lock_guard<std::mutex> lock(GetCombatEffectMutex());
		return GetCombatCameraShakeState().IsActive();
	}

	f32 CombatEffectService::Update(f32 unscaledDt)
	{
		std::lock_guard<std::mutex> lock(GetCombatEffectMutex());
		auto& hitstop = GetCombatHitstopState();
		hitstop.Update(unscaledDt);
		GetCombatCameraShakeState().Update(unscaledDt);
		return hitstop.GetEffectiveTimeScale();
	}

	vec3 CombatEffectService::GetCameraShakePositionOffset()
	{
		std::lock_guard<std::mutex> lock(GetCombatEffectMutex());
		return GetCombatCameraShakeState().GetPositionOffset();
	}

	vec3 CombatEffectService::GetCameraShakeRotationOffsetDeg()
	{
		std::lock_guard<std::mutex> lock(GetCombatEffectMutex());
		return GetCombatCameraShakeState().GetRotationOffsetDeg();
	}

	void CombatEffectService::Reset()
	{
		std::lock_guard<std::mutex> lock(GetCombatEffectMutex());
		GetCombatHitstopState().Reset();
		GetCombatCameraShakeState().Reset();
	}

	// ═══════════════════════════════════════════════════════════════════════════
	// TargetLockOnSelector
	// ═══════════════════════════════════════════════════════════════════════════

	std::optional<uint64> TargetLockOnSelector::FindBestTarget(
		std::span<const TargetCandidate> candidates,
		const vec3& cameraPos,
		const vec3& cameraForward,
		f32 maxDistance,
		f32 maxFovDeg,
		f32 distanceWeight,
		f32 angleWeight)
	{
		if (candidates.empty() || !ActionCombatDetail::IsFiniteVec3(cameraPos))
			return std::nullopt;
		if (!std::isfinite(maxDistance) || maxDistance <= 0.0f || !std::isfinite(maxFovDeg) || maxFovDeg <= 0.0f)
			return std::nullopt;

		vec3 fwd{};
		if (!ActionCombatDetail::TryNormalize(cameraForward, fwd))
			return std::nullopt;

		const f32 clampedFov = std::clamp(maxFovDeg, 0.0f, 360.0f);
		const f32 halfFovRad = glm::radians(clampedFov * 0.5f);
		const f32 cosHalfFov = std::cos(halfFovRad);
		const f32 safeDistW = std::isfinite(distanceWeight) ? std::max(0.0f, distanceWeight) : 0.4f;
		const f32 safeAngleW = std::isfinite(angleWeight) ? std::max(0.0f, angleWeight) : 0.6f;

		std::optional<uint64> bestId = std::nullopt;
		f32 bestScore = std::numeric_limits<f32>::max();

		for (const TargetCandidate& candidate : candidates)
		{
			if (!ActionCombatDetail::IsFiniteVec3(candidate.position))
				continue;

			const vec3 toTarget = candidate.position - cameraPos;
			const f32 distSq = glm::dot(toTarget, toTarget);
			if (distSq > maxDistance * maxDistance)
				continue;

			f32 dist = 0.0f;
			f32 angleRad = 0.0f;
			if (distSq > ActionCombatDetail::EpsilonSq)
			{
				dist = std::sqrt(distSq);
				const vec3 dir = toTarget / dist;
				const f32 cosAngle = std::clamp(glm::dot(fwd, dir), -1.0f, 1.0f);
				if (cosAngle < cosHalfFov)
					continue;
				angleRad = std::acos(cosAngle);
			}

			const f32 distNorm = dist / maxDistance;
			const f32 angleNorm = (halfFovRad > 1e-5f) ? (angleRad / halfFovRad) : 0.0f;
			const f32 score = safeDistW * distNorm + safeAngleW * angleNorm;

			if (score < bestScore)
			{
				bestScore = score;
				bestId = candidate.id;
			}
		}

		return bestId;
	}

	std::optional<uint64> TargetLockOnSelector::SwitchTarget(
		std::span<const TargetCandidate> candidates,
		const vec3& cameraPos,
		const vec3& cameraForward,
		const vec3& cameraRight,
		f32 maxDistance,
		f32 maxFovDeg,
		uint64 currentTargetId,
		bool switchRight)
	{
		if (candidates.empty() || !ActionCombatDetail::IsFiniteVec3(cameraPos))
			return std::nullopt;
		if (!std::isfinite(maxDistance) || maxDistance <= 0.0f || !std::isfinite(maxFovDeg) || maxFovDeg <= 0.0f)
			return std::nullopt;

		vec3 fwd{};
		vec3 right{};
		if (!ActionCombatDetail::TryNormalize(cameraForward, fwd) || !ActionCombatDetail::TryNormalize(cameraRight, right))
			return std::nullopt;

		const f32 clampedFov = std::clamp(maxFovDeg, 0.0f, 360.0f);
		const f32 cosHalfFov = std::cos(glm::radians(clampedFov * 0.5f));

		f32 referenceAngle = 0.0f;
		for (const TargetCandidate& candidate : candidates)
		{
			if (candidate.id != currentTargetId || !ActionCombatDetail::IsFiniteVec3(candidate.position))
				continue;

			const vec3 toCurrent = candidate.position - cameraPos;
			const f32 lenSq = glm::dot(toCurrent, toCurrent);
			if (lenSq > ActionCombatDetail::EpsilonSq)
			{
				const vec3 dir = toCurrent / std::sqrt(lenSq);
				referenceAngle = std::atan2(glm::dot(dir, right), glm::dot(dir, fwd));
			}
			break;
		}

		std::optional<uint64> bestId = std::nullopt;
		f32 bestAngleDelta = std::numeric_limits<f32>::max();
		f32 bestDistSq = std::numeric_limits<f32>::max();

		for (const TargetCandidate& candidate : candidates)
		{
			if (candidate.id == currentTargetId || !ActionCombatDetail::IsFiniteVec3(candidate.position))
				continue;

			const vec3 toTarget = candidate.position - cameraPos;
			const f32 distSq = glm::dot(toTarget, toTarget);
			if (distSq <= ActionCombatDetail::EpsilonSq || distSq > maxDistance * maxDistance)
				continue;

			const vec3 dir = toTarget / std::sqrt(distSq);
			const f32 cosAngle = std::clamp(glm::dot(fwd, dir), -1.0f, 1.0f);
			if (cosAngle < cosHalfFov)
				continue;

			const f32 signedAngle = std::atan2(glm::dot(dir, right), glm::dot(dir, fwd));
			const f32 delta = switchRight ? (signedAngle - referenceAngle) : (referenceAngle - signedAngle);
			if (delta <= 1e-4f)
				continue;

			if (delta < bestAngleDelta - 1e-4f || (std::abs(delta - bestAngleDelta) <= 1e-4f && distSq < bestDistSq))
			{
				bestAngleDelta = delta;
				bestDistSq = distSq;
				bestId = candidate.id;
			}
		}

		return bestId;
	}

	bool TargetLockOnSelector::IsTargetValid(const vec3& targetPos, const vec3& cameraPos, f32 maxDistance)
	{
		if (!ActionCombatDetail::IsFiniteVec3(targetPos) || !ActionCombatDetail::IsFiniteVec3(cameraPos))
			return false;
		if (!std::isfinite(maxDistance) || maxDistance <= 0.0f)
			return false;

		const vec3 delta = targetPos - cameraPos;
		return glm::dot(delta, delta) <= maxDistance * maxDistance;
	}
}
