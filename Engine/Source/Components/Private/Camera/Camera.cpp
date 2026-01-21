#include "Camera.h"


#pragma region Constructor&Destructor
EResult Camera::Initialize(void* arg)
{
	return EResult();
}

Component* Camera::Create(void* arg)
{
	return nullptr;
}

Component* Camera::Clone(GameObject* owner, void* arg)
{
	return nullptr;
}

void Camera::Free()
{
}
#pragma endregion

#pragma region Loop
void Camera::Update(f32 dt)
{
}

EResult Camera::Render(f32 dt)
{
	return EResult();
}
#pragma endregion