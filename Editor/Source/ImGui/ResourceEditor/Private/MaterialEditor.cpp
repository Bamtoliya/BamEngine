#pragma once
#include "MaterialEditor.h"

#include "MaterialInterface.h"
#include "MaterialInstance.h"
#include "RenderComponent.h"
#include "ResourceManager.h"
#include "PropertyDrawer.h"
#include "InspectorHelper.h"
#include "FileDialogs.h"

#include "RenderTargetManager.h"
#include "RenderPassManager.h"
#include "Renderer.h"
#include "RHI.h"
#include "Mesh.h"
#include "PipelineManager.h"
#include "Material.h"
#include "LightManager.h"
#include "SamplerManager.h"

namespace
{
    bool DrawMaterialPropertiesWithPropertyDrawer(void* instance, const TypeInfo& typeInfo)
    {
        bool anyChanged = false;
        const TypeInfo* currentTypeInfo = &typeInfo;

        while (currentTypeInfo)
        {
            if (currentTypeInfo != &typeInfo)
            {
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();
            }

            unordered_map<string, vector<const Engine::PropertyInfo*>> categoryMap;
            vector<const Engine::PropertyInfo*> defaultProps;

            for (const auto& prop : currentTypeInfo->Properties)
            {
                if (!GetMetadataEditable(prop.Metadata))
                {
                    continue;
                }

                string category = GetMetadataString(prop.Metadata, MetaCategoryHash);
                if (!category.empty())
                {
                    categoryMap[category].push_back(&prop);
                }
                else
                {
                    defaultProps.push_back(&prop);
                }
            }

            anyChanged |= PropertyDrawer::DrawPropertyTable(instance, *currentTypeInfo, defaultProps);

            for (const auto& [category, props] : categoryMap)
            {
                ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.2f, 0.2f, 0.25f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.25f, 0.25f, 0.3f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.15f, 0.15f, 0.2f, 1.0f));

                string categoryName = category.empty() ? "General" : category;
                bool categoryOpened = ImGui::CollapsingHeader(categoryName.c_str(), ImGuiTreeNodeFlags_None);
                ImGui::PopStyleColor(3);

                if (categoryOpened)
                {
                    anyChanged |= PropertyDrawer::DrawPropertyTable(instance, *currentTypeInfo, props);
                }
            }

            currentTypeInfo = currentTypeInfo->ParentQualifiedName.empty()
                ? nullptr
                : reflection::Registry::Get().GetTypeByQualifiedName(currentTypeInfo->ParentQualifiedName);
        }

        return anyChanged;
    }
}


#pragma region Constructor&Destructor
void MaterialEditor::Initialize()
{
    using namespace Engine;
    auto& rtMgr = RenderTargetManager::Get();
    auto& rpMgr = RenderPassManager::Get();

    uint32 w = 256;
    uint32 h = 256;
    wstring prefix = L"MatPreview_";

    // 1. G-Buffer 5종 생성
    struct { const wchar_t* suffix; ETextureFormat fmt; ETextureUsage usage; } gBufferDefs[] = {
        { L"GBuffer_Diffuse",  ETextureFormat::R8G8B8A8_UNORM, ETextureUsage::RenderTarget | ETextureUsage::Sampler },
        { L"GBuffer_Normal",   ETextureFormat::R16G16B16A16_FLOAT, ETextureUsage::RenderTarget | ETextureUsage::Sampler },
        { L"GBuffer_PBR",      ETextureFormat::R8G8B8A8_UNORM, ETextureUsage::RenderTarget | ETextureUsage::Sampler },
        { L"GBuffer_Emission", ETextureFormat::R8G8B8A8_UNORM, ETextureUsage::RenderTarget | ETextureUsage::Sampler },
        { L"GBuffer_Position", ETextureFormat::R32G32B32A32_FLOAT, ETextureUsage::RenderTarget | ETextureUsage::Sampler },
    };
    vector<wstring> gBufferNames;
    int32 count = 0;
    for (count = 0; count < 5; ++count)
    {
        m_GBufferNames[count] = prefix + gBufferDefs[count].suffix;
        RenderTargetDesc rtDesc = {};
        rtDesc.name = m_GBufferNames[count];
		RHITextureDesc& texDesc = rtDesc.textureDesc;
		texDesc.width = w;
		texDesc.height = h;
		texDesc.format = gBufferDefs[count].fmt;
		texDesc.usage = gBufferDefs[count].usage;
        rtMgr.CreateRenderTarget(&rtDesc);
        gBufferNames.push_back(m_GBufferNames[count]);
    }

    // 2. Depth, Shadow(Dummy), FinalColor 렌더타겟 생성
    m_DepthName = prefix + L"Depth";
    RenderTargetDesc depthDesc = {};
    depthDesc.name = m_DepthName;
    depthDesc.textureDesc.width = w;
    depthDesc.textureDesc.height = h;
    depthDesc.textureDesc.format = ETextureFormat::D24_UNORM_S8_UINT;
	depthDesc.textureDesc.usage = ETextureUsage::DepthStencilTarget | ETextureUsage::Sampler;
    rtMgr.CreateRenderTarget(&depthDesc);

    m_ShadowDepthName = prefix + L"ShadowDepth";
    RenderTargetDesc shadowDesc = {};
    shadowDesc.name = m_ShadowDepthName;
    shadowDesc.textureDesc.width = w;
    shadowDesc.textureDesc.height = h;
    shadowDesc.textureDesc.format = ETextureFormat::D32_FLOAT;
    shadowDesc.textureDesc.usage = ETextureUsage::DepthStencilTarget | ETextureUsage::Sampler;
    rtMgr.CreateRenderTarget(&shadowDesc);

    m_FinalColorName = prefix + L"FinalColor";
    RenderTargetDesc finalDesc = {};
	finalDesc.textureDesc.width = w;
	finalDesc.textureDesc.height = h;
	finalDesc.textureDesc.format = ETextureFormat::R8G8B8A8_UNORM;

    finalDesc.name = m_FinalColorName;
    finalDesc.textureDesc.width = w;
    finalDesc.textureDesc.height = h;
    finalDesc.textureDesc.format = ETextureFormat::R8G8B8A8_UNORM;
    finalDesc.textureDesc.clearColor = vec4(0.15f, 0.15f, 0.15f, 1.0f);
    finalDesc.textureDesc.usage = ETextureUsage::RenderTarget | ETextureUsage::Sampler;
    rtMgr.CreateRenderTarget(&finalDesc);
    m_FinalColorRT = rtMgr.GetRenderTarget(m_FinalColorName);

    // 3. RenderPass 2개 등록 (Geometry 파트와 Lighting 파트)
    m_GeometryPassID = rpMgr.RegisterRenderPass(
        L"MatPreview_GeometryPass",
        gBufferNames, m_DepthName,
        ERenderPassLoadOperation::RPLO_Clear, ERenderPassStoreOperation::RPSO_Store,
        ERenderPassLoadOperation::RPLO_Clear, ERenderPassStoreOperation::RPSO_Store,
        vec4(0.f, 0.f, 0.f, -1.f), 1000, ERenderSortType::None, ERenderPassType::Geometry
    );

    m_LightingPassID = rpMgr.RegisterRenderPass(
        L"MatPreview_LightingPass",
        { m_FinalColorName }, L"",
        ERenderPassLoadOperation::RPLO_Clear, ERenderPassStoreOperation::RPSO_Store,
        ERenderPassLoadOperation::RPLO_Clear, ERenderPassStoreOperation::RPSO_Store,
        vec4(0.15f, 0.15f, 0.15f, 1.0f), 1010, ERenderSortType::None, ERenderPassType::Lighting
    );

    // 4. Lighting 파이프라인 생성 (ViewportPanel과 동일한 방식)
    ResourceManager& rm = ResourceManager::Get();
    RHIPipelineDesc pipeDesc = {};
    pipeDesc.pipelineType = EPipelineType::Graphics;
    pipeDesc.vertexShader = rm.GetResourceHandle<Shader>(L"FullscreenQuadVS")->GetRHIShader();
    pipeDesc.pixelShader = rm.GetResourceHandle<Shader>(L"LightingPS")->GetRHIShader();
    pipeDesc.colorAttachmentCount = 1;
    pipeDesc.colorAttachmentFormats[0] = ETextureFormat::R8G8B8A8_UNORM;
    pipeDesc.depthStencilAttachmentFormat = ETextureFormat::UNKNOWN;
    pipeDesc.depthStencilState.depthTestEnable = false;
    pipeDesc.depthStencilState.depthWriteEnable = false;
    pipeDesc.topology = ETopology::TriangleList;
    pipeDesc.cullMode = ECullMode::None;
    pipeDesc.blendState = Engine::BlendState{};
    m_LightingPipeline = PipelineManager::Get().GetOrCreatePipeline(pipeDesc);
}


void MaterialEditor::Free()
{
}
#pragma endregion

bool MaterialEditor::IsSupported(const std::filesystem::path& assetPath) const
{
    const auto ext = assetPath.extension().wstring();
    return ext == L".bammat" || ext == L".bammatinst";
}

#pragma region Loop
void MaterialEditor::Update(f32 dt)
{
    using namespace Engine;
    if (!m_Open) return;

    MaterialInterface* mat = nullptr;
    if (m_ContextRenderComponent)
        mat = m_ContextRenderComponent->GetEditableMaterial(m_ContextSlot, m_EditDynamicInstance);
    else if (!m_TargetResourcePath.empty())
    {
        wstring targetKey = NormalizePath(m_TargetResourcePath.wstring());
        auto matHandle = ResourceManager::Get().GetResourceHandle<MaterialInterface>(targetKey);
        if (matHandle) mat = matHandle.Get();
    }
    if (!mat) return;

    // 공통으로 쓸 카메라 세팅 함수
    auto setupCamera = [dt](RHI* rhi) {
        CameraBuffer cb = {};
        cb.viewMatrix = glm::lookAt(vec3(0.0f, 0.0f, -3.0f), vec3(0.0f), vec3(0.0f, 1.0f, 0.0f));
        cb.projMatrix = glm::perspective(glm::radians(60.0f), 1.0f, 0.1f, 100.0f);
        if (Renderer::Get().GetRHIType() == ERHIType::SDLGPU || Renderer::Get().GetRHIType() == ERHIType::Vulkan)
            cb.projMatrix[1][1] *= -1;
        cb.viewProjMatrix = cb.projMatrix * cb.viewMatrix;
        cb.cameraPosition = vec3(0.0f, 0.0f, -3.0f);
        cb.time = dt;

        rhi->BindConstantBuffer(&cb, sizeof(CameraBuffer), 0);
        rhi->BindConstantBuffer(&cb, sizeof(CameraBuffer), 0);
        };

    // [Pass 1] Geometry Pass: Sphere 구체를 그려 G-Buffer 5장 생성
    Renderer::Get().SubmitCustomCommand([this, mat, setupCamera](f32 deltaTime, RenderPass* pass) -> EResult
        {
            RHI* rhi = Renderer::Get().GetRHI();
            ResourceManager& rm = ResourceManager::Get();

            auto meshHandle = rm.GetResourceHandle<Mesh>(L"Resources/Model/UVSphere_Sphere.001.bammesh");
            if (!meshHandle) return EResult::Fail;
            Mesh* mesh = meshHandle.Get();

            setupCamera(rhi);

            struct SceneUBO { mat4 worldMatrix; };
            SceneUBO uboData = { glm::identity<mat4>() };
            rhi->BindConstantBuffer(&uboData, sizeof(SceneUBO), 1);

            if (IsFailure(mat->Bind(2))) return EResult::Fail;

            RHIPipelineDesc pipeDesc = {};
            pipeDesc.topology = mesh->GetTopology();
            pipeDesc.pipelineType = EPipelineType::Graphics;
            pipeDesc.vertexShader = mat->GetVertexShader()->GetRHIShader();
            pipeDesc.pixelShader = mat->GetPixelShader()->GetRHIShader();
            pipeDesc.blendState = mat->GetBlendState();
            pipeDesc.cullMode = mat->GetCullMode();
            pipeDesc.colorAttachmentCount = 5; // 5개의 G-Buffer

            auto& rtMgr = RenderTargetManager::Get();
            for (int i = 0; i < 5; ++i)
                pipeDesc.colorAttachmentFormats[i] = rtMgr.GetRenderTarget(m_GBufferNames[i])->GetFormat();

            pipeDesc.depthStencilAttachmentFormat = rtMgr.GetRenderTarget(m_DepthName)->GetFormat();
            pipeDesc.inputLayouts = mesh->GetInputLayoutDescs();

            pipeDesc.depthStencilState.depthTestEnable = (mat->GetDepthMode() != EDepthMode::None);
            pipeDesc.depthStencilState.depthWriteEnable = pipeDesc.depthStencilState.depthTestEnable && (mat->GetDepthMode() == EDepthMode::ReadWrite);
            pipeDesc.depthStencilState.depthCompareOp = mat->GetDepthCompareOp();

            RHIPipeline* pipeline = PipelineManager::Get().GetOrCreatePipeline(pipeDesc);
            if (!pipeline || IsFailure(rhi->BindPipeline(pipeline))) return EResult::Fail;

            if (IsFailure(mesh->Bind(0))) return EResult::Fail;
            if (mesh->GetIndexBuffer()) rhi->DrawIndexed(mesh->GetIndexCount());
            else rhi->Draw(mesh->GetVertexCount());

            return EResult::Success;
        }, m_GeometryPassID);

    // [Pass 2] Lighting Pass: 만들어진 5장의 텍스처와 빛 정보를 합쳐 최종 색상 계산
    Renderer::Get().SubmitCustomCommand([this, setupCamera](f32 deltaTime, RenderPass* pass) -> EResult
        {
            RHI* rhi = Renderer::Get().GetRHI();
            auto& rtMgr = RenderTargetManager::Get();
            RHISampler* sampler = SamplerManager::Get().GetDefaultSampler();

            setupCamera(rhi);

            // G-Buffer 바인딩 (LightingPS 구조에 맞춰 슬롯 0~4번)
            for (int i = 0; i < 5; ++i)
                rhi->BindTextureSampler(rtMgr.GetRenderTarget(m_GBufferNames[i])->GetTexture(), sampler, i);

            // 5번 슬롯은 ShadowMap용
            rhi->BindTextureSampler(rtMgr.GetRenderTarget(m_ShadowDepthName)->GetTexture(), sampler, 5);

            // 셰도우 데이터 빈 구조체 바인딩 (에러 방지용)
            struct tagLightShadowData { mat4 mat[4]; vec4 splits; } shadowData = {};
            rhi->BindConstantBuffer(&shadowData, sizeof(shadowData), 1);

            rhi->BindPipeline(m_LightingPipeline);

            // 메인 씬의 빛(LightManager) 정보 바인딩 적용
            if (IsFailure(LightManager::Get().Bind(0))) return EResult::Fail;

            rhi->Draw(3); // Fullscreen Quad
            return EResult::Success;
        }, m_LightingPassID);
}


void MaterialEditor::Draw()
{
    using namespace Engine;

    if (!m_Open) return;

    string title = "Material Editor";
    if (!m_TargetResourcePath.empty())
    {
        title += " - " + m_TargetResourcePath.filename().string();
    }

    if (!ImGui::Begin(title.c_str(), &m_Open))
    {
        ImGui::End();
        return;
    }

    MaterialInterface* mat = nullptr;
    wstring targetKey;

    // 1) 런타임 컨텍스트가 있으면 슬롯 머티리얼 우선
    if (m_ContextRenderComponent)
    {
        ImGui::SeparatorText("Instance Context");

        const uint32 slotCount = std::max(m_ContextRenderComponent->GetMaterialSlotCount(), 1u);
        m_ContextSlot = std::min(m_ContextSlot, slotCount - 1);

        string preview = "Slot " + to_string(m_ContextSlot);
        if (ImGui::BeginCombo("Material Slot", preview.c_str()))
        {
            for (uint32 i = 0; i < slotCount; ++i)
            {
                bool sel = (m_ContextSlot == i);
                if (ImGui::Selectable(("Slot " + to_string(i)).c_str(), sel))
                    m_ContextSlot = i;
                if (sel) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        const bool hasDynamic = m_ContextRenderComponent->HasDynamicMaterialInstance(m_ContextSlot);
        ImGui::Text("Dynamic: %s", hasDynamic ? "Yes" : "No");
        ImGui::SameLine();

        if (!hasDynamic)
        {
            if (ImGui::Button("Make Dynamic Instance"))
            {
                m_ContextRenderComponent->CreateDynamicMaterialInstance(m_ContextSlot);
            }
        }
        else
        {
            ImGui::BeginDisabled();
            ImGui::Button("Make Dynamic Instance");
            ImGui::EndDisabled();
        }

        ImGui::Checkbox("Edit Dynamic Instance", &m_EditDynamicInstance);

        // true면 Dynamic을 강제 생성/편집, false면 Shared 편집
        mat = m_ContextRenderComponent->GetEditableMaterial(m_ContextSlot, m_EditDynamicInstance);

        if (MaterialInterface* sharedMat = m_ContextRenderComponent->GetSharedMaterial(m_ContextSlot))
        {
            targetKey = sharedMat->GetKey();
        }
        if (mat && targetKey.empty())
        {
            targetKey = mat->GetKey();
        }
    }
    else
    {
        // 2) 리소스 경로 타겟 편집
        if (m_TargetResourcePath.empty())
        {
            ImGui::TextDisabled("No target material.");
            ImGui::End();
            return;
        }

        targetKey = NormalizePath(m_TargetResourcePath.wstring());
        ResourceHandle<MaterialInterface> matHandle =
            ResourceManager::Get().GetResourceHandle<MaterialInterface>(targetKey);

        if (!matHandle)
        {
            Handle h = ResourceManager::Get().LoadFile(targetKey);
            if (h.IsValid())
            {
                matHandle = ResourceHandle<MaterialInterface>(h);
            }
        }

        mat = matHandle.Get();
    }

    if (!mat)
    {
        ImGui::TextDisabled("Failed to resolve material.");
        ImGui::End();
        return;
    }

    if (m_FinalColorRT && m_FinalColorRT->GetTexture())
    {
        ImGui::SeparatorText("Preview");
        void* texID = (void*)(size_t)m_FinalColorRT->GetTexture()->GetNativeHandle();

        float availWidth = ImGui::GetContentRegionAvail().x;
        if (availWidth > 256.0f) ImGui::SetCursorPosX((availWidth - 256.0f) * 0.5f);

        ImGui::Image(texID, ImVec2(256, 256));
    }
    // =======================================================

    ImGui::SeparatorText("Save");

    const bool editingDynamic = m_ContextRenderComponent
        && m_EditDynamicInstance
        && m_ContextRenderComponent->HasDynamicMaterialInstance(m_ContextSlot);

    wstring savePath = mat->GetKey();
    if (savePath.empty())
        savePath = targetKey;

    if (!savePath.empty() && !editingDynamic)
    {
        if (ImGui::Button("Save"))
            ResourceManager::Get().SaveToJsonFile(mat, savePath);
    }
    else
    {
        ImGui::BeginDisabled();
        ImGui::Button("Save");
        ImGui::EndDisabled();
        if (editingDynamic)
        {
            ImGui::SameLine();
            ImGui::TextDisabled("(Dynamic - saved with scene)");
        }
    }

    ImGui::SameLine();
    if (ImGui::Button("Save As"))
    {
        wstring outPath;
        const bool isInst = (dynamic_cast<MaterialInstance*>(mat) != nullptr);
        const wchar_t* defaultExt = isInst ? L"bammatinst" : L"bammat";
        if (FileDialogs::SaveFileDialog(outPath, { { L"Material Files", L"*.bammat;*.bammatinst" } }, L"NewMaterial", defaultExt))
            ResourceManager::Get().SaveToJsonFile(mat, outPath);
    }

    ImGui::Separator();

    if (const TypeInfo* typeInfo = reflection::Registry::Get().GetType(mat->GetTypeID()))
    {
        ImGui::PushID(mat);

        const bool opened = PropertyDrawer::DrawHeaderNode(mat, *typeInfo);
        if (opened)
        {
            const bool changed = DrawMaterialPropertiesWithPropertyDrawer(mat, *typeInfo);
            if (changed)
            {
                ImGui::Spacing();
                ImGui::TextDisabled("Modified (not saved)");
            }
        }

        ImGui::PopID();
    }
    ImGui::End();
}
#pragma endregion