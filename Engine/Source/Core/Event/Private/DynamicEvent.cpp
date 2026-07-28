#pragma once
#include "DynamicEvent.h"
#include "SceneManager.h"
#include "GameObject.h"
#include "Component.h"

void DynamicEvent::Invoke() const
{
	for (const auto& binding : Bindings)
	{
		//GameObject* targetObject = GameObject::FindByID(binding.TargetGameObjectID);
		//if (!targetObject) continue;
		//Component* targetComponent = targetObject->GetComponentByTypeName(binding.ComponentTypeName);
		//if (!targetComponent) continue;
		//auto method = targetComponent->GetTypeInfo()->GetMethod(binding.MethodName);
		//if (!method) continue;
		//method->Invoke(targetComponent, {});
	}
}