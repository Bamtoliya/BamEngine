#pragma once

#include "Camera.h"
#include "Transform.h"
#include "GameObject.h"
#include "RenderPass.h"

BEGIN(Editor)
class EditorCamera : public GameObject
{
private:
	EditorCamera() {}
	virtual ~EditorCamera() = default;
	EResult Initialize(void* arg = nullptr) override;
public:
	static EditorCamera* Create(void* arg = nullptr);
	virtual void Free() override;
public:
	virtual void FixedUpdate(f32 dt) override;
	virtual void Update(f32 dt) override;
	virtual void LateUpdate(f32 dt) override;

public:
	Camera* GetCamera() const { return m_Camera; }
	Transform* GetTransform() const { return m_Transform; }

private:
	Transform* m_Transform = { nullptr };
	Camera* m_Camera = { nullptr };
};
END