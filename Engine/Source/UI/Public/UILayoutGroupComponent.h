#pragma once

#include "ComponentAnnotations.h"
#include <glm/glm.hpp>

namespace Engine
{
    ENUM()
    enum class EUILayoutGroupMode
    {
        Row,
        Column,
        Grid
    };

    STRUCT(COMPONENT, DISPLAY_NAME("STRUCT_UI_LAYOUT_GROUP"), CATEGORY("CATEGORY_UI"))
    struct UILayoutGroupComponent
    {
        REFLECT_BODY()

        PROPERTY(EDITABLE, DISPLAY_NAME("PROP_UI_LAYOUT_MODE"))
        EUILayoutGroupMode mode = EUILayoutGroupMode::Row;
        // Left, top, right, bottom, in parent-local UI pixels.
        PROPERTY(EDITABLE, DISPLAY_NAME("PROP_UI_LAYOUT_PADDING"), RANGE(0.0, 8192.0, 1.0))
        glm::vec4 padding{ 0.0f };
        PROPERTY(EDITABLE, DISPLAY_NAME("PROP_UI_LAYOUT_SPACING"), RANGE(0.0, 8192.0, 1.0))
        glm::vec2 spacing{ 8.0f };
        // (0, 0) is top-left; (1, 1) is bottom-right.
        PROPERTY(EDITABLE, DISPLAY_NAME("PROP_UI_LAYOUT_ALIGNMENT"), RANGE(0.0, 1.0, 0.01))
        glm::vec2 alignment{ 0.0f };
        PROPERTY(EDITABLE, DISPLAY_NAME("PROP_UI_LAYOUT_CELL_SIZE"), EDITCONDITION(mode == Grid), RANGE(1.0, 8192.0, 1.0))
        glm::vec2 cellSize{ 100.0f };
        PROPERTY(EDITABLE, DISPLAY_NAME("PROP_UI_LAYOUT_COLUMNS"), EDITCONDITION(mode == Grid), RANGE(1, 256))
        int32 columns = 4;
    };
}
