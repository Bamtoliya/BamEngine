#pragma once

#include "BaseRaycaster.h"
#include "ComponentRegistry.h"
#include "EventSystem.h"

EResult BaseRaycaster::Initialize(void* arg)
{
	if (IsFailure(__super::Initialize(arg)))
		return EResult::Fail;

    if (EventSystem::GetCurrent())
    {
        EventSystem::GetCurrent()->AddRaycaster(this);
    }
    return EResult::Success;
}

void BaseRaycaster::Free()
{
    if (EventSystem::GetCurrent())
    {
        EventSystem::GetCurrent()->RemoveRaycaster(this);
    }

    __super::Free();
}
