#pragma once

#include "Component.h"

BEGIN(Engine)

enum class ESpace { Local, World };
enum class EMobility { Static, Movable };

enum class ETransformFlag : uint16
{
	None = 0,
	Dirty = 1 << 0,
	InheritPosition = 1 << 1,
	InheritRotation = 1 << 2,
	InheritScale = 1 << 3,

	LockPosition = 1 << 4,
	LockRotation = 1 << 5,
	LockScale = 1 << 6,

	Static = 1 << 7,

	AllInherit = InheritPosition | InheritRotation | InheritScale,
	AllLocked = LockPosition | LockRotation | LockScale,
	Default = Dirty | AllInherit,
};

ENABLE_BITMASK_OPERATORS(ETransformFlag)

class ENGINE_API Transform final : public Component
{
#pragma region Struct
public:
	typedef struct tagTransformDesc
	{
		vec3	Position;
		vec3	Rotation;
		vec3	Scale;
	}TRANSFORMDESC;
#pragma endregion

#pragma region Constructor&Destructor
private:
	Transform() : Component{} {}
	virtual ~Transform() {}
	virtual EResult Initialize(void* arg = nullptr) override;
public:
	static Component* Create(void* arg = nullptr);
	virtual Component* Clone(GameObject* owner, void* arg = nullptr) override;
	virtual void Free() override;
#pragma endregion

#pragma region Loop
public:
	virtual void	Update(f32 dt) override;
	virtual EResult	Render(f32 dt) override;
#pragma endregion

#pragma region Getter
public:
	const vec3& GetLocalPosition()		const { return m_Position; }
	const quat& GetLocalRotationQuat()	const { return m_Rotation; }
	vec3		GetLocalRotationEuler() const { return glm::degrees(glm::eulerAngles(m_Rotation)); }
	const vec3& GetLocalScale()			const { return m_Scale; }
	EMobility   GetMobility()			const { return HasFlag(m_Flags, ETransformFlag::Static) ? EMobility::Static : EMobility::Movable; }
	const mat4& GetLocalMatrix();
public:
	vec3 GetWorldPosition();
	quat GetWorldRotationQuat();
	vec3 GetWorldRotationEuler();
	vec3 GetWorldScale();

	const mat4& GetWorldMatrix();
	vec3 GetRight();
	vec3 GetUp();
	vec3 GetForward();
#pragma endregion

#pragma region Internal Helper Functions
private:
	bool IsDirty()  const { return HasFlag(m_Flags, ETransformFlag::Dirty); }
	bool IsStatic() const { return HasFlag(m_Flags, ETransformFlag::Static); }

	bool IsPositionLocked() const { return IsStatic() || HasFlag(m_Flags, ETransformFlag::LockPosition); }
	bool IsRotationLocked() const { return IsStatic() || HasFlag(m_Flags, ETransformFlag::LockRotation); }
	bool IsScaleLocked()    const { return IsStatic() || HasFlag(m_Flags, ETransformFlag::LockScale); }

	mat4 CalculateEffectiveParentMatrix(Transform* parent);
#pragma endregion

#pragma region Setter
public:
	void SetPosition(const vec3& position);
	void SetRotation(const quat& rotation);
	void SetRotation(const vec3& eulerAngles);
	
	void SetScale(const vec3& scale);
	void SetMobility(EMobility mobility);
	void SetInheritPosition(bool inherit);
	void SetInheritRotation(bool inherit);
	void SetInheritScale(bool inherit);
	void SetLockPosition(bool lock);
	void SetLockRotation(bool lock);
	void SetLockScale(bool lock);
#pragma endregion

#pragma region Matrix
private:
	void UpdateLocalMatrix();
	void UpdateWorldMatrix();
#pragma endregion

#pragma region Control
public:
	void Translate(const vec3& translation, ESpace space = ESpace::Local);
	void Rotate(const vec3& eulerAngle, ESpace space = ESpace::Local);
	void Rotate(const vec3& axis, float angle, ESpace space = ESpace::Local);
	void Rotate(const quat& rotation, ESpace space = ESpace::Local);

	void LookAt(const vec3& target, const vec3& up = glm::vec3(0, 1, 0));
#pragma endregion

#pragma region Variable
private:
	vec3 m_Position = vec3(0.0f);
	quat m_Rotation = glm::identity<quat>();
	vec3 m_Scale = vec3(1.0f);
	mat4 m_LocalMatrix = glm::identity<mat4>();
	mat4 m_WorldMatrix = glm::identity<mat4>();
	ETransformFlag m_Flags = ETransformFlag::Default;
#pragma endregion

};
END