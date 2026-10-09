#pragma once

#define NAME(value) META("DisplayName", value)
#define CATEGORY(value) META("Category", value)
#define EDITABLE META("Editable", true)
#define READONLY META("ReadOnly", true)
#define RANGE(minimum, maximum) META("RangeMin", minimum), META("RangeMax", maximum)
#define HELP_TEXT(value) META("Help", value)
#define TOOLTIP(value) HELP_TEXT(value)
