#pragma once

#include "Base.h"
#include "RenderPass.h"
#include "RHIBlendTypes.h"
#include <vector>

BEGIN(Engine)

struct ViewportCameraInfo;

namespace VFXPassHelper
{
	ENGINE_API std::vector<ViewportCameraInfo> CollectVFXViewportTargets();

	inline EBlendMode ResolveEffectiveAcceptedBlendModes(const RenderPass* renderPass)
	{
		if (!renderPass) return EBlendMode::None;
		EBlendMode accepted = renderPass->GetAcceptedBlendModes();
		if (accepted == EBlendMode::None && renderPass->GetPassType() == ERenderPassType::Geometry)
			accepted = EBlendMode::Opaque | EBlendMode::Masked;
		return accepted;
	}

	inline bool IsBlendModeAccepted(EBlendMode acceptedModes, EBlendMode materialMode)
	{
		if ((acceptedModes & materialMode) != EBlendMode::None) return true;
		if (materialMode == EBlendMode::PureAdditive && (acceptedModes & EBlendMode::Additive) != EBlendMode::None) return true;
		return false;
	}
}

END