#pragma once

#include "BoxCollider.h"
#include "ComponentRegistry.h"

REGISTER_COMPONENT(BoxCollider)

#pragma region Constructor&Destructor
EResult BoxCollider::Initialize(void* arg)
{
	if (arg)
	{
		CAST_DESC
		m_Center = desc->center;
		m_Extent = desc->extent;
	}
	return EResult::Success;
}

BoxCollider* BoxCollider::Create(void* arg)
{
	BoxCollider* instance = new BoxCollider();
	if (IsFailure(instance->Initialize(arg)))
	{
		Safe_Release(instance);
		return nullptr;
	}
	return instance;
}

Component* BoxCollider::Clone(GameObject* owner, void* arg)
{
	BoxCollider* instance = new BoxCollider();

	DESC colliderDesc;
	colliderDesc.center = this->m_Center;
	colliderDesc.extent = this->m_Extent;
	colliderDesc.Owner = owner;
	colliderDesc.Active = this->m_Active;
	colliderDesc.Tag = this->m_Tag;

	if (IsFailure(instance->Initialize(&colliderDesc)))
	{
		Safe_Release(instance);
		return nullptr;
	}
	return instance;
}

void BoxCollider::Free()
{
	__super::Free();
}
#pragma endregion