#pragma once

#include "Engine_Includes.h"
#include "ResourceHandle.h"
#include "Animation.h"
#include "Skeleton.h"
#include "BlendTree1D.h"
#include "Reflection/ReflectionMacro.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>

BEGIN(Engine)

ENUM()
enum class EAnimatorParamType : uint8
{
	Float,
	Int,
	Bool,
	Trigger
};

STRUCT()
struct ENGINE_API AnimatorParameter
{
	REFLECT_BODY()
		PROPERTY(EDITABLE)
		EAnimatorParamType type = EAnimatorParamType::Float;
	PROPERTY(EDITABLE)
		float floatValue = 0.0f;
	PROPERTY(EDITABLE)
		int32 intValue = 0;
	PROPERTY(EDITABLE)
		bool boolValue = false;
};

class ENGINE_API AnimatorParameters
{
public:
	void SetFloat(const std::string& name, float value);
	void SetInt(const std::string& name, int32 value);
	void SetBool(const std::string& name, bool value);
	void SetTrigger(const std::string& name);
	void ResetTrigger(const std::string& name);
	float GetFloat(const std::string& name, float defaultValue = 0.0f) const;
	int32 GetInt(const std::string& name, int32 defaultValue = 0) const;
	bool GetBool(const std::string& name, bool defaultValue = false) const;
	bool GetTrigger(const std::string& name) const;
	const std::unordered_map<std::string, AnimatorParameter>& GetAll() const { return m_parameters; }
	std::unordered_map<std::string, AnimatorParameter>& GetAll() { return m_parameters; }
private:
	std::unordered_map<std::string, AnimatorParameter> m_parameters;
};

ENUM()
enum class EConditionMode : uint8
{
	Greater,
	Less,
	Equal,
	NotEqual,
	IfTrue,
	IfFalse
};

STRUCT()
struct ENGINE_API TransitionCondition
{
	REFLECT_BODY()

		PROPERTY(EDITABLE)
		std::string parameterName;

	PROPERTY(EDITABLE)
		EConditionMode mode = EConditionMode::Greater;

	PROPERTY(EDITABLE)
		float threshold = 0.0f;

	bool Evaluate(const AnimatorParameters& params) const;
};

STRUCT()
struct ENGINE_API AnimState
{
	REFLECT_BODY()

		PROPERTY(EDITABLE)
		std::string name;

	PROPERTY(EDITABLE)
		ResourceHandle<Animation> clip;

	PROPERTY(EDITABLE)
		std::shared_ptr<BlendTree1D> blendTree;

	PROPERTY(EDITABLE)
		float playRate = 1.0f;

	PROPERTY(EDITABLE)
		bool isLooping = true;

	bool IsBlendTree() const { return blendTree != nullptr; }
};

STRUCT()
struct ENGINE_API AnimTransition
{
	REFLECT_BODY()

		PROPERTY(EDITABLE)
		std::string sourceState;

	PROPERTY(EDITABLE)
		std::string targetState;

	PROPERTY(EDITABLE)
		float duration = 0.25f;

	PROPERTY(EDITABLE)
		bool hasExitTime = false;

	PROPERTY(EDITABLE)
		float exitTime = 0.9f;

	PROPERTY(EDITABLE)
		std::vector<TransitionCondition> conditions;

	bool CanTransition(float currentNormalizedTime, const AnimatorParameters& params) const;
};

CLASS()
class ENGINE_API AnimatorStateMachine
{
	REFLECT_BODY()

public:
	PROPERTY(EDITABLE)
		std::string defaultState;

	PROPERTY(EDITABLE)
		std::vector<AnimState> states;

	PROPERTY(EDITABLE)
		std::vector<AnimTransition> transitions;

	PROPERTY(EDITABLE)
		std::unordered_map<std::string, AnimatorParameter> defaultParameters;

	const AnimState* FindState(const std::string& name) const;
	std::vector<const AnimTransition*> GetTransitionsFrom(const std::string& stateName) const;

	bool SaveToFile(const std::string& filePath) const;
	bool LoadFromFile(const std::string& filePath);
};

class ENGINE_API AnimatorRuntimeState
{
public:
	void BindStateMachine(const std::shared_ptr<AnimatorStateMachine>& sm);
	bool HasStateMachine() const { return m_stateMachine != nullptr; }
	const std::shared_ptr<AnimatorStateMachine>& GetStateMachine() const { return m_stateMachine; }

	void Update(float dt, const Skeleton& skeleton, std::vector<mat4>& outBoneMatrices);

	void SetFloat(const std::string& name, float value) { m_parameters.SetFloat(name, value); }
	void SetInt(const std::string& name, int32 value) { m_parameters.SetInt(name, value); }
	void SetBool(const std::string& name, bool value) { m_parameters.SetBool(name, value); }
	void SetTrigger(const std::string& name) { m_parameters.SetTrigger(name); }

	float GetFloat(const std::string& name) const { return m_parameters.GetFloat(name); }
	int32 GetInt(const std::string& name) const { return m_parameters.GetInt(name); }
	bool GetBool(const std::string& name) const { return m_parameters.GetBool(name); }

	const std::string& GetCurrentStateName() const { return m_currentStateName; }
	bool IsTransitioning() const { return m_isTransitioning; }
	float GetTransitionProgress() const { return m_transitionDuration > 0.0f ? (m_transitionElapsed / m_transitionDuration) : 1.0f; }

	AnimatorParameters& GetParameters() { return m_parameters; }
	const AnimatorParameters& GetParameters() const { return m_parameters; }

private:
	std::shared_ptr<AnimatorStateMachine> m_stateMachine;
	AnimatorParameters m_parameters;

	std::string m_currentStateName;
	float m_currentStateTime = 0.0f;

	bool m_isTransitioning = false;
	std::string m_targetStateName;
	float m_targetStateTime = 0.0f;
	float m_transitionElapsed = 0.0f;
	float m_transitionDuration = 0.25f;
};

END