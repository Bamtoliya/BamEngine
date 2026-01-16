#pragma once

#include "Base.h"

BEGIN(Engine)
class ENGINE_API Mesh final : public Base
{
#pragma region Constructor&Destructor
private:
	Mesh() {}
	virtual ~Mesh() = default;
	EResult Initialize(void* arg = nullptr);
public:
	static Mesh* Create(void* arg = nullptr);
	Mesh* Clone(void* arg = nullptr);
	virtual void Free() override;

private:

	vector<Vertex> m_Vertices;

	void* m_VertexBuffer = {};
	void* m_IndexBuffer = {};
#pragma endregion

};
END