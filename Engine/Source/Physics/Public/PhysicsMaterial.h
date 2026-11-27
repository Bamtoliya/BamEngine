#pragma once

#include "Engine_API.h"
#include "Resource.h"
#include <algorithm>
#include <cmath>
#include <filesystem>

BEGIN(Engine)

ENUM()
enum class EPhysicsCombineMode : uint8
{
	Average = 0,
	Minimum,
	Multiply,
	Maximum
};

struct PhysicsMaterialCreateDesc : public ResourceCreateDesc
{
};

// 물리 재질 독립 에셋(.bamphysmat.json). 여러 콜라이더가 공유할 수 있습니다.
CLASS()
class ENGINE_API PhysicsMaterial : public Resource
{
	REFLECT_CLASS()
	DECLARE_RESOURCE(PhysicsMaterial)

private:
	using DESC = PhysicsMaterialCreateDesc;
	PhysicsMaterial() : Resource(EResourceType::Unknown) {}
	virtual ~PhysicsMaterial() = default;

public:
	static constexpr const wchar_t* FileSuffix = L".bamphysmat.json";

	virtual EResult Initialize(void* arg = nullptr) override;
	virtual void Free() override {}
	static PhysicsMaterial* Create(void* arg = nullptr);

	// 가상 URI·프로젝트 상대·절대 경로를 받는다.
	bool SaveToFile(const std::filesystem::path& path) const;
	bool LoadFromFile(const std::filesystem::path& path);
	static bool IsPhysicsMaterialFile(const std::filesystem::path& path);

	static f32 CombineValues(f32 a, f32 b, EPhysicsCombineMode mode) noexcept
	{
		switch (mode)
		{
		case EPhysicsCombineMode::Average:  return (a + b) * 0.5f;
		case EPhysicsCombineMode::Minimum:  return std::min(a, b);
		case EPhysicsCombineMode::Multiply: return a * b;
		case EPhysicsCombineMode::Maximum:  return std::max(a, b);
		default:                            return (a + b) * 0.5f;
		}
	}

	bool HasValidParameters() const noexcept
	{
		return std::isfinite(StaticFriction) && StaticFriction >= 0.f &&
			std::isfinite(DynamicFriction) && DynamicFriction >= 0.f &&
			std::isfinite(Restitution) && Restitution >= 0.f && Restitution <= 1.f;
	}

public:
	PROPERTY(EDITABLE, ALLOW_MISSING, RANGE(0.0, 10.0, 0.01), DISPLAY_NAME("PROP_STATIC_FRICTION"))
	f32 StaticFriction = 0.5f;

	PROPERTY(EDITABLE, ALLOW_MISSING, RANGE(0.0, 10.0, 0.01), DISPLAY_NAME("PROP_DYNAMIC_FRICTION"))
	f32 DynamicFriction = 0.5f;

	PROPERTY(EDITABLE, ALLOW_MISSING, RANGE(0.0, 1.0, 0.01), DISPLAY_NAME("PROP_RESTITUTION"))
	f32 Restitution = 0.0f;

	PROPERTY(EDITABLE, ALLOW_MISSING, DISPLAY_NAME("PROP_FRICTION_COMBINE"))
	EPhysicsCombineMode FrictionCombine = EPhysicsCombineMode::Average;

	PROPERTY(EDITABLE, ALLOW_MISSING, DISPLAY_NAME("PROP_RESTITUTION_COMBINE"))
	EPhysicsCombineMode RestitutionCombine = EPhysicsCombineMode::Average;
};

END
