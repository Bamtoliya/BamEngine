#pragma once

#include "RenderComponent.h"

BEGIN(Engine)
class ENGINE_API SpriteRenderer : public RenderComponent
{
#pragma region Constructor&Destructor
private:
	SpriteRenderer() {}
	virtual ~SpriteRenderer() = default;
	virtual EResult Initialize(void* arg = nullptr);
public:
	static SpriteRenderer* Create(void* arg = nullptr);
	virtual Component* Clone(GameObject* owner, void* arg = nullptr) override;
	virtual void Free() override;
#pragma endregion
#pragma region Loop
public:
	virtual EResult Render(f32 dt) override;
#pragma endregion

#pragma region Getter
public:
	// Add getter methods here if needed
#pragma endregion
#pragma region Setter
public:
	// Add setter methods here if needed
#pragma endregion

#pragma region Variable
private:

#pragma endregion


};
END