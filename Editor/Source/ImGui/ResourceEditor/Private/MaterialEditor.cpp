#pragma once
#include "MaterialEditor.h"

#include "MaterialInterface.h"
#include "MaterialInstance.h"
#include "RenderComponent.h"
#include "ResourceManager.h"
#include "PropertyDrawer.h"
#include "InspectorHelper.h"
#include "FileDialogs.h"

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

bool MaterialEditor::IsSupported(const std::filesystem::path& assetPath) const
{
    const auto ext = assetPath.extension().wstring();
    return ext == L".bammat" || ext == L".bammatinst";
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

    const TypeInfo& typeInfo = mat->GetTypeInfo();
    ImGui::PushID(mat);

    const bool opened = PropertyDrawer::DrawHeaderNode(mat, typeInfo);
    if (opened)
    {
        const bool changed = DrawMaterialPropertiesWithPropertyDrawer(mat, typeInfo);
        if (changed)
        {
            ImGui::Spacing();
            ImGui::TextDisabled("Modified (not saved)");
        }
    }

    ImGui::PopID();
    ImGui::End();
}