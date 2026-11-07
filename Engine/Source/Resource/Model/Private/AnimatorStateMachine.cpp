#include "AnimatorStateMachine.h"
#include "AnimationEvaluator.h"
#include "ReflectionRegistry.h"
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

namespace
{
	bool MatchAnimatorConditionList(const std::vector<TransitionCondition>& conditions,
		ETransitionConditionLogic logic, const AnimatorParameters& params)
	{
		if (conditions.empty()) return false;

		const auto matches = [&](const TransitionCondition& condition)
			{
				return condition.Evaluate(params);
			};

		if (logic == ETransitionConditionLogic::Any)
			return std::any_of(conditions.begin(), conditions.end(), matches);

		return std::all_of(conditions.begin(), conditions.end(), matches);
	}
}

bool AnimTransition::CanTransition(float currentNormalizedTime, const AnimatorParameters& params) const
{
	if (fromAnyState && conditions.empty() && conditionGroups.empty()) return false;
	if (!fromAnyState && hasExitTime && currentNormalizedTime < exitTime) return false;

	if (conditionGroups.empty())
		return conditions.empty() || MatchAnimatorConditionList(conditions, conditionLogic, params);

	const bool requireAll = groupLogic == ETransitionConditionLogic::All;

	if (!conditions.empty() && MatchAnimatorConditionList(conditions, conditionLogic, params) != requireAll)
		return !requireAll;

	for (const auto& group : conditionGroups)
	{
		if (MatchAnimatorConditionList(group.conditions, group.conditionLogic, params) != requireAll)
			return !requireAll;
	}

	return requireAll;
}

void AnimTransition::ConsumeMatchedTriggers(AnimatorParameters& params) const
{
	std::vector<const std::string*> matched;
	const auto& values = params.GetAll();

	const auto collect = [&](const std::vector<TransitionCondition>& list, ETransitionConditionLogic logic)
		{
			if (!MatchAnimatorConditionList(list, logic, params)) return;

			for (const auto& condition : list)
			{
				const auto it = values.find(condition.parameterName);
				if (it == values.end() || it->second.type != EAnimatorParamType::Trigger ||
					!it->second.boolValue || !condition.Evaluate(params))
				{
					continue;
				}

				matched.push_back(&condition.parameterName);
			}
		};

	collect(conditions, conditionLogic);
	for (const auto& group : conditionGroups)
		collect(group.conditions, group.conditionLogic);

	for (const auto* name : matched)
		params.ResetTrigger(*name);
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

	for (const auto& transition : transitions)
	{
		if (transition.fromAnyState && transition.targetState != stateName)
			result.push_back(&transition);
	}

	for (const auto& transition : transitions)
	{
		if (!transition.fromAnyState && transition.sourceState == stateName)
			result.push_back(&transition);
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
	m_currentBlendInput.reset();
	m_targetBlendInput.reset();
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
	m_currentBlendInput.reset();
	m_targetBlendInput.reset();
	if (m_stateMachine) m_currentStateName = m_stateMachine->defaultState;
}

namespace
{
	struct AnimatorMotion
	{
		const Animation* Primary = nullptr;
		const Animation* Secondary = nullptr;
		f32 SecondaryWeight = 0.0f;
		f64 Duration = 0.0;
		std::array<BlendClipWeight2D, 4> Corners{};
	};

	f64 AnimatorClipDuration(const Animation& clip)
	{
		const f64 tps = clip.GetTicksPerSecond() > 0.0 ? clip.GetTicksPerSecond() : 25.0;
		return clip.GetDuration() / tps;
	}

	AnimatorMotion ResolveAnimatorMotion(const AnimState& state, const AnimatorParameters& parameters, std::optional<vec2>& blendInput, f32 dt)
	{
		AnimatorMotion motion;

		if (state.IsBlendTree2D())
		{
			const auto& tree = state.blendTree2D;
			const auto& values = parameters.GetAll();
			const auto x = values.find(tree.parameterX);
			const auto y = values.find(tree.parameterY);
			if (x == values.end() || y == values.end() ||
				x->second.type != EAnimatorParamType::Float || y->second.type != EAnimatorParamType::Float)
			{
				return {};
			}

			const f32 rawX = x->second.floatValue;
			const f32 rawY = y->second.floatValue;

			if (!std::isfinite(rawX) || !std::isfinite(rawY) ||
				!std::isfinite(tree.minX) || !std::isfinite(tree.maxX) || tree.minX >= tree.maxX ||
				!std::isfinite(tree.minY) || !std::isfinite(tree.maxY) || tree.minY >= tree.maxY)
			{
				return {};
			}

			const vec2 desired(
				std::clamp(rawX, tree.minX, tree.maxX),
				std::clamp(rawY, tree.minY, tree.maxY));

			if (blendInput)
			{
				constexpr f32 halfLife = 0.08f;
				const f32 alpha = std::isfinite(dt) && dt > 0.0f ? 1.0f - std::exp2(-dt / halfLife) : 0.0f;
				const vec2 previous(
					std::clamp(blendInput->x, tree.minX, tree.maxX),
					std::clamp(blendInput->y, tree.minY, tree.maxY));

				blendInput = glm::mix(previous, desired, alpha);
			}
			else
			{
				blendInput = desired;
			}

			if (!tree.Evaluate(blendInput->x, blendInput->y, motion.Corners))
				return {};

			for (const auto& sample : motion.Corners)
			{
				if (!sample.Clip) continue;
				motion.Primary = sample.Clip;
				break;
			}

			const std::array<const ResourceHandle<Animation>*, 4> corners{
				&tree.clip00, &tree.clip10, &tree.clip01, &tree.clip11
			};
			for (const auto* corner : corners)
			{
				const auto* clip = corner->Get();
				if (!clip) continue;

				const f64 duration = AnimatorClipDuration(*clip);
				if (duration > 0.0)
				{
					motion.Duration = duration;
					break;
				}
			}
			return motion;
		}

		blendInput.reset();

		if (state.IsBlendTree())
		{
			const f32 value = parameters.GetFloat(state.blendTree.parameterName);
			state.blendTree.Evaluate(value, motion.Primary, motion.Secondary, motion.SecondaryWeight);
		}
		else
		{
			motion.Primary = state.clip.Get();
		}

		if (motion.Primary)
			motion.Duration = AnimatorClipDuration(*motion.Primary);

		return motion;
	}

	void EvaluateAnimatorMotion(const AnimState& state, const AnimatorMotion& motion, f32 time,
		const Skeleton& skeleton, std::vector<BoneLocalPose>& pose, std::vector<BoneLocalPose>& scratch)
	{
		const f32 phase = motion.Duration > 0.0 ? time / static_cast<f32>(motion.Duration) : 0.0f;
		const auto timeFor = [&](const Animation& clip)
			{
				const f64 duration = AnimatorClipDuration(clip);
				return duration > 0.0 ? phase * static_cast<f32>(duration) : time;
			};

		if (state.IsBlendTree2D())
		{
			f32 totalWeight = 0.0f;
			for (const auto& sample : motion.Corners)
			{
				if (!sample.Clip) continue;

				auto& sampledPose = totalWeight > 0.0f ? scratch : pose;
				AnimationEvaluator::EvaluateLocalPose(*sample.Clip, skeleton, timeFor(*sample.Clip),
					sampledPose, state.isLooping, state.loopBlendDuration);

				if (totalWeight > 0.0f)
				{
					const f32 weight = sample.Weight / (totalWeight + sample.Weight);
					for (size_t i = 0; i < pose.size(); ++i)
						pose[i] = AnimationEvaluator::BlendBoneLocalPose(pose[i], scratch[i], weight);
				}
				totalWeight += sample.Weight;
			}
			return;
		}

		if (motion.Secondary && motion.SecondaryWeight > 0.0f)
		{
			AnimationEvaluator::EvaluateBlendedLocalPose(*motion.Primary, time,
				*motion.Secondary, timeFor(*motion.Secondary), motion.SecondaryWeight,
				skeleton, pose, state.isLooping, state.isLooping);
		}
		else
		{
			AnimationEvaluator::EvaluateLocalPose(*motion.Primary, skeleton, time, pose,
				state.isLooping, state.loopBlendDuration);
		}
	}
}

void AnimatorRuntimeState::UpdateLocalPose(float dt, const Skeleton& skeleton,
	std::vector<BoneLocalPose>& outLocalPose)
{
	outLocalPose.clear();
	if (!m_stateMachine) return;

	const auto* current = m_stateMachine->FindState(m_currentStateName);
	if (!current) return;

	const auto motion = ResolveAnimatorMotion(*current, m_parameters, m_currentBlendInput, dt);
	if (!motion.Primary) return;

	const f64 duration = motion.Duration;
	const f32 normalizedTime = duration > 0.0 ? m_currentStateTime / static_cast<f32>(duration) : 0.0f;

	const AnimState* target = nullptr;
	AnimatorMotion targetMotion;

	if (m_isTransitioning)
	{
		target = m_stateMachine->FindState(m_targetStateName);
		if (target)
			targetMotion = ResolveAnimatorMotion(*target, m_parameters, m_targetBlendInput, dt);
	}
	else
	{
		for (const auto* transition : m_stateMachine->GetTransitionsFrom(m_currentStateName))
		{
			if (!transition->CanTransition(normalizedTime, m_parameters)) continue;

			const auto* candidate = m_stateMachine->FindState(transition->targetState);
			if (!candidate) continue;

			std::optional<vec2> candidateInput;
			if (m_currentBlendInput && candidate->IsBlendTree2D() &&
				current->blendTree2D.parameterX == candidate->blendTree2D.parameterX &&
				current->blendTree2D.parameterY == candidate->blendTree2D.parameterY)
			{
				candidateInput = m_currentBlendInput;
			}

			const auto candidateMotion = ResolveAnimatorMotion(*candidate, m_parameters, candidateInput, 0.0f);
			if (!candidateMotion.Primary) continue;

			target = candidate;
			targetMotion = candidateMotion;
			m_isTransitioning = true;
			m_targetStateName = transition->targetState;
			m_targetStateTime = 0.0f;
			m_targetBlendInput = candidateInput;
			m_transitionElapsed = 0.0f;
			m_transitionDuration = std::max(0.001f, transition->duration);

			transition->ConsumeMatchedTriggers(m_parameters);
			break;
		}
	}

	const f32 prevTime = m_currentStateTime;
	m_currentStateTime += dt * current->playRate;

	if (current->isLooping && duration > 0.0)
	{
		const f32 limit = static_cast<f32>(duration);
		const f32 wrappedTime = std::fmod(m_currentStateTime, limit);
		const f32 notifyTime = m_currentStateTime >= limit ? wrappedTime : m_currentStateTime;
		CheckAndTriggerNotifies(*current, prevTime, notifyTime, limit, true);
		m_currentStateTime = wrappedTime;
	}
	else if (duration > 0.0)
	{
		const f32 limit = static_cast<f32>(duration);
		m_currentStateTime = std::min(m_currentStateTime, limit);
		CheckAndTriggerNotifies(*current, prevTime, m_currentStateTime, limit, false);
	}
	else
	{
		CheckAndTriggerNotifies(*current, prevTime, m_currentStateTime, 0.0f, false);
	}
	m_previousStateTime = m_currentStateTime;

	if (m_isTransitioning)
	{
		if (targetMotion.Primary)
		{
			m_targetStateTime += dt * target->playRate;
			if (!target->isLooping && targetMotion.Duration > 0.0)
				m_targetStateTime = std::min(m_targetStateTime, static_cast<f32>(targetMotion.Duration));

			m_transitionElapsed += dt;
			const f32 alpha = std::clamp(m_transitionElapsed / m_transitionDuration, 0.0f, 1.0f);

			if (alpha >= 1.0f)
			{
				EvaluateAnimatorMotion(*target, targetMotion, m_targetStateTime,
					skeleton, outLocalPose, m_blendSamplePose);
				m_currentStateName = m_targetStateName;
				m_currentStateTime = m_targetStateTime;
				m_currentBlendInput = m_targetBlendInput;
				m_targetBlendInput.reset();
				m_isTransitioning = false;
				return;
			}

			EvaluateAnimatorMotion(*current, motion, m_currentStateTime,
				skeleton, outLocalPose, m_blendSamplePose);

			if (alpha > 0.0f)
			{
				EvaluateAnimatorMotion(*target, targetMotion, m_targetStateTime,
					skeleton, m_transitionTargetPose, m_blendSamplePose);

				for (size_t i = 0; i < outLocalPose.size(); ++i)
				{
					outLocalPose[i] = AnimationEvaluator::BlendBoneLocalPose(
						outLocalPose[i], m_transitionTargetPose[i], alpha);
				}
			}
			return;
		}

		m_isTransitioning = false;
	}

	EvaluateAnimatorMotion(*current, motion, m_currentStateTime, skeleton, outLocalPose, m_blendSamplePose);
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

	const auto* state = m_stateMachine->FindState(m_currentStateName);
	auto blendInput = m_currentBlendInput;
	return state ? ResolveAnimatorMotion(*state, m_parameters, blendInput, 0.0f).Primary : nullptr;
}