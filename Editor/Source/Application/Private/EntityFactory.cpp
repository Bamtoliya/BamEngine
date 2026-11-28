#pragma once
#include "EntityFactory.h"
#include "Application.h"
#include "Entity.h"
#include "Scene.h"
#include "WorldComponents.h"
#include "PhysicsComponent.h"
#include "RenderComponents.h"
#include "ResourceHandle.h"
#include "ResourceManager.h"
#include "UILayout.h"
#include "UIRenderSystem.h"
#include "UITextInputComponent.h"
#include "UIDropdownComponent.h"
#include "PrefabModeManager.h"
#include "CameraComponent.h"
#include <stdexcept>

Entity& EntityFactory::CreateCanvasEntity(Scene* scene, const wstring& name)
{
	if (!scene) throw std::invalid_argument("Canvas creation requires a scene.");
	SceneEditScope edit(Application::Get().GetEditHistory(), *scene, "History.Command.CreateEntity");
	Entity& entity = scene->CreateEntity();
	auto& world = scene->GetRegistry();
	const auto handle = entity.GetEntityHandle();
	world.emplace<NameComponent>(handle, name);
	world.emplace<FlagComponent>(handle);
	world.emplace<HierarchyComponent>(handle);
	world.emplace<CanvasComponent>(handle);
	world.emplace<UIRenderOrderComponent>(handle);
	return entity;
}

namespace
{
	void ValidateUIParent(Scene* scene, const Entity& parent)
	{
		if (!scene || parent.GetScene() != scene ||
			UILayout::FindCanvas(scene->GetRegistry(), parent.GetEntityHandle()) == entt::null)
			throw std::invalid_argument("UI element requires a parent in a canvas.");
		if (!scene->GetRegistry().all_of<HierarchyComponent>(parent.GetEntityHandle()))
			throw std::invalid_argument("UI parent requires a hierarchy component.");
	}

	Entity& CreateUIElement(Scene& scene, Entity& parent, const wstring& name, glm::vec2 size)
	{
		auto& world = scene.GetRegistry();
		Entity& entity = scene.CreateEntity();
		const auto handle = entity.GetEntityHandle();
		world.emplace<NameComponent>(handle, name);
		world.emplace<FlagComponent>(handle);
		world.emplace<HierarchyComponent>(handle).parent = parent.GetEntityHandle();
		world.emplace<RectTransformComponent>(handle).sizeDelta = size;
		world.emplace<UIRenderOrderComponent>(handle);
		world.get<HierarchyComponent>(parent.GetEntityHandle()).children.push_back(handle);
		return entity;
	}
}

Entity& EntityFactory::CreateUIImageEntity(Scene* scene, Entity& parent, const wstring& name)
{
	ValidateUIParent(scene, parent);
	SceneEditScope edit(Application::Get().GetEditHistory(), *scene, "History.Command.CreateEntity");
	Entity& entity = CreateUIElement(*scene, parent, name, { 200.0f, 120.0f });
	auto& image = scene->GetRegistry().emplace<UIImageComponent>(entity.GetEntityHandle());
	image.materialHandle = Engine::UIRenderSystem::GetDefaultMaterial();
	return entity;
}

Entity& EntityFactory::CreateUIButtonEntity(Scene* scene, Entity& parent, const wstring& name)
{
	if (!scene) throw std::invalid_argument("UI Button creation requires a scene.");
	SceneEditScope edit(Application::Get().GetEditHistory(), *scene, "History.Command.CreateEntity");
	Entity& entity = CreateUIImageEntity(scene, parent, name);
	scene->GetRegistry().emplace<UIButtonComponent>(entity.GetEntityHandle());
	return entity;
}

Entity& EntityFactory::CreateUITextEntity(Scene* scene, Entity& parent, const wstring& name, const std::string& text)
{
	ValidateUIParent(scene, parent);
	SceneEditScope edit(Application::Get().GetEditHistory(), *scene, "History.Command.CreateEntity");
	Entity& entity = CreateUIElement(*scene, parent, name, { 200.0f, 48.0f });
	auto& uiText = scene->GetRegistry().emplace<UITextComponent>(entity.GetEntityHandle());
	uiText.text = text;
	uiText.useLocalization = false;
	return entity;
}

Entity& EntityFactory::CreateUITextInputEntity(Scene* scene, Entity& parent, const wstring& name, const std::string& placeholder)
{
	ValidateUIParent(scene, parent);
	SceneEditScope edit(Application::Get().GetEditHistory(), *scene, "History.Command.CreateEntity");
	Entity& entity = CreateUIElement(*scene, parent, name, { 200.0f, 40.0f });
	auto& world = scene->GetRegistry();
	const auto handle = entity.GetEntityHandle();
	auto& image = world.emplace<UIImageComponent>(handle);
	image.materialHandle = UIRenderSystem::GetDefaultMaterial();
	image.color = { 0.15f, 0.15f, 0.18f, 0.95f };
	image.raycastTarget = true;
	auto& textInput = world.emplace<UITextInputComponent>(handle);
	textInput.placeholder = placeholder;
	auto& text = world.emplace<UITextComponent>(handle);
	text.text = "";
	text.fontPixels = 18;
	text.useLocalization = false;
	return entity;
}

Entity& EntityFactory::CreateUIDropdownEntity(Scene* scene, Entity& parent, const wstring& name)
{
	ValidateUIParent(scene, parent);
	SceneEditScope edit(Application::Get().GetEditHistory(), *scene, "History.Command.CreateEntity");
	Entity& entity = CreateUIElement(*scene, parent, name, { 200.0f, 36.0f });
	auto& world = scene->GetRegistry();
	const auto handle = entity.GetEntityHandle();
	auto& image = world.emplace<UIImageComponent>(handle);
	image.materialHandle = UIRenderSystem::GetDefaultMaterial();
	image.color = { 0.20f, 0.22f, 0.26f, 1.0f };
	image.raycastTarget = true;
	auto& dropdown = world.emplace<UIDropdownComponent>(handle);
	dropdown.placeholder = "Select...";
	dropdown.options = { "Option 1", "Option 2", "Option 3" };
	auto& text = world.emplace<UITextComponent>(handle);
	text.text = "";
	text.fontPixels = 18;
	text.useLocalization = false;
	return entity;
}

Entity& EntityFactory::CreateUIPreset(Scene* scene, Entity& parent, UIElementPreset preset, const wstring& name)
{
	ValidateUIParent(scene, parent);
	SceneEditScope edit(Application::Get().GetEditHistory(), *scene, "History.Command.CreateEntity");
	glm::vec2 size{ 360.0f, 210.0f };
	if (preset == UIElementPreset::ProgressBar || preset == UIElementPreset::Slider) size = { 240.0f, 28.0f };
	if (preset == UIElementPreset::Toggle) size = glm::vec2(36.0f);
	if (preset == UIElementPreset::EllipseMask || preset == UIElementPreset::ImageMask) size = glm::vec2(200.0f);
	if (preset == UIElementPreset::ScrollView) size = { 320.0f, 200.0f };
	if (preset == UIElementPreset::TextInput) size = { 200.0f, 40.0f };
	if (preset == UIElementPreset::Dropdown) size = { 200.0f, 36.0f };

	auto& entity = CreateUIElement(*scene, parent, name, size);
	auto& world = scene->GetRegistry();
	const auto handle = entity.GetEntityHandle();
	if (preset == UIElementPreset::EllipseMask)
	{
		world.emplace<UIEllipseMaskComponent>(handle);
		return entity;
	}

	if (preset == UIElementPreset::ImageMask)
	{
		world.emplace<UIAlphaMaskComponent>(handle);
		return entity;
	}

	auto& image = world.emplace<UIImageComponent>(handle);
	image.materialHandle = UIRenderSystem::GetDefaultMaterial();
	image.color = { 0.12f, 0.16f, 0.21f, 0.95f };
	image.raycastTarget = false;

	switch (preset)
	{
	case UIElementPreset::Panel:
	case UIElementPreset::EllipseMask:
	case UIElementPreset::ImageMask:
		break;
	case UIElementPreset::Row:
	case UIElementPreset::Column:
	case UIElementPreset::Grid:
	{
		auto& group = world.emplace<UILayoutGroupComponent>(handle);
		group.padding = glm::vec4(12.0f);
		if (preset == UIElementPreset::Column) group.mode = EUILayoutGroupMode::Column;
		if (preset == UIElementPreset::Grid)
		{
			group.mode = EUILayoutGroupMode::Grid;
			group.cellSize = glm::vec2(72.0f);
		}
		break;
	}
	case UIElementPreset::ProgressBar:
		world.emplace<UIProgressBarComponent>(handle);
		break;
	case UIElementPreset::Toggle:
		image.raycastTarget = true;
		world.emplace<UIToggleComponent>(handle);
		break;
	case UIElementPreset::Slider:
		image.raycastTarget = true;
		world.emplace<UISliderComponent>(handle);
		break;
	case UIElementPreset::ClipPanel:
		world.emplace<UIClipComponent>(handle);
		break;
	case UIElementPreset::ScrollView:
		{
			world.emplace<UIScrollComponent>(handle);
			const auto contentHandle = CreateUIElement(*scene, entity, Engine::Unicode::Utf8ToWide(LOCAL("Viewport.ScrollContent")),
				{ 0.0f, 480.0f }).GetEntityHandle();
			auto& rect = world.get<RectTransformComponent>(contentHandle);
			rect.anchorMin = { 0.0f, 0.0f };
			rect.anchorMax = { 1.0f, 0.0f };
			rect.pivot = { 0.0f, 0.0f };
			auto& contentImage = world.emplace<UIImageComponent>(contentHandle);
			contentImage.materialHandle = UIRenderSystem::GetDefaultMaterial();
			contentImage.color = { 0.18f, 0.23f, 0.29f, 1.0f };
			contentImage.raycastTarget = false;
		}
		break;
	case UIElementPreset::TextInput:
		image.raycastTarget = true;
		image.color = { 0.15f, 0.15f, 0.18f, 0.95f };
		{
			auto& textInput = world.emplace<UITextInputComponent>(handle);
			textInput.placeholder = "Enter text...";
			auto& text = world.emplace<UITextComponent>(handle);
			text.text = "";
			text.fontPixels = 18;
			text.useLocalization = false;
		}
		break;
	case UIElementPreset::Dropdown:
		image.raycastTarget = true;
		image.color = { 0.20f, 0.22f, 0.26f, 1.0f };
		{
			auto& dropdown = world.emplace<UIDropdownComponent>(handle);
			dropdown.placeholder = "Select...";
			dropdown.options = { "Option 1", "Option 2", "Option 3" };
			auto& text = world.emplace<UITextComponent>(handle);
			text.text = "";
			text.fontPixels = 18;
			text.useLocalization = false;
		}
		break;
	}
	return entity;
}

Entity& EntityFactory::CreateEmptyEntity(Scene* scene, const wstring& name)
{
	SceneEditScope edit(Application::Get().GetEditHistory(), *scene, "History.Command.CreateEntity");
	Entity& entity = scene->CreateEntity();
	scene->AddComponent<HierarchyComponent>(entity);
	scene->AddComponent<NameComponent>(entity, name);
	scene->AddComponent<FlagComponent>(entity, EEntityFlag::Default);
	auto& transform = scene->AddComponent<TransformComponent>(entity);
	transform.position = vec3(0.0f, 0.0f, 0.0f);
	transform.rotation = quat(1.0f, 0.0f, 0.0f, 0.0f);
	transform.scale = vec3(1.0f, 1.0f, 1.0f);
	scene->AddComponent<WorldTransformComponent>(entity);

	auto& prefabMode = PrefabModeManager::Get();
	if (prefabMode.IsInPrefabMode() && prefabMode.GetPrefabScene() == scene)
	{
		auto* root = prefabMode.GetRootEntity();
		const auto result = root ? scene->ReparentEntity(entity, root) : EntityReparentResult::InvalidEntity;

		if (result != EntityReparentResult::Success && result != EntityReparentResult::Unchanged)
		{
			if (!scene->DestroyEntity(entity))
				prefabMode.MarkDirty();

			throw std::runtime_error("Could not attach the new entity to the prefab root.");
		}

		prefabMode.MarkDirty();
	}

	return entity;
}

Entity& EntityFactory::CreatePrimitiveEntity(Scene* scene, const wstring& name, const wstring& meshKey, const wstring& materialKey)
{
	Entity& entity = CreateEmptyEntity(scene, name);
	ResourceManager& resourceManager = ResourceManager::Get();
	ResourceHandle<Mesh> meshHandle = resourceManager.GetResourceHandle<Mesh>(meshKey);

	const wstring effectiveMaterialKey = materialKey.empty() ? L"Engine://Material/DefaultMaterial.bammat.json" : materialKey;

	// 1. 직접 키 조회
	ResourceHandle<MaterialInterface> materialHandle = resourceManager.GetResourceHandle<MaterialInterface>(effectiveMaterialKey);

	// 2. 가상 URI 변환 조회 Fallback (Resources/... -> Engine://... 등)
	if (!materialHandle && !effectiveMaterialKey.empty())
	{
		const wstring virtualUri = PlatformPath::ToVirtualUri(effectiveMaterialKey);
		if (!virtualUri.empty() && virtualUri != effectiveMaterialKey)
			materialHandle = resourceManager.GetResourceHandle<MaterialInterface>(virtualUri);
	}

	// 3. 디스크 로드 시도 Fallback
	if (!materialHandle && !effectiveMaterialKey.empty())
	{
		materialHandle = ResourceHandle<MaterialInterface>(resourceManager.LoadFile(effectiveMaterialKey));
	}

	if (!meshHandle || !materialHandle)
	{
		EDITOR_LOG_ERROR("Failed to create primitive entity: Missing resources. Mesh: {} ('{}'), Material: {} ('{}')",
			meshHandle ? "OK" : "Missing", Engine::Unicode::WideToUtf8(meshKey),
			materialHandle ? "OK" : "Missing", Engine::Unicode::WideToUtf8(effectiveMaterialKey));
		return entity;
	}

	scene->AddComponent<StaticMeshRendererComponent>(entity, meshHandle, materialHandle);
	return entity;
}

Entity& EntityFactory::CreateCubeEntity(Scene* scene, const wstring& name)
{
	return CreatePrimitiveEntity(scene, name, L"CubeMesh", L"Engine://Material/DefaultMaterial.bammat.json");
}

Entity& EntityFactory::CreateSphereEntity(Scene* scene, const wstring& name)
{
	return CreatePrimitiveEntity(scene, name, L"SphereMesh", L"Engine://Material/DefaultMaterial.bammat.json");
}

Entity& EntityFactory::CreatePlaneEntity(Scene* scene, const wstring& name)
{
	return CreatePrimitiveEntity(scene, name, L"PlaneMesh", L"Engine://Material/DefaultMaterial.bammat.json");
}

Entity& EntityFactory::CreateCylinderEntity(Scene* scene, const wstring& name)
{
	return CreatePrimitiveEntity(scene, name, L"CylinderMesh", L"Engine://Material/DefaultMaterial.bammat.json");
}

Entity& EntityFactory::CreateCapsuleEntity(Scene* scene, const wstring& name)
{
	return CreatePrimitiveEntity(scene, name, L"CapsuleMesh", L"Engine://Material/DefaultMaterial.bammat.json");
}

Entity& EntityFactory::CreateSpriteEntity(Scene* scene, const wstring& name, const wstring& materialKey)
{
	Entity& entity = CreateEmptyEntity(scene, name);

	ResourceManager& resourceManager = ResourceManager::Get();

	ResourceHandle<Sprite> spriteHandle = resourceManager.GetResourceHandle<Sprite>(RESOURCE_PATH L"Texture/uv1.bamsprite.json");
	ResourceHandle<Mesh> meshHandle = resourceManager.GetResourceHandle<Mesh>(L"QuadMesh");
	ResourceHandle<Material> materialHandle = resourceManager.GetResourceHandle<Material>(materialKey);

	if (!spriteHandle || !meshHandle || !materialHandle)
	{
		EDITOR_LOG_ERROR("Failed to create sprite entity: Missing resources. Sprite: {}, Mesh: {}, Material: {}",
			spriteHandle ? "OK" : "Missing", meshHandle ? "OK" : "Missing", materialHandle ? "OK" : "Missing");
	}

	scene->AddComponent<SpriteRendererComponent>(entity, spriteHandle, meshHandle, materialHandle);
	return entity;
}

Entity& EntityFactory::CreateLightEntity(Scene* scene, const wstring& name, const wstring& lightType)
{
	Entity& entity = CreateEmptyEntity(scene, name);
	scene->AddComponent<LightComponent>(entity);
	if (lightType == L"Directional")
	{
		auto& light = scene->GetComponent<LightComponent>(entity);
		light.LightType = ELightType::Directional;
		light.Color = vec3(1.0f, 0.95f, 0.85f); // 따뜻한 햇빛
		light.Intensity = 1.2f;
		light.Flags = ELightFlags::UseInDeferredRendering | ELightFlags::AffectDiffuse | ELightFlags::AffectSpecular | ELightFlags::CastShadows;
		light.ShadowRange = 50.0f;
		light.ShadowBias = 0.002f;
	}
	else if (lightType == L"Point")
	{
		scene->GetComponent<LightComponent>(entity).LightType = ELightType::Point;
	}
	else if (lightType == L"Spot")
	{
		scene->GetComponent<LightComponent>(entity).LightType = ELightType::Spot;
	}
	else if (lightType == L"Sky")
	{
		auto& light = scene->GetComponent<LightComponent>(entity);
		light.LightType = ELightType::Sky;
		light.Color = vec3(0.35f, 0.55f, 0.85f);       // 하늘 천정(Zenith) 색상
		light.GroundColor = vec3(0.1f, 0.08f, 0.05f); // 지표면 반사 앰비언트
		light.IndirectIntensity = 0.8f;
		light.Intensity = 1.0f;
		light.Flags = ELightFlags::UseInDeferredRendering | ELightFlags::AffectDiffuse;
	}
	else
	{
		EDITOR_LOG_WARN("Unknown light type: {}. Defaulting to Point light.", Engine::Unicode::WideToUtf8(lightType));
		scene->GetComponent<LightComponent>(entity).LightType = ELightType::Point;
	}
	return entity;
}

Entity& EntityFactory::CreateDirectionalLightEntity(Scene* scene, const wstring& name)
{
	return CreateLightEntity(scene, name, L"Directional");
}

Entity& EntityFactory::CreatePointLightEntity(Scene* scene, const wstring& name)
{
	return CreateLightEntity(scene, name, L"Point");
}

Entity& EntityFactory::CreateSpotLightEntity(Scene* scene, const wstring& name)
{
	return CreateLightEntity(scene, name, L"Spot");
}

Entity& EntityFactory::CreateSkyLightEntity(Scene* scene, const wstring& name)
{
	return CreateLightEntity(scene, name, L"Sky");
}

Entity& EntityFactory::CreateCameraEntity(Scene* scene, const wstring& name, bool isMainCamera)
{
	Entity& entity = CreateEmptyEntity(scene, name);
	auto& camera = scene->AddComponent<CameraComponent>(entity);
	camera.CameraType = ECameraType::Perspective;
	camera.FOV = 60.0f;
	camera.NearPlane = 0.1f;
	camera.FarPlane = 1000.0f;

	if (isMainCamera)
	{
		camera.IsMainCamera = true;
	}
	else
	{
		bool hasMainCamera = false;
		for (auto camEntity : scene->GetRegistry().view<CameraComponent>())
		{
			if (camEntity != entity.GetEntityHandle() && scene->GetRegistry().get<CameraComponent>(camEntity).IsMainCamera)
			{
				hasMainCamera = true;
				break;
			}
		}
		camera.IsMainCamera = !hasMainCamera;
	}

	return entity;
}

Entity& EntityFactory::CreateTerrainEntity(Scene* scene, const wstring& name)
{
	Entity& entity = CreateEmptyEntity(scene, name);
	auto& terrain = scene->AddComponent<TerrainComponent>(entity);
	// 키 조회(GetResourceHandle)는 미리 불러온 리소스만 찾으므로 파일에서 불러온다. 기본 머티리얼은 추적되는 엔진 리소스다.
	terrain.TerrainMaterial = ResourceHandle<MaterialInterface>(ResourceManager::Get().LoadFile(TerrainComponent::DefaultMaterialUri));
	return entity;
}
