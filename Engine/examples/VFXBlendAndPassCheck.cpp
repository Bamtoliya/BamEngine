#include "RHIBlendTypes.h"
#include "RenderPass.h"
#include "VFXPassHelper.h"
#include <iostream>
#include <memory>
#include <string_view>

namespace
{
	bool Expect(bool condition, std::string_view description)
	{
		if (!condition)
		{
			std::cerr << "[FAIL] " << description << '\n';
			return false;
		}
		std::cout << "[PASS] " << description << '\n';
		return true;
	}

	struct ReleaseRenderPass
	{
		void operator()(Engine::RenderPass* pass) const { if (pass) pass->Release(); }
	};

	using RenderPassPtr = std::unique_ptr<Engine::RenderPass, ReleaseRenderPass>;

	RenderPassPtr MakeRenderPass(Engine::RenderPassDesc desc)
	{
		return RenderPassPtr(Engine::RenderPass::Create(&desc));
	}
}

int main()
{
	using namespace Engine;
	bool allPassed = true;

	std::cout << "=== [Test 1] BlendState Additive Preservation and PureAdditive (One/One) ===" << std::endl;
	{
		const BlendState additive(EBlendMode::Additive);
		allPassed &= Expect(additive.enable, "Additive blend must be enabled");
		allPassed &= Expect(additive.srcColor == EBlendFactor::SrcAlpha, "Additive srcColor must be SrcAlpha");
		allPassed &= Expect(additive.dstColor == EBlendFactor::One, "Additive dstColor must be One");
		allPassed &= Expect(additive.srcAlpha == EBlendFactor::One, "Additive srcAlpha must be One");
		allPassed &= Expect(additive.dstAlpha == EBlendFactor::One, "Additive dstAlpha must be One");
		allPassed &= Expect(additive.colorBlendOp == EBlendOp::Add, "Additive colorBlendOp must be Add");

		const BlendState pureAdditive(EBlendMode::PureAdditive);
		allPassed &= Expect(pureAdditive.enable, "PureAdditive blend must be enabled");
		allPassed &= Expect(pureAdditive.srcColor == EBlendFactor::One, "PureAdditive srcColor must be One");
		allPassed &= Expect(pureAdditive.dstColor == EBlendFactor::One, "PureAdditive dstColor must be One");
		allPassed &= Expect(pureAdditive.srcAlpha == EBlendFactor::One, "PureAdditive srcAlpha must be One");
		allPassed &= Expect(pureAdditive.dstAlpha == EBlendFactor::One, "PureAdditive dstAlpha must be One");
		allPassed &= Expect(pureAdditive.colorBlendOp == EBlendOp::Add, "PureAdditive colorBlendOp must be Add");
	}

	std::cout << "\n=== [Test 2] VFXPassHelper Effective Accepted Blend Mode Resolution ===" << std::endl;
	{
		allPassed &= Expect(VFXPassHelper::ResolveEffectiveAcceptedBlendModes(nullptr) == EBlendMode::None, "Null pass returns None");

		RenderPassDesc geomDesc;
		geomDesc.passType = ERenderPassType::Geometry;
		geomDesc.acceptedBlendModes = EBlendMode::None;
		const auto previewGeomPass = MakeRenderPass(geomDesc);
		allPassed &= Expect(previewGeomPass != nullptr, "Preview Geometry pass created");
		allPassed &= Expect(VFXPassHelper::ResolveEffectiveAcceptedBlendModes(previewGeomPass.get()) == (EBlendMode::Opaque | EBlendMode::Masked),
			"Preview Geometry pass with None resolves to Opaque | Masked");

		RenderPassDesc sceneGeomDesc;
		sceneGeomDesc.passType = ERenderPassType::Geometry;
		sceneGeomDesc.acceptedBlendModes = EBlendMode::Opaque | EBlendMode::Masked;
		const auto sceneGeomPass = MakeRenderPass(sceneGeomDesc);
		allPassed &= Expect(sceneGeomPass != nullptr, "Scene Geometry pass created");
		allPassed &= Expect(VFXPassHelper::ResolveEffectiveAcceptedBlendModes(sceneGeomPass.get()) == (EBlendMode::Opaque | EBlendMode::Masked),
			"Scene Geometry pass preserves Opaque | Masked");

		RenderPassDesc fwdTransDesc;
		fwdTransDesc.passType = ERenderPassType::ForwardTransparent;
		fwdTransDesc.acceptedBlendModes = EBlendMode::AlphaBlend | EBlendMode::Additive | EBlendMode::PureAdditive;
		const auto fwdTransPass = MakeRenderPass(fwdTransDesc);
		allPassed &= Expect(fwdTransPass != nullptr, "ForwardTransparent pass created");
		allPassed &= Expect(VFXPassHelper::ResolveEffectiveAcceptedBlendModes(fwdTransPass.get()) == (EBlendMode::AlphaBlend | EBlendMode::Additive | EBlendMode::PureAdditive),
			"ForwardTransparent pass preserves transparent blend modes");
	}

	std::cout << "\n=== [Test 3] VFXPassHelper and RenderPass Blend Mode Acceptance Filter ===" << std::endl;
	{
		const EBlendMode opaqueMasked = EBlendMode::Opaque | EBlendMode::Masked;
		const EBlendMode legacyTransparent = EBlendMode::AlphaBlend | EBlendMode::Additive | EBlendMode::NonPremultiplied | EBlendMode::Decal;
		const EBlendMode fullTransparent = legacyTransparent | EBlendMode::PureAdditive;

		allPassed &= Expect(VFXPassHelper::IsBlendModeAccepted(opaqueMasked, EBlendMode::Opaque), "Opaque accepted in Geometry");
		allPassed &= Expect(VFXPassHelper::IsBlendModeAccepted(opaqueMasked, EBlendMode::Masked), "Masked accepted in Geometry");
		allPassed &= Expect(!VFXPassHelper::IsBlendModeAccepted(opaqueMasked, EBlendMode::AlphaBlend), "AlphaBlend rejected in Geometry");
		allPassed &= Expect(!VFXPassHelper::IsBlendModeAccepted(opaqueMasked, EBlendMode::Additive), "Additive rejected in Geometry");
		allPassed &= Expect(!VFXPassHelper::IsBlendModeAccepted(opaqueMasked, EBlendMode::PureAdditive), "PureAdditive rejected in Geometry");

		allPassed &= Expect(VFXPassHelper::IsBlendModeAccepted(legacyTransparent, EBlendMode::Additive), "Additive accepted in legacy transparent pass");
		allPassed &= Expect(VFXPassHelper::IsBlendModeAccepted(legacyTransparent, EBlendMode::PureAdditive), "PureAdditive accepted in pass accepting Additive");
		allPassed &= Expect(VFXPassHelper::IsBlendModeAccepted(fullTransparent, EBlendMode::PureAdditive), "PureAdditive accepted in full transparent pass");

		RenderPassDesc passDesc;
		passDesc.acceptedBlendModes = legacyTransparent;
		const auto legacyPass = MakeRenderPass(passDesc);
		allPassed &= Expect(legacyPass != nullptr, "Legacy transparent pass created");
		allPassed &= Expect(legacyPass->IsAcceptsBlendMode(EBlendMode::Additive), "RenderPass accepts Additive");
		allPassed &= Expect(legacyPass->IsAcceptsBlendMode(EBlendMode::PureAdditive), "RenderPass with Additive accepts PureAdditive via fallback");
		allPassed &= Expect(!legacyPass->IsAcceptsBlendMode(EBlendMode::Opaque), "RenderPass transparent rejects Opaque");
	}

	std::cout << "\n=== Overall Result: " << (allPassed ? "ALL TESTS PASSED" : "SOME TESTS FAILED") << " ===" << std::endl;
	return allPassed ? 0 : 1;
}