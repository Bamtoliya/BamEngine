#include "DeferredProfilePasses.h"

#include "Functions.h"
#include "RenderPassManager.h"
#include "RenderTargetManager.h"
#include "Renderer.h"
#include <algorithm>
#include <set>

namespace Engine
{
    DeferredProfilePasses::~DeferredProfilePasses()
    {
        Free();
    }

    RenderPipelineProfileResult DeferredProfilePasses::Initialize(const RenderPipelineProfile& profile,
        const std::wstring& prefix, uint32 width, uint32 height)
    {
        if (!m_Passes.empty() || !m_Targets.empty())
            return { {}, {}, "The deferred profile instance is already initialized." };
        if (auto result = ValidateRenderPipelineProfile(profile); !result)
            return result;
        if (!Renderer::IsCreated() || !Renderer::Get().GetRHI() || !RenderTargetManager::IsCreated() ||
            !RenderPassManager::IsCreated() || width == 0 || height == 0)
            return { {}, {}, "An active RHI, render managers and a valid viewport size are required." };

        std::set<std::string_view> types;
        for (std::size_t index = 0; index < profile.Passes.size(); ++index)
        {
            const auto& pass = profile.Passes[index];
            const auto field = "Passes[" + std::to_string(index) + "]";
            if (!types.insert(pass.Type).second)
                return { {}, field + ".Type", "This deferred renderer supports one pass per type." };
            if (pass.Type == "DirectionalShadow" && pass.Resolution != 4096u)
                return { {}, field + ".Settings.Resolution", "The current directional shadow atlas requires 4096 texels." };
            if (pass.Type == "ScenePostProcess" && (index + 1 != profile.Passes.size() || profile.Output != pass.Id + ".Color"))
                return { {}, field, "ScenePostProcess must produce the final color in this renderer." };
        }
        if (!types.contains("DeferredLighting"))
            return { {}, "Passes", "DeferredLighting is required by this renderer." };

        auto& targets = RenderTargetManager::Get();
        auto& passes = RenderPassManager::Get();
        std::map<std::string, std::vector<std::wstring>, std::less<>> outputs;
        const auto createTarget = [&](const std::wstring& name, ETextureFormat format, uint32 resolution = 0,
            bool cube = false, vec4 clearColor = vec4(0.f))
        {
            if (targets.GetRenderTarget(name))
                return false;

            RenderTargetDesc desc{};
            desc.name = name;
            desc.fixedSize = resolution != 0;
            desc.textureDesc.width = resolution ? resolution : width;
            desc.textureDesc.height = resolution ? resolution : height;
            desc.textureDesc.format = format;
            desc.textureDesc.dimension = cube ? ETextureDimension::TextureCube : ETextureDimension::Texture2D;
            desc.textureDesc.arraySize = cube ? 6 : 1;
            const bool depth = format == ETextureFormat::D32_FLOAT || format == ETextureFormat::D24_UNORM_S8_UINT;
            desc.textureDesc.usage = ETextureUsage::Sampler |
                (depth ? ETextureUsage::DepthStencilTarget : ETextureUsage::RenderTarget);
            desc.textureDesc.clearColor = clearColor;

            auto* target = targets.CreateRenderTarget(&desc);
            if (!target)
                return false;
            m_Targets.push_back(name);
            return target->GetTexture() && target->GetTexture()->GetNativeHandle();
        };

        const auto fail = [this](std::size_t index, std::string message) -> RenderPipelineProfileResult
        {
            Free();
            return { {}, "Passes[" + std::to_string(index) + "]", std::move(message) };
        };

        for (std::size_t index = 0; index < profile.Passes.size(); ++index)
        {
            const auto& pass = profile.Passes[index];
            const auto name = prefix + L"_" + Utf8ToPath(pass.Id).wstring();
            const auto& type = pass.Type;
            const auto input = [&](const char* slot) -> const std::vector<std::wstring>&
            {
                return outputs.at(pass.Inputs.at(slot));
            };
            const auto output = [&](const char* slot, const std::vector<std::wstring>& names)
            {
                outputs.emplace(pass.Id + "." + slot, names);
            };
            if (type == "ScenePostProcess")
            {
                m_UsesScenePostProcess = true;
                output("Color", input("Color"));
                continue;
            }

            RenderPassDesc desc{};
            desc.name = name;
            desc.priority = 100u + static_cast<uint32>(index); // Deferred passes run before bloom and post-processing.
            desc.loadOperation = ERenderPassLoadOperation::RPLO_Clear;
            desc.stencilLoadOperation = ERenderPassLoadOperation::RPLO_Clear;
            desc.sortType = ERenderSortType::FrontToBack;
            desc.acceptedBlendModes = EBlendMode::Opaque | EBlendMode::Masked;

            if (type == "DirectionalShadow" || type == "PointShadow")
            {
                desc.passType = ERenderPassType::Shadow;
                desc.depthStencilName = name + L"_Depth";
                if (!createTarget(desc.depthStencilName, ETextureFormat::D32_FLOAT, *pass.Resolution, type == "PointShadow"))
                    return fail(index, "Could not create shadow depth target.");
                output("Depth", { desc.depthStencilName });
            }
            else if (type == "GBuffer")
            {
                desc.passType = ERenderPassType::Geometry;
                const std::pair<const wchar_t*, ETextureFormat> attachments[] = {
                    { L"_Diffuse", ETextureFormat::R8G8B8A8_UNORM },
                    { L"_Normal", ETextureFormat::R16G16B16A16_FLOAT },
                    { L"_PBR", ETextureFormat::R8G8B8A8_UNORM },
                    { L"_Emission", ETextureFormat::R8G8B8A8_UNORM },
                    { L"_Position", ETextureFormat::R32G32B32A32_FLOAT }
                };
                for (const auto& [suffix, format] : attachments)
                {
                    const auto targetName = name + suffix;
                    if (!createTarget(targetName, format))
                        return fail(index, "Could not create a G-Buffer color target.");
                    desc.renderTargetNames.push_back(targetName);
                }
                desc.depthStencilName = name + L"_Depth";
                if (!createTarget(desc.depthStencilName, ETextureFormat::D24_UNORM_S8_UINT))
                    return fail(index, "Could not create the G-Buffer depth target.");
                output("GBuffer", desc.renderTargetNames);
                output("Depth", { desc.depthStencilName });
            }
            else if (type == "DeferredLighting")
            {
                desc.passType = ERenderPassType::Lighting;
                desc.sortType = ERenderSortType::None;
                desc.acceptedBlendModes = EBlendMode::None;
                const auto color = name + L"_HdrColor";
                if (!createTarget(color, ETextureFormat::R16G16B16A16_FLOAT, 0, false, vec4(0.f, 0.f, 0.f, 1.f)))
                    return fail(index, "Could not create the HDR color target.");
                desc.renderTargetNames = { color };
                const auto& gbuffer = input("GBuffer");
                std::copy(gbuffer.begin(), gbuffer.end(), m_LightingInputs.begin());
                m_LightingInputs.back() = input("DirectionalShadow").front();
                m_PointShadowInput = input("PointShadow").front();
                output("HdrColor", desc.renderTargetNames);
            }
            else
            {
                const bool transparent = type == "ForwardTransparent";
                desc.passType = transparent ? ERenderPassType::ForwardTransparent : ERenderPassType::Forward;
                desc.sortType = transparent ? ERenderSortType::BackToFront : ERenderSortType::FrontToBack;
                desc.acceptedBlendModes = transparent
                    ? EBlendMode::AlphaBlend | EBlendMode::Additive | EBlendMode::NonPremultiplied | EBlendMode::Decal
                    : EBlendMode::Forward;
                desc.loadOperation = desc.stencilLoadOperation = ERenderPassLoadOperation::RPLO_Load;
                desc.renderTargetNames = input("Color");
                desc.depthStencilName = input("Depth").front();
                output("Color", desc.renderTargetNames);
                output("Depth", { desc.depthStencilName });
            }

            const auto id = passes.RegisterRenderPass(std::move(desc));
            if (id == INVALID_PASS_ID)
                return fail(index, "Could not register the render pass.");
            m_Passes.emplace_back(type, id);
        }

        m_ColorTarget = outputs.at(profile.Output).front();
        return {};
    }

    void DeferredProfilePasses::Free()
    {
        if (RenderPassManager::IsCreated())
        {
            auto& passes = RenderPassManager::Get();
            for (auto pass = m_Passes.rbegin(); pass != m_Passes.rend(); ++pass)
                passes.RemoveRenderPass(pass->second);
        }
        if (RenderTargetManager::IsCreated())
        {
            auto& targets = RenderTargetManager::Get();
            for (auto target = m_Targets.rbegin(); target != m_Targets.rend(); ++target)
                targets.RemoveRenderTarget(*target);
        }

        m_Passes.clear();
        m_Targets.clear();
        m_LightingInputs = {};
        m_PointShadowInput.clear();
        m_ColorTarget.clear();
        m_UsesScenePostProcess = false;
    }

    RenderPassID DeferredProfilePasses::GetPass(std::string_view type) const
    {
        for (const auto& [name, id] : m_Passes)
        {
            if (name == type)
                return id;
        }
        return INVALID_PASS_ID;
    }
}
