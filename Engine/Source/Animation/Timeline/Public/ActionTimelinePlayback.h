#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace Engine
{
	// 재생 세션이 끝난 사유. 그래프 핀에는 정수로 전달되므로 순서를 바꾸지 않는다.
	enum class EActionTimelineEndReason : std::uint8_t
	{
		Completed,   // 끝 시점까지 재생했다.
		Stopped,     // Stop으로 멈췄다.
		Interrupted  // 재시작·에셋 교체·다른 타임라인의 Animator 인계로 끊겼다.
	};

	struct ActionTimelinePlaybackStep
	{
		float PreviousTime = 0.0f;
		float CurrentTime = 0.0f;
		float Delta = 0.0f;
		float Duration = 0.0f;
		std::size_t LoopCount = 0;
		bool Looping = false;
		bool IncludeStart = false;
		bool Finished = false;
	};

	inline ActionTimelinePlaybackStep AdvanceActionTimeline(float currentTime, float delta, float duration, bool looping, bool includeStart)
	{
		ActionTimelinePlaybackStep result;
		result.PreviousTime = currentTime;
		result.CurrentTime = currentTime;
		result.Delta = delta;
		result.Duration = duration;
		result.Looping = looping;
		result.IncludeStart = includeStart;
		if (!std::isfinite(currentTime) || !std::isfinite(delta) || !std::isfinite(duration) || duration <= 0.0f)
			return result;

		if (delta < 0.0f)
		{
			result.CurrentTime = currentTime + delta;
			return result;
		}

		if (!looping)
		{
			const float nextTime = currentTime + delta;
			result.CurrentTime = nextTime >= duration ? duration : nextTime;
			result.Finished = nextTime >= duration;
			return result;
		}

		const float normalizedTime = std::fmod(currentTime, duration);
		result.PreviousTime = normalizedTime < 0.0f ? normalizedTime + duration : normalizedTime;
		const double elapsed = static_cast<double>(result.PreviousTime) + static_cast<double>(delta);
		const double loopCount = std::floor(elapsed / static_cast<double>(duration));
		result.LoopCount = loopCount >= static_cast<double>(std::numeric_limits<std::size_t>::max())
			? std::numeric_limits<std::size_t>::max() : static_cast<std::size_t>(loopCount);
		result.CurrentTime = static_cast<float>(std::fmod(elapsed, static_cast<double>(duration)));
		return result;
	}

	inline std::size_t CountActionTimelineCrossings(const ActionTimelinePlaybackStep& step, float time)
	{
		if (!std::isfinite(time) || !std::isfinite(step.PreviousTime) || !std::isfinite(step.Delta) ||
			!std::isfinite(step.Duration) || step.Duration <= 0.0f || time < 0.0f || time > step.Duration || step.Delta < 0.0f)
			return 0;

		if (!step.Looping)
		{
			const bool crossed = (step.IncludeStart ? time >= step.PreviousTime : time > step.PreviousTime) &&
				time <= step.CurrentTime;
			return crossed ? 1 : 0;
		}

		if (step.Delta == 0.0f)
		{
			return step.IncludeStart && time == step.PreviousTime ? 1 : 0;
		}

		const double duration = static_cast<double>(step.Duration);
		const double start = static_cast<double>(step.PreviousTime);
		const double firstDistance = time == step.Duration
			? duration - start
			: (time > step.PreviousTime || (step.IncludeStart && time == step.PreviousTime)
				? static_cast<double>(time) - start
				: duration - start + static_cast<double>(time));
		const double total = static_cast<double>(step.Delta);
		if (firstDistance > total) return 0;
		const double crossings = std::floor((total - firstDistance) / duration) + 1.0;
		return crossings >= static_cast<double>(std::numeric_limits<std::size_t>::max())
			? std::numeric_limits<std::size_t>::max() : static_cast<std::size_t>(crossings);
	}

}
