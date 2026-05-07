#pragma once

#include "Engine_Includes.h"

class PostProcessInterface
{
public:
    virtual ~PostProcessInterface() = default;
    virtual void ApplyPostProcess() = 0;
};