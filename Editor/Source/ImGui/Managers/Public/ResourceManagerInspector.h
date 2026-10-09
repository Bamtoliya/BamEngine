#pragma once

#include "ImGuiInterface.h"
#include "ResourceManager.h"

BEGIN(Editor)

class ResourceManagerInspector final : public ImGuiInterface
{
public:
    ResourceManagerInspector();
    ~ResourceManagerInspector() override = default;

    void Update(f32 dt) override;
    void Draw() override;

private:
    struct ResourceRow
    {
        Engine::uint32 SlotIndex = 0;
        Engine::uint32 Generation = 0;
        Engine::uint32 RefCount = 0;

        std::string Type;
        std::string Key;
        std::string Path;
        std::string SearchText;
    };

    void Refresh();
    void RebuildVisibleRows();

    void DrawToolbar();
    void DrawTable();
    void DrawRow(const ResourceRow& row);

    static const char* GetTypeName(Engine::EResourceType type);
    static void DrawTextCell(const std::string& text);

private:
    std::vector<ResourceRow> m_Rows;
    std::vector<size_t> m_VisibleRows;

    ImGuiTextFilter m_Filter;

    bool m_AutoRefresh = true;
    bool m_HasSnapshot = false;
    float m_RefreshElapsed = 0.0f;
};

END