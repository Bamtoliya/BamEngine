#pragma once

#include "Collider.h"

BEGIN(Engine)

class ENGINE_API Collision
{
public:
	static bool Raycast(const Ray& ray, HitResult& outHit);

};
END