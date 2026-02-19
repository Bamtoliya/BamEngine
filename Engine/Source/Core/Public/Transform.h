#pragma once

#include "Component.h"
#include "ReflectionMacro.h"

BEGIN(Engine)


#pragma region Enum
enum class ESpace { Local, World };
enum class EMobility { Static, Movable };

ENUM()
enum class ETransformFlag : uint16
{
	None = 0,
	InheritPosition = 1 << 0,
	InheritRotation = 1 << 1,
	InheritScale = 1 << 2,

	LockPositionX = 1 << 3,
	LockPositionY = 1 << 4,
	LockPositionZ = 1 << 5,

	LockPosition = LockPositionX | LockPositionY | LockPositionZ,

	LockRotationX = 1 << 6,
	LockRotationY = 1 << 7,
	LockRotationZ = 1 << 8,

	LockRotation = LockRotationX | LockRotationY | LockRotationZ,

	LockScaleX = 1 << 9,
	LockScaleY = 1 << 10,
	LockScaleZ = 1 << 11,

	LockScale = LockScaleX | LockScaleY | LockScaleZ,

	Static = 1 << 12,
	AllInherit = InheritPosition | InheritRotation | InheritScale,
	AllLocked = LockPosition |  LockRotation | LockScale,
	Default = AllInherit,
};
#pragma endregion

#pragma region Struct

STRUCT()
struct tagTransformDesc : tagComponentDesc
{
	REFLECT_STRUCT(tagTransformDesc)

	PROPERTY()
	vec3	Position;

	PROPERTY()
	vec3	Rotation;

	PROPERTY()
	vec3	Scale;
};
#pragma endregion

ENABLE_BITMASK_OPERATORS(ETransformFlag)

CLASS()
class ENGINE_API Transform final : public Component
{
	REFLECT_CLASS(Transform);

	using DESC = tagTransformDesc;

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
	virtual EResult	Render(f32 dt, RenderPass* renderPass) override;
#pragma endregion

#pragma region Getter
public:
	const vec3& GetLocalPosition()		const { return m_Position; }
	const quat& GetLocalRotationQuat()	const { return m_Rotation; }
	vec3		GetLocalRotationEuler() const { return m_EulerRotation; }
	const vec3& GetLocalScale()			const { return m_Scale; }
	EMobility   GetMobility()			const { return HasFlag(m_Flags, ETransformFlag::Static) ? EMobility::Static : EMobility::Movable; }
	const mat4& GetLocalMatrix();
public:
	vec3 GetWorldPosition();
	quat GetWorldRotationQuat();
	vec3 GetWorldRotationEuler();
	vec3 GetWorldScale();
	const mat4& GetWorldMatrix();
public:
	vec3 GetRight();
	vec3 GetUp();
	vec3 GetForward();
public:
	bool GetState(ETransformFlag state) const { return HasFlag(m_Flags, state); }
#pragma endregion

#pragma region Internal Helper Functions
private:
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
	void SetState(ETransformFlag state, bool value = true);
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

	PROPERTY()
	vec3 m_Position = vec3(0.0f);

	PROPERTY()
	quat m_Rotation = glm::identity<quat>();

	vec3 m_EulerRotation = vec3(0.f);

	PROPERTY()
	vec3 m_Scale = vec3(1.0f);

	PROPERTY("PROP_LOCALMATRIX",READONLY)
	mat4 m_LocalMatrix = glm::identity<mat4>();

	PROPERTY("PROP_WORLDMATRIX", READONLY)
	mat4 m_WorldMatrix = glm::identity<mat4>();

	PROPERTY("PROP_BITFLAG")
	ETransformFlag m_Flags = ETransformFlag::Default;
#pragma endregion

};
END