from model import ReflectedType

from .function_emitter import emit_functions_block
from .property_emitter import emit_properties_block


def emit_type_reflection(reflected_type: ReflectedType, bitflag_enums: set[str]) -> str:
    lines: list[str] = []

    lines.append(f"#pragma region {reflected_type.kind}: {reflected_type.qualified_name}")

    properties_block = emit_properties_block(
        owner_name=reflected_type.name,
        properties=reflected_type.properties,
        bitflag_enums=bitflag_enums,
    )
    if properties_block:
        lines.append(properties_block.rstrip())

    functions_block = emit_functions_block(
        owner_name=reflected_type.name,
        owner_qualified_name=reflected_type.qualified_name,
        functions=reflected_type.functions,
    )
    if functions_block:
        lines.append(functions_block.rstrip())

    lines.append(
        f'IMPLEMENT_CLASS_EX({reflected_type.name}, "{reflected_type.qualified_name}", '
        f'"{reflected_type.parent_qualified_name}")'
    )
    lines.append("")
    lines.append(f"#pragma endregion // {reflected_type.kind}: {reflected_type.qualified_name}")
    lines.append("")

    return "\n".join(lines)