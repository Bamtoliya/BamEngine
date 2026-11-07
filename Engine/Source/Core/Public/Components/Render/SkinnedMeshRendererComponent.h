#pragma once

#include "ComponentAnnotations.h"
#include "ResourceHandle.h"
#include "MaterialInterface.h"
#include "Mesh.h"
#include "Skeleton.h"
#include <optional>

namespace Engine
{
	struct SkinnedMaterialParameterOverride
	{
		std::optional<vec4> baseColor;
		std::optional<vec3> emissiveColor;
		std::optional<float> roughness;
		std::optional<float> metallic;
		std::optional<float> cavity;
		std::optional<float> ao;

		void ApplyTo(MaterialConstants& constants) const
		{
			if (baseColor) constants.baseColor = *baseColor;
			if (emissiveColor) constants.emissiveColor = *emissiveColor;
			if (roughness) constants.roughness = *roughness;
			if (metallic) constants.metallic = *metallic;
			if (cavity) constants.cavity = *cavity;
			if (ao) constants.ao = *ao;
		}

		[[nodiscard]] bool HasAny() const
		{
			return baseColor.has_value() || emissiveColor.has_value() ||
				roughness.has_value() || metallic.has_value() ||
				cavity.has_value() || ao.has_value();
		}
	};

	struct SkinnedTemporaryEffect
	{
		uint32 requestId = 0;
		float remainingTime = 0.0f;
		ResourceHandle<MaterialInterface> overrideMaterial;
		SkinnedMaterialParameterOverride parameterOverride;
		bool isActive = false;
	};

	struct SkinnedRuntimeEquipment
	{
		ResourceHandle<Mesh> mesh;
		ResourceHandle<MaterialInterface> material;
		bool isEquipped = false;
	};

	STRUCT(COMPONENT, DISPLAY_NAME("STRUCT_SKINNED_MESH_RENDERER"), CATEGORY("CATEGORY_RENDER"))
	struct SkinnedMeshRendererComponent
	{
		REFLECT_BODY()

		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_MESH_HANDLE"))
		ResourceHandle<Mesh> meshHandle;
		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_MATERIAL_HANDLE"))
		ResourceHandle<MaterialInterface> materialHandle;
		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_SKELETON_HANDLE"))
		ResourceHandle<Skeleton> skeletonHandle;
		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_CAST_SHADOW"))
		bool castShadow = true;

		// 런타임 외형 교체 (플레이 중 장착한 외형: 씬 저장 X)
		SkinnedRuntimeEquipment runtimeEquipment;

		// 일시 효과 (피격/상태이상 등: 씬 저장 X)
		SkinnedTemporaryEffect temporaryEffect;

		// 런타임 장비 장착 (스켈레톤 호환성 검증 후 장착)
		EResult EquipRuntimeMesh(const ResourceHandle<Mesh>& newMesh,
			const ResourceHandle<MaterialInterface>& newMaterial = {})
		{
			const auto* mesh = newMesh.Get();
			const auto* skeleton = skeletonHandle.Get();
			if (!mesh || !skeleton || !mesh->IsSkinCompatibleWith(*skeleton))
				return EResult::InvalidArgument;

			runtimeEquipment.mesh = newMesh;
			if (newMaterial.IsValid())
				runtimeEquipment.material = newMaterial;
			runtimeEquipment.isEquipped = true;
			return EResult::Success;
		}

		void UnequipRuntimeMesh()
		{
			runtimeEquipment.mesh.Reset();
			runtimeEquipment.material.Reset();
			runtimeEquipment.isEquipped = false;
		}

		// 일시 머티리얼 통째 교체
		void ApplyTemporaryMaterial(const ResourceHandle<MaterialInterface>& material,
			float duration, uint32 requestId = 0)
		{
			temporaryEffect.overrideMaterial = material;
			temporaryEffect.parameterOverride = {};
			temporaryEffect.remainingTime = duration;
			temporaryEffect.requestId = requestId;
			temporaryEffect.isActive = true;
		}

		// 일시 파라미터 오버라이드 (발광, 색상 등)
		void ApplyParameterOverride(const SkinnedMaterialParameterOverride& overrideParams,
			float duration, uint32 requestId = 0)
		{
			temporaryEffect.overrideMaterial.Reset();
			temporaryEffect.parameterOverride = overrideParams;
			temporaryEffect.remainingTime = duration;
			temporaryEffect.requestId = requestId;
			temporaryEffect.isActive = true;
		}

		// 일시 효과 즉시 제거 (requestId가 0이거나 일치할 때)
		void ClearTemporaryEffect(uint32 requestId = 0)
		{
			if (requestId == 0 || temporaryEffect.requestId == requestId)
			{
				temporaryEffect.overrideMaterial.Reset();
				temporaryEffect.parameterOverride = {};
				temporaryEffect.remainingTime = 0.0f;
				temporaryEffect.isActive = false;
			}
		}

		// 일시 효과 시간 갱신
		void UpdateTemporaryEffects(float dt)
		{
			if (!temporaryEffect.isActive) return;
			temporaryEffect.remainingTime -= dt;
			if (temporaryEffect.remainingTime <= 0.0f)
			{
				temporaryEffect.overrideMaterial.Reset();
				temporaryEffect.parameterOverride = {};
				temporaryEffect.remainingTime = 0.0f;
				temporaryEffect.isActive = false;
			}
		}

		// 최종 렌더 상태 결정 (1 모델 기본 -> 2 외형 교체 -> 3 일시 효과)
		void ResolveRenderState(Mesh*& outMesh, MaterialInterface*& outMaterial,
			MaterialConstants& outConstants) const
		{
			// 1단계: 씬 기본 외형
			outMesh = meshHandle.Get();
			outMaterial = materialHandle.Get();

			// 2단계: 런타임 장착 외형
			if (runtimeEquipment.isEquipped)
			{
				if (runtimeEquipment.mesh.IsValid())
					outMesh = runtimeEquipment.mesh.Get();
				if (runtimeEquipment.material.IsValid())
					outMaterial = runtimeEquipment.material.Get();
			}

			// 3단계: 일시 효과 (머티리얼 통째 교체)
			if (temporaryEffect.isActive && temporaryEffect.overrideMaterial.IsValid())
			{
				outMaterial = temporaryEffect.overrideMaterial.Get();
			}

			// 머티리얼 기본 상수 빌드
			if (outMaterial)
			{
				outMaterial->BuildMaterialConstants(outConstants);
			}
			else
			{
				outConstants = MaterialConstants{};
			}

			// 3단계: 일시 파라미터 오버라이드 합성 (피격 발광 등)
			if (temporaryEffect.isActive)
			{
				temporaryEffect.parameterOverride.ApplyTo(outConstants);
			}
		}
	};
}
