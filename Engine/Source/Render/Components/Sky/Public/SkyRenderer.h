#pragma once

#include "RenderComponent.h"

BEGIN(Engine)
class GameObject;

ENUM()
enum class ESkyShape : uint8
{
	Sphere,
	Cube,
};

struct tagSkyRendererDesc : public tagComponentDesc
{
	ESkyShape SkyType;
	wstring SkyTexturePath;
};

struct tagSkyAtmosphereDesc
{
	vec3  ZenithColor;    f32 _pad0;
	vec3  HorizonColor;   f32 _pad1;
	vec3  GroundColor;    f32 _pad2;
	vec3  SunDirection;   f32 HorizonBlend;
	vec3  SunColor;       f32 SunIntensity;
	f32	  SkyIntensity;	  f32 SunSize;        f32 _pad3[2];
};

CLASS()
class ENGINE_API SkyRenderer : public RenderComponent
{
	REFLECT_CLASS()
	using DESC = tagSkyRendererDesc;
#pragma region Constructor&Destructor
private:
	SkyRenderer() = default;
	virtual ~SkyRenderer() = default;
	EResult Initialize(void* arg = nullptr) override;
public:
	static SkyRenderer* Create(void* arg = nullptr);
	Component* Clone(GameObject* owner, void* arg = nullptr) override;
	virtual void Free() override;
#pragma endregion

#pragma region Loop
public:
	virtual EResult Render(f32 dt, class RenderPass* renderPass = nullptr) override;
#pragma endregion

#pragma region Getter
public:
	ESkyShape	GetShape()        const { return m_Shape; }
	const vec3& GetZenithColor()  const { return m_ZenithColor; }
	const vec3& GetHorizonColor() const { return m_HorizonColor; }
	const vec3& GetGroundColor()  const { return m_GroundColor; }
	const vec3& GetSunDirection() const { return m_SunDirection; }
	const vec3& GetSunColor()     const { return m_SunColor; }
	f32         GetSunIntensity() const { return m_SunIntensity; }
	f32         GetSkyIntensity() const { return m_SkyIntensity; }
public:
	tagSkyAtmosphereDesc BuildSkyAtmosphereDesc() const;
#pragma endregion


#pragma region  Setter
public: 
	void SetSunDirection(const vec3& direction) { m_SunDirection = direction; }
#pragma endregion



#pragma region Geometry
private:
	EResult BuildSphereMesh();
	EResult BuildCubeMesh();
	EResult RebuildMesh();
#pragma endregion


#pragma region Member Variable
private:
	//Shape
	PROPERTY(EDITABLE, CATEGORY("Shape"))
	ESkyShape m_Shape = ESkyShape::Sphere;

	//Atmosphere
	PROPERTY(EDITABLE, COLOR, CATEGORY("Atmosphere"))
	vec3  m_ZenithColor = vec3(0.08f, 0.35f, 0.85f);

	PROPERTY(EDITABLE, COLOR, CATEGORY("Atmosphere"))
	vec3  m_HorizonColor = vec3(0.55f, 0.75f, 0.95f);

	PROPERTY(EDITABLE, COLOR, CATEGORY("Atmosphere"))
	vec3  m_GroundColor = vec3(0.18f, 0.13f, 0.08f);

	PROPERTY(EDITABLE, CATEGORY("Atmosphere"))
	f32   m_HorizonBlend = 0.3f;

	PROPERTY(EDITABLE, CATEGORY("Atmosphere"))
	f32   m_SkyIntensity = 1.0f;

	//Sun Disc
	PROPERTY(EDITABLE, CATEGORY("Sun"))
	vec3  m_SunDirection = vec3(0.5f, 1.0f, 0.5f);

	PROPERTY(EDITABLE, COLOR, CATEGORY("Sun"))
	vec3  m_SunColor = vec3(1.0f, 0.90f, 0.70f);

	PROPERTY(EDITABLE, CATEGORY("Sun"))
	f32   m_SunIntensity = 10.0f;

	PROPERTY(EDITABLE, CATEGORY("Sun"))
	f32   m_SunSize = 0.02f;

	//Sphere Resolution
	PROPERTY(EDITABLE, EDITCONDITION("m_Shape", ESkyShape::Sphere, true), CATEGORY("Shape"))
	int32 m_SphereStacks = 16;

	PROPERTY(EDITABLE, EDITCONDITION("m_Shape", ESkyShape::Sphere, true), CATEGORY("Shape"))
	int32 m_SphereSlices = 32;

	//Internal Mesh
	PROPERTY(EDITABLE)
	ResourceHandle<Mesh> m_Mesh;

	ESkyShape m_LastBuiltShape = ESkyShape::Sphere;
#pragma endregion
};
END