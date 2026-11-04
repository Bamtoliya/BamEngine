#include "AnimatorStateMachine.h"
#include "AnimationEvaluator.h"
#include "Reflection/ReflectionRegistry.h"
#include <archive/JsonArchive.h>
#include <reflection_archive/ReflectionArchiveAdapter.h>
#include "PlatformPath.h"

void AnimatorParameters::SetFloat(const std::string& name, float value)
{
	auto& param = m_parameters[name];
	param.type = EAnimatorParamType::Float;
	param.floatValue = value;
}

void AnimatorParameters::SetInt(const std::string& name, int32 value)
{
	auto& param = m_parameters[name];
	param.type = EAnimatorParamType::Int;
	param.intValue = value;
}

void AnimatorParameters::SetBool(const std::string& name, bool value)
{
	auto& param = m_parameters[name];
	param.type = EAnimatorParamType::Bool;
	param.boolValue = value;
}

void AnimatorParameters::SetTrigger(const std::string& name)
{
	auto& param = m_parameters[name];
	param.type = EAnimatorParamType::Trigger;
	param.boolValue = true;
}

void AnimatorParameters::ResetTrigger(const std::string& name)
{
	auto it = m_parameters.find(name);
	if (it != m_parameters.end() && it->second.type == EAnimatorParamType::Trigger)
		it->second.boolValue = false;
}

float AnimatorParameters::GetFloat(const std::string& name, float defaultValue) const
{
	auto it = m_parameters.find(name);
	return it != m_parameters.end() ? it->second.floatValue : defaultValue;
}

int32 AnimatorParameters::GetInt(const std::string& name, int32 defaultValue) const
{
	auto it = m_parameters.find(name);
	return it != m_parameters.end() ? it->second.intValue : defaultValue;
}

bool AnimatorParameters::GetBool(const std::string& name, bool defaultValue) const
{
	auto it = m_parameters.find(name);
	return it != m_parameters.end() ? it->second.boolValue : defaultValue;
}

bool AnimatorParameters::GetTrigger(const std::string& name) const
{
	auto it = m_parameters.find(name);
	return it != m_parameters.end() ? it->second.boolValue : false;
}

bool TransitionCondition::Evaluate(const AnimatorParameters& params) const
{
	const auto& all = params.GetAll();
	auto it = all.find(parameterName);
	if (it == all.end()) return false;

	const auto& p = it->second;
	switch (mode)
	{
	case EConditionMode::Greater:
		return p.type == EAnimatorParamType::Int ? (p.intValue > threshold) : (p.floatValue > threshold);
	case EConditionMode::Less:
		return p.type == EAnimatorParamType::Int ? (p.intValue < threshold) : (p.floatValue < threshold);
	case EConditionMode::Equal:
		return p.type == EAnimatorParamType::Int ? (p.intValue == static_cast<int32>(threshold)) : (std::abs(p.floatValue - threshold) < 0.0001f);
	case EConditionMode::NotEqual:
		return p.type == EAnimatorParamType::Int ? (p.intValue != static_cast<int32>(threshold)) : (std::abs(p.floatValue - threshold) >= 0.0001f);
	case EConditionMode::IfTrue:
		return p.boolValue;
	case EConditionMode::IfFalse:
		return !p.boolValue;
	default:
		return false;
	}
}

bool AnimTransition::CanTransition(float currentNormalizedTime, const AnimatorParameters& params) const
{
	if (hasExitTime && currentNormalizedTime < exitTime) return false;
	for (const auto& cond : conditions)
	{
		if (!cond.Evaluate(params)) return false;
	}
	return true;
}

const AnimState* AnimatorStateMachine::FindState(const std::string& name) const
{
	for (const auto& st : states)
	{
		if (st.name == name) return &st;
	}
	return nullptr;
}

std::vector<const AnimTransition*> AnimatorStateMachine::GetTransitionsFrom(const std::string& stateName) const
{
	std::vector<const AnimTransition*> result;
	for (const auto& tr : transitions)
	{
		if (tr.sourceState == stateName) result.push_back(&tr);
	}
	return result;
}

void AnimatorRuntimeState::BindStateMachine(const std::shared_ptr<AnimatorStateMachine>& sm)
{
	m_stateMachine = sm;
	if (!m_stateMachine) return;
	m_currentStateName = sm->defaultState;
	m_currentStateTime = 0.0f;
	m_previousStateTime = 0.0f;
	m_isTransitioning = false;
	m_parameters.GetAll() = sm->defaultParameters;
	m_triggeredNotifies.clear();
}
void AnimatorRuntimeState::Reset()
{
	m_currentStateTime = 0.0f;
	m_previousStateTime = 0.0f;
	m_targetStateTime = 0.0f;
	m_transitionElapsed = 0.0f;
	m_isTransitioning = false;
	m_targetStateName.clear();
	m_triggeredNotifies.clear();
	if (m_stateMachine) m_currentStateName = m_stateMachine->defaultState;
}

void AnimatorRuntimeState::Update(float dt, const Skeleton& skeleton, std::vector<mat4>& outBoneMatrices)
{
	if (!m_stateMachine) return;
	const auto* curState = m_stateMachine->FindState(m_currentStateName);
	if (!curState) return;

	// 1. 현재 상태의 주 애니메이션 클립 및 지속 시간 계산
	const Animation* primaryClip = nullptr;
	const Animation* blendSecondaryClip = nullptr;
	float blendWeight = 0.0f;

	if (curState->IsBlendTree())
	{
		const float paramVal = m_parameters.GetFloat(curState->blendTree.parameterName);
		curState->blendTree.Evaluate(paramVal, primaryClip, blendSecondaryClip, blendWeight);
	}
	else
	{
		primaryClip = curState->clip.Get();
	}

	if (!primaryClip) return;

	const f64 curDurationSec = primaryClip->GetDuration() / (primaryClip->GetTicksPerSecond() > 0.0 ? primaryClip->GetTicksPerSecond() : 25.0);
	const float normalizedTime = curDurationSec > 0.0 ? static_cast<float>(m_currentStateTime / curDurationSec) : 0.0f;

	// 2. 전이 조건 평가 (전이 중이 아닐 때만 새 전이 검사)
	if (!m_isTransitioning)
	{
		const auto transitions = m_stateMachine->GetTransitionsFrom(m_currentStateName);
		for (const auto* tr : transitions)
		{
			if (tr->CanTransition(normalizedTime, m_parameters))
			{
				m_isTransitioning = true;
				m_targetStateName = tr->targetState;
				m_targetStateTime = 0.0f;
				m_transitionElapsed = 0.0f;
				m_transitionDuration = std::max(0.001f, tr->duration);

				for (const auto& cond : tr->conditions)
				{
					m_parameters.ResetTrigger(cond.parameterName);
				}
				break;
			}
		}
	}

	// 3. 시간 진행, 노티파이 검출 및 비반복 루프 처리
	const float prevTime = m_currentStateTime;
	m_currentStateTime += dt * curState->playRate;
	if (curState->isLooping && curDurationSec > 0.0)
	{
		const float dur = static_cast<float>(curDurationSec);
		const float wrappedTime = std::fmod(m_currentStateTime, dur);
		CheckAndTriggerNotifies(*curState, prevTime, (m_currentStateTime >= dur ? wrappedTime : m_currentStateTime), dur, true);
		m_currentStateTime = wrappedTime;
	}
	else if (curDurationSec > 0.0)
	{
		const float dur = static_cast<float>(curDurationSec);
		const float clampedTime = std::min(m_currentStateTime, dur);
		CheckAndTriggerNotifies(*curState, prevTime, clampedTime, dur, false);
		m_currentStateTime = clampedTime;
	}
	else
	{
		CheckAndTriggerNotifies(*curState, prevTime, m_currentStateTime, 0.0f, false);
	}
	m_previousStateTime = m_currentStateTime;
	// 4. 포즈 평가 및 블렌딩
	if (m_isTransitioning)
	{
		const auto* targetState = m_stateMachine->FindState(m_targetStateName);
		const Animation* targetClip = nullptr;
		if (targetState)
		{
			if (targetState->IsBlendTree())
			{
				const Animation* secClip = nullptr;
				float secWeight = 0.0f;
				targetState->blendTree.Evaluate(m_parameters.GetFloat(targetState->blendTree.parameterName), targetClip, secClip, secWeight);
			}
			else
			{
				targetClip = targetState->clip.Get();
			}
		}
		if (targetClip)
		{
			m_targetStateTime += dt * targetState->playRate;
			const f64 targetDurationSec = targetClip->GetDuration() / (targetClip->GetTicksPerSecond() > 0.0 ? targetClip->GetTicksPerSecond() : 25.0);
			if (!targetState->isLooping && targetDurationSec > 0.0)
			{
				m_targetStateTime = std::min(m_targetStateTime, static_cast<float>(targetDurationSec));
			}
			m_transitionElapsed += dt;
			const float alpha = m_transitionElapsed / m_transitionDuration;
			AnimationEvaluator::EvaluateBlendedSkinningMatrices(*primaryClip, m_currentStateTime, *targetClip, m_targetStateTime, alpha, skeleton, outBoneMatrices, curState->isLooping, targetState->isLooping);
			if (alpha >= 1.0f)
			{
				m_currentStateName = m_targetStateName;
				m_currentStateTime = m_targetStateTime;
				m_isTransitioning = false;
			}
		}
		else
		{
			m_isTransitioning = false;
			AnimationEvaluator::EvaluateSkinningMatrices(*primaryClip, skeleton, m_currentStateTime, outBoneMatrices, curState->isLooping);
		}
	}
	else
	{
		if (curState->IsBlendTree() && blendSecondaryClip && blendWeight > 0.0f)
		{
			// [위상 동기화 (Phase Matching)]: 두 클립의 길이가 달라도 발걸음 주기(Normalized Time)를 일치시켜 블렌딩
			const f64 secTps = blendSecondaryClip->GetTicksPerSecond() > 0.0 ? blendSecondaryClip->GetTicksPerSecond() : 25.0;
			const f64 secDurationSec = blendSecondaryClip->GetDuration() / secTps;
			const float normTime = (curDurationSec > 0.0) ? (m_currentStateTime / static_cast<float>(curDurationSec)) : 0.0f;
			const float secondaryTime = (secDurationSec > 0.0) ? (normTime * static_cast<float>(secDurationSec)) : m_currentStateTime;

			AnimationEvaluator::EvaluateBlendedSkinningMatrices(*primaryClip, m_currentStateTime, *blendSecondaryClip, secondaryTime, blendWeight, skeleton, outBoneMatrices, curState->isLooping, curState->isLooping);
		}
		else
		{
			AnimationEvaluator::EvaluateSkinningMatrices(*primaryClip, skeleton, m_currentStateTime, outBoneMatrices, curState->isLooping, curState->loopBlendDuration);
		}
	}
}

void AnimatorRuntimeState::CheckAndTriggerNotifies(const AnimState& state, float prevTime, float newTime, float duration, bool isLooping)
{
	if (state.notifies.empty() || duration <= 0.0f) return;
	for (const auto& notify : state.notifies)
	{
		const float t = notify.triggerTime;
		bool triggered = false;
		if (prevTime <= newTime)
		{
			// 일반 순방향 재생 구간
			if (prevTime < t && t <= newTime) triggered = true;
		}
		else if (isLooping)
		{
			// 루프로 인해 타임라인이 끝에서 시작으로 래핑된 경우: [prevTime, duration] 또는 [0, newTime]
			if ((prevTime < t && t <= duration) || (0.0f <= t && t <= newTime)) triggered = true;
		}
		if (triggered)
		{
			m_triggeredNotifies.push_back(AnimNotifyEvent{ state.name, notify.name, notify.payload, t });
		}
	}
}

bool AnimatorStateMachine::SaveToFile(const std::string& filePath) const
{
	const auto& refl = Engine::ReflectionRegistry::Get();
	const auto* type = refl.GetRegistry().FindType<AnimatorStateMachine>();
	if (!type)
	{
		BAM_LOG(Error, "Engine", "Asset", "[Animator] Reflection type not found for AnimatorStateMachine");
		return false;
	}
	const std::string resolvedPath = PlatformPath::ResolveProjectRelative(filePath).string();
	archive::JsonArchive ar;
	const auto writeRes = archive_reflection::WriteObject(ar, "AnimatorController", *type, reflection::ObjectView::From(*this), &refl.GetRegistry(), &refl.GetPropertyCodecs());
	if (!writeRes)
	{
		BAM_LOG(Error, "Engine", "Asset", "[Animator] WriteObject failed: code={}, field={}", static_cast<int>(writeRes.Code), writeRes.Field);
		return false;
	}
	const auto saveRes = ar.SaveToFile(resolvedPath);
	if (!saveRes)
	{
		BAM_LOG(Error, "Engine", "Asset", "[Animator] SaveToFile failed: code={}, field={}, path={}", static_cast<int>(saveRes.Code), saveRes.Field, resolvedPath);
		return false;
	}
	return true;
}
bool AnimatorStateMachine::LoadFromFile(const std::string& filePath)
{
	const std::string resolvedPath = PlatformPath::ResolveProjectRelative(filePath).string();
	archive::JsonArchive ar;
	const auto loadRes = ar.LoadFromFile(resolvedPath);
	if (!loadRes)
	{
		BAM_LOG(Error, "Engine", "Asset", "[Animator] LoadFromFile failed: code={}, field={}, path={}", static_cast<int>(loadRes.Code), loadRes.Field, resolvedPath);
		return false;
	}
	const auto& refl = Engine::ReflectionRegistry::Get();
	const auto* type = refl.GetRegistry().FindType<AnimatorStateMachine>();
	if (!type)
	{
		BAM_LOG(Error, "Engine", "Asset", "[Animator] Reflection type not found for AnimatorStateMachine");
		return false;
	}
	AnimatorStateMachine temp;
	const auto readRes = archive_reflection::ReadObject(ar, "AnimatorController", *type, temp, &refl.GetRegistry(), &refl.GetPropertyCodecs());
	if (!readRes)
	{
		BAM_LOG(Error, "Engine", "Asset", "[Animator] ReadObject failed: code={}, field={}", static_cast<int>(readRes.Code), readRes.Field);
		return false;
	}
	*this = std::move(temp);
	return true;
}

const Animation* AnimatorRuntimeState::GetCurrentClip() const
{
	if (!m_stateMachine) return nullptr;
	const auto* st = m_stateMachine->FindState(m_currentStateName);
	if (!st) return nullptr;
	if (st->IsBlendTree())
	{
		const Animation* primary = nullptr;
		const Animation* secondary = nullptr;
		float weight = 0.0f;
		st->blendTree.Evaluate(m_parameters.GetFloat(st->blendTree.parameterName), primary, secondary, weight);
		return primary;
	}
	return st->clip.Get();
}