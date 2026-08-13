#pragma once

#include "SceneFactory.h"
#include "Components.h"

void SceneFactory::CreateLayer(Scene* scene, const wstring& name)
{
	scene->CreateLayer(name);
}

void SceneFactory::CreateEmptyObject(Scene* scene)
{
	GameObject* newGameObject = GameObject::Create();
	newGameObject->SetName(L"New GameObject");
	scene->AddGameObject(newGameObject);
	Safe_Release(newGameObject);
}

void SceneFactory::CreatePrimitive(Scene* scene, const wstring& name, const wstring& meshName)
{
	ResourceManager& resourceMgr = ResourceManager::Get();
	GameObject* newGameObject = GameObject::Create();
	newGameObject->AddComponent<Transform>();
	newGameObject->SetName(L"New" + name);
	newGameObject->AddComponent<MeshRenderer>();
	newGameObject->AddComponent<MeshFilter>();
	MeshFilter* meshFilter = newGameObject->GetComponent<MeshFilter>();
	meshFilter->SetMeshHandle(resourceMgr.GetResourceHandle<Mesh>(meshName));
	MeshRenderer* meshRenderer = newGameObject->GetComponent<MeshRenderer>();
	meshRenderer->SetMaterial(resourceMgr.GetResourceHandle<Material>(L"Resources/Material/DefaultMaterial.bammat"));
	newGameObject->AddComponent<BoxCollider>();
	scene->AddGameObject(newGameObject);
	Safe_Release(newGameObject);
}

void SceneFactory::CreateSpriteObject(Scene* scene)
{
	ResourceManager& resourceMgr = ResourceManager::Get();
	GameObject* newGameObject = GameObject::Create();
	newGameObject->SetName(L"New Sprite");
	newGameObject->AddComponent<Transform>();
	newGameObject->AddComponent<SpriteRenderer>();
	SpriteRenderer* spriteRenderer = newGameObject->GetComponent<SpriteRenderer>();
	spriteRenderer->SetMaterial(resourceMgr.GetResourceHandle<Material>(L"Resources/Material/SpriteMaterial.bammat"));
	spriteRenderer->SetSprite(resourceMgr.GetResourceHandle<Sprite>(L"Resources/Texture/uv1.bamsprite"));

	newGameObject->AddComponent<Box2DCollider>();
	scene->AddGameObject(newGameObject);
	Safe_Release(newGameObject);
}

void SceneFactory::CreateAnimatorObject(Scene* scene)
{
	ResourceManager& resourceMgr = ResourceManager::Get();
	GameObject* newGameObject = GameObject::Create();
	newGameObject->AddComponent<Transform>();
	newGameObject->SetName(L"New Animation Object");
	// 1. 3D 모델 렌더링을 위한 필수 컴포넌트
	newGameObject->AddComponent<MeshFilter>();
	newGameObject->AddComponent<SkinnedMeshRenderer>();

	SkinnedMeshRenderer* skinnedMeshRenderer = newGameObject->GetComponent<SkinnedMeshRenderer>();
	skinnedMeshRenderer->SetMaterial(resourceMgr.GetResourceHandle<Material>(L"Resources/Material/SkinningMaterial.bammat"));

	// 2. 애니메이터 컴포넌트 부착 (스켈레톤과 애니메이션 제어용)
	newGameObject->AddComponent<Animator>();
	Animator* animator = newGameObject->GetComponent<Animator>();

	// [테스트용 임시 코드] 스켈레톤과 애니메이션 로드 및 설정
	// TODO: 실제 임포트하신 파일 경로로 변경해 주세요!
	ResourceHandle<Skeleton> skeleton = resourceMgr.GetResourceHandle<Skeleton>(L"Resources/Model/TestBall_Skeleton.bamskel");
	ResourceHandle<Animation> animation = resourceMgr.GetResourceHandle<Animation>(L"Resources/Model/TestBall_Armature_Anim_Bend.bamanim");
	ResourceHandle<Animation> animation2 = resourceMgr.GetResourceHandle<Animation>(L"Resources/Model/TestBall_Armature_Anim_Jump.bamanim");

	if (skeleton.IsValid() && animation.IsValid())
	{
		animator->SetSkeleton(skeleton);
		animator->AddState(L"TestAnim", animation, true, 1.0f);
		animator->AddState(L"TestAnim2", animation2, true, 1.0f);
		animator->Play(L"TestAnim");
	}

	// (선택) 물리 충돌체
	newGameObject->AddComponent<BoxCollider>();
	// 3. 씬에 등록
	scene->AddGameObject(newGameObject);
	Safe_Release(newGameObject);
}

void SceneFactory::CreateCamera(Scene* scene)
{
	GameObject* newGameObject = GameObject::Create();
	newGameObject->AddComponent<Transform>();
	newGameObject->SetName(L"Camera");
	newGameObject->AddComponent<Camera>();
	scene->AddGameObject(newGameObject);
	Safe_Release(newGameObject);
}

void SceneFactory::CreateDirectionalLight(Scene* scene)
{
	GameObject* newGameObject = GameObject::Create();
	newGameObject->AddComponent<Transform>();
	newGameObject->SetName(L"Directional Light");
	LightSourceDesc lightDesc;
	lightDesc.Type = ELightType::Directional;
	lightDesc.Color = vec3(1.0f, 1.0f, 1.f);
	newGameObject->AddComponent<LightSource>(&lightDesc);
	scene->AddGameObject(newGameObject);
	Safe_Release(newGameObject);
}

void SceneFactory::CreatePointLight(Scene* scene)
{
	GameObject* newGameObject = GameObject::Create();
	newGameObject->AddComponent<Transform>();
	newGameObject->SetName(L"Point Light");
	LightSourceDesc lightDesc;
	lightDesc.Type = ELightType::Point;
	lightDesc.Color = vec3(1.0f, 1.0f, 1.f);
	newGameObject->AddComponent<LightSource>(&lightDesc);
	scene->AddGameObject(newGameObject);
	Safe_Release(newGameObject);
}

void SceneFactory::CreateSpotLight(Scene* scene)
{
	GameObject* newGameObject = GameObject::Create();
	newGameObject->AddComponent<Transform>();
	newGameObject->SetName(L"Spot Light");
	LightSourceDesc lightDesc;
	lightDesc.Type = ELightType::Spot;
	lightDesc.Color = vec3(1.0f, 1.0f, 1.f);
	newGameObject->AddComponent<LightSource>(&lightDesc);
	scene->AddGameObject(newGameObject);
	Safe_Release(newGameObject);
}

void SceneFactory::CreateSky(Scene* scene)
{
	ResourceManager& resourceMgr = ResourceManager::Get();
	GameObject* newGameObject = GameObject::Create();
	newGameObject->AddComponent<Transform>();
	newGameObject->SetName(L"Sky");

	SkyRendererDesc skyRendererDesc;
	newGameObject->AddComponent<SkyRenderer>();
	newGameObject->AddComponent<SkyLight>();
	SkyRenderer* skyRenderer = newGameObject->GetComponent<SkyRenderer>();
	if (skyRenderer)
	{
		skyRenderer->SetMaterial(
			resourceMgr.GetResourceHandle<Material>(L"Resources/Material/SkyMaterial.bammat"));
	}

	scene->AddGameObject(newGameObject);
	Safe_Release(newGameObject);
}

void SceneFactory::CreateCanvas(Scene* scene)
{
	ResourceManager& resourceMgr = ResourceManager::Get();
	GameObject* newGameObject = GameObject::Create();
	newGameObject->SetName(L"New Canvas");
	newGameObject->AddComponent<RectTransform>();
	newGameObject->AddComponent<UICanvas>();
	newGameObject->AddComponent<GraphicRaycaster>();
	scene->AddGameObject(newGameObject);
	Safe_Release(newGameObject);
}

void SceneFactory::CreateImage(Scene* scene)
{
	ResourceManager& resourceMgr = ResourceManager::Get();
	GameObject* newGameObject = GameObject::Create();
	newGameObject->SetName(L"New Image");
	newGameObject->AddComponent<RectTransform>();
	newGameObject->AddComponent<UIImage>();
	UIImage* uiImage = newGameObject->GetComponent<UIImage>();
	uiImage->SetMaterial(resourceMgr.GetResourceHandle<MaterialInterface>(L"Resources/Material/UIMaterial"));
	uiImage->SetSprite(resourceMgr.GetResourceHandle<Sprite>(L"Resources/Texture/uv1.bamsprite.json"));
	scene->AddGameObject(newGameObject);
	Safe_Release(newGameObject);
}

void SceneFactory::CreateButton(Scene* scene)
{
	ResourceManager& resourceMgr = ResourceManager::Get();
	GameObject* newGameObject = GameObject::Create();
	newGameObject->SetName(L"New Button");
	newGameObject->AddComponent<RectTransform>();
	newGameObject->AddComponent<UIButton>();
	newGameObject->AddComponent<UIImage>();
	UIButton* uiButton = newGameObject->GetComponent<UIButton>();
	UIImage* uiImage = newGameObject->GetComponent<UIImage>();
	uiImage->SetMaterial(resourceMgr.GetResourceHandle<MaterialInterface>(L"Resources/Material/UIMaterial"));
	uiImage->SetSprite(resourceMgr.GetResourceHandle<Sprite>(L"Resources/Texture/uv1.bamsprite.json"));
	scene->AddGameObject(newGameObject);
	Safe_Release(newGameObject);
}

void SceneFactory::CreateEventSystem(Scene* scene)
{
	GameObject* newGameObject = GameObject::Create();
	newGameObject->SetName(L"Event System");
	newGameObject->AddComponent<EventSystem>();
	scene->AddGameObject(newGameObject);
	Safe_Release(newGameObject);
}
