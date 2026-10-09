#include "ResourceManagerInspector.h"

#include <algorithm>
#include <cfloat>

BEGIN(Editor)

ResourceManagerInspector::ResourceManagerInspector()
{
    m_Name = L"Resource Manager";
}

void ResourceManagerInspector::Update(f32 dt)
{
    if (!m_Open)
        return;

    m_RefreshElapsed += dt;

    if (!m_HasSnapshot ||
        (m_AutoRefresh && m_RefreshElapsed >= 0.5f))
    {
        Refresh();
    }
}

void ResourceManagerInspector::Refresh()
{
    const auto snapshot =
        Engine::ResourceManager::Get().GetDebugSnapshot();

    m_Rows.clear();
    m_Rows.reserve(snapshot.size());

    for (const Engine::ResourceDebugInfo& info : snapshot)
    {
        ResourceRow row;
        row.SlotIndex = info.SlotIndex;
        row.Generation = info.Generation;
        row.RefCount = info.RefCount;

        row.Type = GetTypeName(info.Type);
        row.Key = Engine::WStrToStr(info.Key);
        row.Path = Engine::WStrToStr(info.Path);

        row.SearchText =
            row.Type + " " + row.Key + " " + row.Path;

        m_Rows.push_back(std::move(row));
    }

    // 갱신되어도 목록의 순서가 크게 흔들리지 않도록 정렬한다.
    std::sort(
        m_Rows.begin(),
        m_Rows.end(),
        [](const ResourceRow& lhs, const ResourceRow& rhs)
        {
            if (lhs.Key != rhs.Key)
                return lhs.Key < rhs.Key;

            return lhs.SlotIndex < rhs.SlotIndex;
        });

    RebuildVisibleRows();

    m_HasSnapshot = true;
    m_RefreshElapsed = 0.0f;
}

void ResourceManagerInspector::RebuildVisibleRows()
{
    m_VisibleRows.clear();

    for (size_t index = 0; index < m_Rows.size(); ++index)
    {
        if (m_Filter.PassFilter(m_Rows[index].SearchText.c_str()))
            m_VisibleRows.push_back(index);
    }
}

void ResourceManagerInspector::Draw()
{
    if (!m_Open)
        return;

    const string windowTitle = LOCAL("UI_RESOURCE_MANGER_INSPECTOR") + "###ResourceManagerInspector";

    if (ImGui::Begin(windowTitle.c_str(), &m_Open))
    {
        if (!m_HasSnapshot)
            Refresh();

        DrawToolbar();

        ImGui::Text(
            "Showing %zu / %zu registered resources",
            m_VisibleRows.size(),
            m_Rows.size());

        DrawTable();
    }

    ImGui::End();
}

void ResourceManagerInspector::DrawToolbar()
{
    if (ImGui::Button("Refresh"))
        Refresh();

    ImGui::SameLine();
    ImGui::Checkbox("Auto", &m_AutoRefresh);

    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Refresh every 0.5 seconds");

    ImGui::SameLine();
    ImGui::SetNextItemWidth(-FLT_MIN);

    if (ImGui::InputTextWithHint(
        "##ResourceSearch",
        "Search type, key or path...",
        m_Filter.InputBuf,
        IM_ARRAYSIZE(m_Filter.InputBuf)))
    {
        m_Filter.Build();
        RebuildVisibleRows();
    }
}

void ResourceManagerInspector::DrawTable()
{
    constexpr ImGuiTableFlags flags =
        ImGuiTableFlags_BordersInnerV |
        ImGuiTableFlags_RowBg |
        ImGuiTableFlags_Resizable |
        ImGuiTableFlags_ScrollY |
        ImGuiTableFlags_SizingStretchProp;

    if (!ImGui::BeginTable(
        "ResourceTable",
        4,
        flags,
        ImVec2(0.0f, 0.0f)))
    {
        return;
    }

    ImGui::TableSetupScrollFreeze(0, 1);

    ImGui::TableSetupColumn(
        "Type", ImGuiTableColumnFlags_WidthFixed, 110.0f);

    ImGui::TableSetupColumn(
        "Key", ImGuiTableColumnFlags_WidthStretch, 1.0f);

    ImGui::TableSetupColumn(
        "Path", ImGuiTableColumnFlags_WidthStretch, 1.5f);

    ImGui::TableSetupColumn(
        "Refs", ImGuiTableColumnFlags_WidthFixed, 55.0f);

    ImGui::TableHeadersRow();

    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(m_VisibleRows.size()));

    while (clipper.Step())
    {
        for (int index = clipper.DisplayStart;
            index < clipper.DisplayEnd;
            ++index)
        {
            DrawRow(m_Rows[m_VisibleRows[index]]);
        }
    }

    ImGui::EndTable();
}

void ResourceManagerInspector::DrawRow(const ResourceRow& row)
{
    const std::string rowID =
        std::to_string(row.SlotIndex) + ":" +
        std::to_string(row.Generation);

    ImGui::PushID(rowID.c_str());
    ImGui::TableNextRow();

    ImGui::TableSetColumnIndex(0);
    ImGui::TextUnformatted(row.Type.c_str());

    ImGui::TableSetColumnIndex(1);

    // 표시 이름에 ##가 포함돼도 잘리지 않도록 별도로 그린다.
    const ImVec2 textPosition = ImGui::GetCursorScreenPos();

    ImGui::Selectable(
        "##Resource",
        false,
        ImGuiSelectableFlags_SpanAllColumns);

    ImGui::GetWindowDrawList()->AddText(
        textPosition,
        ImGui::GetColorU32(ImGuiCol_Text),
        row.Key.empty() ? "(empty key)" : row.Key.c_str());

    if (ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();

        ImGui::TextUnformatted("Key:");
        ImGui::TextUnformatted(row.Key.c_str());

        ImGui::Separator();

        ImGui::TextUnformatted("Path:");
        ImGui::TextUnformatted(
            row.Path.empty() ? "(no path recorded)" : row.Path.c_str());

        ImGui::Separator();

        ImGui::Text(
            "Slot: %u / Generation: %u",
            static_cast<unsigned>(row.SlotIndex),
            static_cast<unsigned>(row.Generation));

        ImGui::EndTooltip();
    }

    if (ImGui::BeginPopupContextItem("ResourceActions"))
    {
        if (ImGui::MenuItem("Copy Key"))
            ImGui::SetClipboardText(row.Key.c_str());

        if (ImGui::MenuItem(
            "Copy Path", nullptr, false, !row.Path.empty()))
        {
            ImGui::SetClipboardText(row.Path.c_str());
        }

        ImGui::EndPopup();
    }

    ImGui::TableSetColumnIndex(2);
    DrawTextCell(row.Path);

    ImGui::TableSetColumnIndex(3);
    ImGui::Text("%u", static_cast<unsigned>(row.RefCount));

    ImGui::PopID();
}

void ResourceManagerInspector::DrawTextCell(
    const std::string& text)
{
    if (text.empty())
    {
        ImGui::TextDisabled("(none)");
        return;
    }

    ImGui::TextUnformatted(text.c_str());
}

const char* ResourceManagerInspector::GetTypeName(
    Engine::EResourceType type)
{
    using Engine::EResourceType;

    switch (type)
    {
    case EResourceType::Texture:          return "Texture";
    case EResourceType::Sprite:           return "Sprite";
    case EResourceType::Material:         return "Material";
    case EResourceType::MaterialInstance: return "MaterialInstance";
    case EResourceType::Shader:           return "Shader";
    case EResourceType::Mesh:             return "Mesh";
    case EResourceType::Model:            return "Model";
    case EResourceType::Animation:        return "Animation";
    case EResourceType::AudioClip:        return "AudioClip";
    case EResourceType::Skeleton:         return "Skeleton";
    case EResourceType::Prefab:           return "Prefab";
    case EResourceType::Script:           return "Script";
    case EResourceType::Scene:            return "Scene";
    case EResourceType::Save:             return "Save";
    default:                             return "Unknown";
    }
}

END