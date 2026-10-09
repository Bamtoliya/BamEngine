#pragma once

#include "Engine_Includes.h"
#include "Reflection/ReflectionMacro.h"

#include "Components/NameComponent.h"
#include "Components/TagComponent.h"
#include "Components/IDComponent.h"
#include "Components/FlagComponent.h"
#include "Components/TransformComponent.h"
#include "Components/WorldTransformComponent.h"

BEGIN(Engine)

STRUCT()
struct HierarchyComponent
{
    REFLECT_STRUCT()

        entt::entity parent = entt::null;
    std::vector<entt::entity> children;
};

END