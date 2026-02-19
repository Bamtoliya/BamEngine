#pragma once

#include "MeshGenerator.h"

BEGIN(Engine)

class ENGINE_API SphereGenerator final : public MeshGenerator
{
#pragma region Constructor&Destructor
	private:
	SphereGenerator() {}
	virtual ~SphereGenerator() = default;
	virtual EResult Initialize(void* arg = nullptr) override;
public:
	static SphereGenerator* Create(void* arg = nullptr);
	virtual void Free() override;
#pragma endregion



#pragma region Variable
private:
	float m_Radius = 1.0f;
	int m_Segments = 16;

#pragma endregion


};

END