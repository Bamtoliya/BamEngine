#include "PropertyDrawerEnTT.h"
#include "ImGuiManager.h"
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

BEGIN(Editor)

bool PropertyDrawerEnTT::DrawHeaderNode(entt::meta_any& instance, const entt::meta_type& type)
{
    const char* typeName = type.info().name().data();
    // Simplified header node for EnTT
    const string header = LOCAL(typeName) + "###" + typeName;
    bool isOpen = ImGui::CollapsingHeader(header.c_str(), ImGuiTreeNodeFlags_DefaultOpen);
    return isOpen;
}

bool PropertyDrawerEnTT::DrawPropertyTable(entt::meta_any& instance, const entt::meta_type& type)
{
    bool anyChanged = false;

    // 컴포넌트 타입별로 ID 스택을 격리하여 다른 컴포넌트 테이블과의 컬럼 너비 충돌 방지
    ImGui::PushID(type.info().name().data());
    if (ImGui::BeginTable("PropertyTable", 2, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_Resizable))
    {
        // 다국어 변경 시에도 컬럼 폭이 유지되도록 ##Name, ##Value 부여
        const std::string colName = LOCAL("EditorCommon.Name") + "##Name";
        const std::string colValue = LOCAL("EditorCommon.Value") + "##Value";
        ImGui::TableSetupColumn(colName.c_str(), ImGuiTableColumnFlags_WidthFixed, 120.0f);
        ImGui::TableSetupColumn(colValue.c_str(), ImGuiTableColumnFlags_WidthStretch);
        for (auto [id, data] : type.data())
        {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);

            const char* propName = "Unknown";
            if constexpr (requires { data.name(); }) {
                propName = data.name() ? data.name() : "Unknown";
            }
            else {
                propName = "UnknownProperty";
            }
            ImGui::AlignTextToFramePadding();
            const string displayName = std::strcmp(propName, "UnknownProperty") == 0 ?
                LOCAL("Inspector.UnknownProperty") : std::strcmp(propName, "Unknown") == 0 ?
                LOCAL("EditorCommon.Unknown") : LOCAL(propName);
            ImGui::TextUnformatted(displayName.c_str());

            ImGui::TableSetColumnIndex(1);
            ImGui::PushID(propName);
            if (DrawProperty(instance, data))
            {
                anyChanged = true;
            }
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    ImGui::PopID(); // PushID 해제
    return anyChanged;
}

bool PropertyDrawerEnTT::DrawProperty(entt::meta_any& instance, const entt::meta_data& data)
{
    entt::meta_any propValue = data.get(instance);
    if (!propValue) return false;

    entt::meta_type valueType = propValue.type();
    bool changed = false;

    ImGui::SetNextItemWidth(-FLT_MIN);

    if (valueType == entt::resolve<int>())
    {
        int val = propValue.cast<int>();
        if (ImGui::DragInt("##val", &val))
        {
            data.set(instance, val);
            changed = true;
        }
    }
    else if (valueType == entt::resolve<float>())
    {
        float val = propValue.cast<float>();
        if (ImGui::DragFloat("##val", &val, 0.1f))
        {
            data.set(instance, val);
            changed = true;
        }
    }
    else if (valueType == entt::resolve<bool>())
    {
        bool val = propValue.cast<bool>();
        if (ImGui::Checkbox("##val", &val))
        {
            data.set(instance, val);
            changed = true;
        }
    }
    else if (valueType == entt::resolve<glm::vec3>())
    {
        glm::vec3 val = propValue.cast<glm::vec3>();
        if (ImGui::DragFloat3("##val", glm::value_ptr(val), 0.1f))
        {
            data.set(instance, val);
            changed = true;
        }
    }
    else if (valueType == entt::resolve<glm::vec2>())
    {
        glm::vec2 val = propValue.cast<glm::vec2>();
        if (ImGui::DragFloat2("##val", glm::value_ptr(val), 0.1f))
        {
            data.set(instance, val);
            changed = true;
        }
    }
    else if (valueType == entt::resolve<glm::vec4>())
    {
        glm::vec4 val = propValue.cast<glm::vec4>();
        if (ImGui::ColorEdit4("##val", glm::value_ptr(val)))
        {
            data.set(instance, val);
            changed = true;
        }
    }
    else if (valueType == entt::resolve<std::string>())
    {
        std::string val = propValue.cast<std::string>();
        char buffer[256];
        strcpy_s(buffer, val.c_str());
        if (ImGui::InputText("##val", buffer, sizeof(buffer)))
        {
            data.set(instance, std::string(buffer));
            changed = true;
        }
    }
    else
    {
        ImGui::Text("%s", LOCAL_CSTR("Inspector.UnsupportedType"));
    }

    return changed;
}

END
