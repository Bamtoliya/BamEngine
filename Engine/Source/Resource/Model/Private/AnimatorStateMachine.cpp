#include "AnimatorStateMachine.h"
#include "AnimationEvaluator.h"
#include "Reflection/ReflectionRegistry.h"
#include <archive/JsonArchive.h>
#include <reflection_archive/ReflectionArchiveAdapter.h>

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
	m_isTransitioning = false;
	m_parameters.GetAll() = sm->defaultParameters;
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

	// 3. 시간 진행
	m_currentStateTime += dt * curState->playRate;

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
			m_transitionElapsed += dt;
			const float alpha = m_transitionElapsed / m_transitionDuration;

			AnimationEvaluator::EvaluateBlendedSkinningMatrices(*primaryClip, m_currentStateTime, *targetClip, m_targetStateTime, alpha, skeleton, outBoneMatrices);

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
			AnimationEvaluator::EvaluateSkinningMatrices(*primaryClip, skeleton, m_currentStateTime, outBoneMatrices);
		}
	}
	else
	{
		// 블렌드 트리 활성 상태인 경우 인접 두 클립 블렌딩
		if (curState->IsBlendTree() && blendSecondaryClip && blendWeight > 0.0f)
		{
			AnimationEvaluator::EvaluateBlendedSkinningMatrices(*primaryClip, m_currentStateTime, *blendSecondaryClip, m_currentStateTime, blendWeight, skeleton, outBoneMatrices);
		}
		else
		{
			AnimationEvaluator::EvaluateSkinningMatrices(*primaryClip, skeleton, m_currentStateTime, outBoneMatrices);
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

	archive::JsonArchive ar;
	const auto writeRes = archive_reflection::WriteObject(ar, "AnimatorController", *type, reflection::ObjectView::From(*this), &refl.GetRegistry(), &refl.GetPropertyCodecs());
	if (!writeRes)
	{
		BAM_LOG(Error, "Engine", "Asset", "[Animator] WriteObject failed: code={}, field={}", static_cast<int>(writeRes.Code), writeRes.Field);
		return false;
	}

	const auto saveRes = ar.SaveToFile(filePath);
	if (!saveRes)
	{
		BAM_LOG(Error, "Engine", "Asset", "[Animator] SaveToFile failed: code={}, field={}, path={}", static_cast<int>(saveRes.Code), saveRes.Field, filePath);
		return false;
	}
	return true;
}

bool AnimatorStateMachine::LoadFromFile(const std::string& filePath)
{
	archive::JsonArchive ar;
	const auto loadRes = ar.LoadFromFile(filePath);
	if (!loadRes)
	{
		BAM_LOG(Error, "Engine", "Asset", "[Animator] LoadFromFile failed: code={}, field={}, path={}", static_cast<int>(loadRes.Code), loadRes.Field, filePath);
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