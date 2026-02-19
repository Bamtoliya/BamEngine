#pragma once

#include "GameObject.h"

#include "MeshGenerator.h"
#include "MeshRenderer.h"
#include "Mesh.h"

#pragma region Constructor&Destructor
EResult MeshGenerator::Initialize(void* arg)
{
	if (IsFailure(__super::Initialize(arg))) return EResult::Fail;
	m_MeshRenderer = m_Owner->GetComponent<MeshRenderer>();
	if (!m_MeshRenderer)
	{
		//m_MeshRenderer = m_Owner->AddComponent<MeshRenderer>();
	}
	Rebuild();
	return EResult::Success;
}
void MeshGenerator::Free()
{
	m_MeshRenderer = nullptr;
	m_GeneratedMesh = nullptr;
	Component::Free();
}

#pragma endregion

#pragma region Management
void MeshGenerator::SetMesh(Mesh* mesh)
{
	m_GeneratedMesh = mesh;
	if(m_MeshRenderer) m_MeshRenderer->SetMesh(mesh);
}
#pragma endregion
