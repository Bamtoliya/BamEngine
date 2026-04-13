from model import ReflectedEnum


def emit_enum_reflection(reflected_enum: ReflectedEnum) -> str:
    lines: list[str] = []
    lines.append(f"// Enum: {reflected_enum.qualified_name}")
    lines.append(f"BEGIN_ENUM({reflected_enum.name})")

    for entry in reflected_enum.entries:
        lines.append(f"\tREFLECT_ENUM_ENTRY({reflected_enum.name}, {entry})")

    lines.append(f'END_ENUM_REFLECT_EX({reflected_enum.name}, "{reflected_enum.qualified_name}")')
    lines.append("")
    return "\n".join(lines)