from collections import Counter

from model import ReflectedFunction

from .common import get_property_type, normalize_cpp_type


def make_function_pointer_cast(reflected_function: ReflectedFunction) -> str:
    return_type = normalize_cpp_type(reflected_function.return_type_raw)

    if reflected_function.params:
        args = ", ".join(param.normalized_type() for param in reflected_function.params)
    else:
        args = ""

    qualifier_suffix = reflected_function.qualifiers.to_suffix_string()

    return (
        f"static_cast<{return_type} ({reflected_function.owner_name}::*)({args})"
        f"{qualifier_suffix}>(&{reflected_function.owner_name}::{reflected_function.name})"
    )


def emit_function_params_block(owner_name: str, reflected_function: ReflectedFunction) -> str:
    if not reflected_function.has_params():
        return ""

    unique_function_name = f"{reflected_function.name}_{reflected_function.overload_suffix()}"

    lines: list[str] = []
    lines.append(f"DECLARE_FUNCTION_PARAMS({owner_name}, {unique_function_name})")

    for param in reflected_function.params:
        clean_type = param.normalized_type()
        clean_type = clean_type.replace("const", "").replace("&", "").replace("*", "").strip()

        property_type_name, reflected_type_name = get_property_type(clean_type)
        lines.append(
            f'\tFUNCTION_PARAM("{reflected_type_name}", reflection::EPropertyType::{property_type_name})'
        )

    lines.append("END_FUNCTION_PARAMS")
    lines.append("")
    return "\n".join(lines)


def emit_functions_block(
    owner_name: str,
    owner_qualified_name: str,
    functions: list[ReflectedFunction],
) -> str:
    lines: list[str] = []
    function_lines: list[str] = []

    name_counts = Counter(function.name for function in functions)

    for reflected_function in functions:
        params_block = emit_function_params_block(owner_name, reflected_function)
        if params_block:
            lines.append(params_block.rstrip())

        clean_return_type = normalize_cpp_type(reflected_function.return_type_raw)
        for token in ("virtual", "static", "inline", "constexpr", "explicit", "override", "final"):
            clean_return_type = clean_return_type.replace(token, "")
        clean_return_type = clean_return_type.strip()

        property_type_name, reflected_return_type = get_property_type(
            clean_return_type.replace("const", "").replace("&", "").replace("*", "").strip()
        )

        params_argument = "{}"
        if reflected_function.has_params():
            params_argument = (
                f"{owner_name}_{reflected_function.name}_{reflected_function.overload_suffix()}_Params"
            )

        is_overloaded = name_counts[reflected_function.name] > 1

        if is_overloaded:
            function_pointer = make_function_pointer_cast(reflected_function)
            function_lines.append(
                f'REFLECT_FUNCTION_OVERLOAD_EX({owner_name}, "{owner_qualified_name}", '
                f'"{reflected_function.name}", "{reflected_function.signature}", {function_pointer}, '
                f'"{reflected_return_type}", reflection::EPropertyType::{property_type_name}, {params_argument})'
            )
        else:
            function_lines.append(
                f'REFLECT_FUNCTION_EX({owner_name}, "{owner_qualified_name}", '
                f'{reflected_function.name}, "{reflected_return_type}", '
                f'reflection::EPropertyType::{property_type_name}, {params_argument})'
            )

    if not function_lines:
        lines.append(f"EMPTY_FUNCTIONS({owner_name})")
        lines.append("")
        return "\n".join(lines)

    lines.append(f"BEGIN_FUNCTIONS({owner_name})")
    for function_line in function_lines:
        lines.append(f"\t{function_line}")
    lines.append("END_FUNCTIONS")
    lines.append("")
    return "\n".join(lines)