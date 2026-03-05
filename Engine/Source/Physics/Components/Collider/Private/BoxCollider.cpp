#pragma once

#include "BoxCollider.h"
#include "ComponentRegistry.h"
#include "GameObject.h"
#include "Transform.h"
#include "Collision.h"
#include "MeshFilter.h"

#ifdef _DEBUG
#include "Renderer.h"
#endif // 

REGISTER_COMPONENT(BoxCollider)

#pragma region Constructor&Destructor
EResult BoxCollider::Initialize(void* arg)
{
	if (arg)
	{
		CAST_DESC
		m_Center = desc->center;
		m_Extents = desc->extent;
	}
	return EResult::Success;
}

EResult BoxCollider::LateInitialize(void* arg)
{
	AutoFit();
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
	colliderDesc.extent = this->m_Extents;
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

#pragma region Loop
void BoxCollider::LateUpdate(f32 dt)
{
#ifdef _DEBUG
	if (m_DrawCollider)
	{
		mat4 worldMatrix = m_Owner ? m_Owner->GetTransform()->GetWorldMatrix() : mat4(1.0f);
		Renderer::Get().DrawDebugBox(m_Center, m_Extents, vec4(0.f, 1.f, 0.f, 1.f), worldMatrix);
	}
#endif // _DEBUG
}
#pragma endregion



#pragma region Collision
bool BoxCollider::Raycast(const Ray& ray, HitResult& outResult)
{
	mat4 worldMatrix = m_Owner ? m_Owner->GetTransform()->GetWorldMatrix() : mat4(1.0f);
	if(Collision::Raycast(ray, GetBox(), worldMatrix, outResult))
	{
		outResult.UserData = m_Owner;
		return true;
	}
	return false;
}
#pragma endregion

#pragma region Bounds
void BoxCollider::AutoFit()
{
	MeshFilter* meshFilter = m_Owner->GetComponent<MeshFilter>();
	if (!meshFilter)
		return;
	Mesh* mesh = meshFilter->GetMesh();
	if (!mesh)
		return;
	m_Center = mesh->GetCenter();
	m_Extents = mesh->GetExtents();
}
#pragma endregion

