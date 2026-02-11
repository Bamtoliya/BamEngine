#pragma once

#include "Transform.h"
#include "GameObject.h"
#include "ComponentRegistry.h"

REGISTER_COMPONENT(Transform)

#pragma region Constructor&Destructor
EResult Transform::Initialize(void* arg)
{
	if (IsFailure(__super::Initialize(arg)))
		return EResult::Fail;

	if (arg)
	{
		CAST_DESC
		m_Position = desc ? desc->Position : vec3(0.0f);
		m_Rotation = desc ? quat(glm::radians(desc->Rotation)) : quat(1.0f, 0.0f, 0.0f, 0.0f);
		m_Scale = desc ? desc->Scale : vec3(1.0f);
	}

	SetDirty();
	UpdateLocalMatrix();
	UpdateWorldMatrix();

	return EResult::Success;
}

Component* Transform::Create(void* arg)
{
	Transform* instance = new Transform();
	if (IsFailure(instance->Initialize(arg)))
	{
		Safe_Release(instance);
		return nullptr;
	}
	return instance;
}

Component* Transform::Clone(GameObject* owner, void* arg)
{
	Transform* instance = new Transform();
	DESC transformDesc;
	transformDesc.Position = this->m_Position;
	transformDesc.Rotation = glm::degrees(glm::eulerAngles(this->m_Rotation));
	transformDesc.Scale = this->m_Scale;
	transformDesc.Owner = owner;
	transformDesc.Active = this->m_Active;
	transformDesc.Tag = this->m_Tag;

	if (IsFailure(instance->Initialize(&transformDesc)))
	{
		Safe_Release(instance);
		return nullptr;
	}
	return instance;
}

void Transform::Free()
{
	m_Owner = nullptr;
	__super::Free();
}
#pragma endregion

#pragma region Loop
void Transform::Update(f32 dt)
{
	UpdateLocalMatrix();		
	UpdateWorldMatrix();
}

EResult Transform::Render(f32 dt)
{
	return EResult::Success;
}
#pragma endregion

#pragma region Getter
const mat4& Transform::GetLocalMatrix()
{
	UpdateLocalMatrix();
	return m_LocalMatrix;
}
const mat4& Transform::GetWorldMatrix()
{
	UpdateLocalMatrix();
	UpdateWorldMatrix();
	return m_WorldMatrix;
}
vec3 Transform::GetWorldPosition()
{
	const mat4& worldMatrix = GetWorldMatrix();
	return vec3(worldMatrix[3]);
}
quat Transform::GetWorldRotationQuat()
{
	const mat4& worldMatrix = GetWorldMatrix();

	vec3 scale;
	quat rotation;
	vec3 translation;
	vec3 skew;
	vec4 perspective;

	glm::decompose(worldMatrix, scale, rotation, translation, skew, perspective);

	return rotation;
}
vec3 Transform::GetWorldRotationEuler()
{
	const mat4& worldMatrix = GetWorldMatrix();

	vec3 scale;
	quat rotation;
	vec3 translation;
	vec3 skew;
	vec4 perspective;

	glm::decompose(worldMatrix, scale, rotation, translation, skew, perspective);

	return glm::degrees(glm::eulerAngles(rotation));
}
vec3 Transform::GetWorldScale()
{
	const mat4& worldMatrix = GetWorldMatrix();
	return vec3(
		glm::length(vec3(worldMatrix[0])),
		glm::length(vec3(worldMatrix[1])),
		glm::length(vec3(worldMatrix[2]))
	);
}


vec3 Transform::GetRight()
{
	UpdateWorldMatrix();
	return normalize(vec3(m_WorldMatrix[0]));
}

vec3 Transform::GetUp()
{
	UpdateWorldMatrix();
	return normalize(vec3(m_WorldMatrix[1]));
}

vec3 Transform::GetForward()
{
	UpdateWorldMatrix();
	return normalize(vec3(m_WorldMatrix[2]));
}
#pragma endregion

#pragma region Internal Helper Function
mat4 Transform::CalculateEffectiveParentMatrix(Transform* parent)
{
	mat4 effectiveParent = glm::identity<mat4>();

	if (HasFlag(m_Flags, ETransformFlag::InheritScale))
	{
		effectiveParent = glm::scale(effectiveParent, parent->GetWorldScale());
	}

	if (HasFlag(m_Flags, ETransformFlag::InheritRotation))
	{
		effectiveParent = effectiveParent * glm::toMat4(parent->GetWorldRotationQuat());
	}

	if (HasFlag(m_Flags, ETransformFlag::InheritPosition))
	{
		vec3 pPos = parent->GetWorldPosition();
		effectiveParent[3] = vec4(pPos, 1.0f);
	}

	return effectiveParent;
}
#pragma endregion


#pragma region Setter

void Transform::SetPosition(const vec3& position)
{
	if (!IsStatic())
	{
		SetDirty();
		m_Position = position;
	}
}

void Transform::SetRotation(const quat& rotation)
{
	if (!IsStatic())
	{
		m_Rotation = rotation;
		m_EulerRotation = glm::degrees(glm::eulerAngles(m_Rotation));
		SetDirty();

	}
}

void Transform::SetRotation(const vec3& eulerAngles)
{
	if (!IsStatic())
	{
		m_EulerRotation = eulerAngles;
		SetDirty();
		m_Rotation = quat(glm::radians(m_EulerRotation));
	}
}

void Transform::SetScale(const vec3& scale)
{
	if (!IsStatic())
	{
		SetDirty();
		m_Scale = scale;
	}
}

void Transform::SetMobility(EMobility mobility)
{
	if (mobility == EMobility::Static)
	{
		AddFlag(m_Flags, ETransformFlag::Static);
	}
	else
	{
		RemoveFlag(m_Flags, ETransformFlag::Static);
	}
}

void Transform::SetState(ETransformFlag state, bool value)
{
	if(value)
	{
		AddFlag(m_Flags, state);
	}
	else
	{
		RemoveFlag(m_Flags, state);
	}
}
#pragma endregion

#pragma region Matrix
void Transform::UpdateLocalMatrix()
{
	if (!IsDirty()) return;

	mat4 translation = glm::translate(glm::identity<mat4>(), m_Position);

	mat4 matRotation = glm::toMat4(m_Rotation);

	mat4 scale = glm::scale(glm::identity<mat4>(), m_Scale);

	m_LocalMatrix = translation * matRotation * scale;
}

void Transform::UpdateWorldMatrix()
{
	if (IsStatic() && !IsDirty()) return;

	GameObject* parent = (m_Owner) ? m_Owner->GetParent() : nullptr;
	if (!parent)
	{
		m_WorldMatrix = m_LocalMatrix;
		SetDirty(false);
		return;
	}
	Transform* parentTransform = parent->GetComponent<Transform>();

	const mat4& parentWorldMatrix = parentTransform->GetWorldMatrix();
	if (HasFlag(m_Flags, ETransformFlag::AllInherit))
	{
		m_WorldMatrix = parentWorldMatrix * m_LocalMatrix;
	}
	else
	{
		mat4 effectiveParent = CalculateEffectiveParentMatrix(parentTransform);
		m_WorldMatrix = effectiveParent * m_LocalMatrix;
	}
	SetDirty(false);
}
#pragma endregion

#pragma region Control
void Transform::Translate(const vec3& translation, ESpace space)
{
	if (IsStatic() || HasFlag(m_Flags, ETransformFlag::LockPosition)) return;

	if (space == ESpace::Local)
	{
		m_Position += m_Rotation * translation;
	}
	else
	{
		vec3 localTranslation = translation;
		GameObject* parent = (m_Owner) ? m_Owner->GetParent() : nullptr;
		if (parent)
		{
			Transform* parentTransform = parent->GetComponent<Transform>();
			if (parentTransform)
			{
				quat parentRotation = parentTransform->GetWorldRotationQuat();
				localTranslation = glm::inverse(parentRotation) * translation;
			}
		}
		m_Position += localTranslation;
	}
	SetDirty();
}
void Transform::Rotate(const vec3& eulerAngle, ESpace space)
{
	if (IsStatic() || HasFlag(m_Flags, ETransformFlag::LockRotation)) return;

	quat q = quat(glm::radians(eulerAngle));
	Rotate(q, space);
}
void Transform::Rotate(const vec3& axis, float angle, ESpace space)
{
	if (IsStatic() || HasFlag(m_Flags, ETransformFlag::LockRotation)) return;

	quat q = glm::angleAxis(glm::radians(angle), glm::normalize(axis));
	Rotate(q, space);
}
void Transform::Rotate(const quat& rotation, ESpace space)
{
	if (IsStatic() || HasFlag(m_Flags, ETransformFlag::LockRotation)) return;

	if (space == ESpace::Local)
	{
		m_Rotation = m_Rotation * rotation;
	}
	else
	{
		m_Rotation = rotation * m_Rotation;
	}
	SetDirty();
}
void Transform::LookAt(const vec3& target, const vec3& up)
{
	if (IsStatic() || HasFlag(m_Flags, ETransformFlag::LockRotation)) return;
	vec3 worldPos = GetWorldPosition();
	vec3 direction = target - worldPos;
	float length = glm::length(direction);

	if (length < 0.0001f) return;

	direction  = normalize(direction);

	quat targetRot = glm::quatLookAt(direction, up);

	GameObject* parent = (m_Owner) ? m_Owner->GetParent() : nullptr;
	if (parent)
	{
		Transform* parentTransform = parent->GetComponent<Transform>();
		if (parentTransform)
		{
			quat parentRot = parentTransform->GetWorldRotationQuat();
			m_Rotation = glm::inverse(parentRot) * targetRot;
		}
		else
		{
			m_Rotation = targetRot;
		}
	}
	else
	{
		m_Rotation = targetRot;
	}
	SetDirty();
}
#pragma endregion





