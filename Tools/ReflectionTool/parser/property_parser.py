import re

from model import ContainerTypeNode, ReflectedEnum, ReflectedProperty, ReflectedType, ResolvedTypeRef

from .patterns import PROPERTY_PATTERN


def normalize_cpp_type(type_str: str) -> str:
    return re.sub(r"\s+", " ", type_str.strip())


def normalize_type_name(type_name: str) -> str:
    return type_name.replace("std::", "").replace("Engine::", "").strip()


def strip_cvref_and_pointer(type_name: str) -> str:
    text = normalize_cpp_type(type_name)
    text = text.replace("const ", "").replace(" volatile", "")
    text = text.rstrip("&* ").strip()
    return text


def parse_template_args(type_str: str) -> list[str]:
    start = type_str.find("<")
    end = type_str.rfind(">")
    if start == -1 or end == -1:
        return []

    content = type_str[start + 1:end]
    parts: list[str] = []
    current = ""
    depth = 0

    for char in content:
        if char == "<":
            depth += 1
        elif char == ">":
            depth -= 1

        if char == "," and depth == 0:
            parts.append(current.strip())
            current = ""
        else:
            current += char

    if current.strip():
        parts.append(current.strip())

    return parts


def is_container_type(raw_type: str) -> bool:
    lowered = raw_type.lower()
    return lowered.startswith(
        (
            "vector<",
            "deque<",
            "list<",
            "set<",
            "unordered_set<",
            "map<",
            "unordered_map<",
        )
    )


def parse_properties(scope_content: str) -> list[ReflectedProperty]:
    properties: list[ReflectedProperty] = []

    for attributes_raw, cpp_type, name in PROPERTY_PATTERN.findall(scope_content):
        properties.append(
            ReflectedProperty(
                attributes_raw=attributes_raw.strip(),
                cpp_type=cpp_type.strip(),
                name=name.strip(),
            )
        )

    return properties


def build_symbol_tables(
    reflected_enums: list[ReflectedEnum],
    reflected_types: list[ReflectedType],
) -> tuple[dict[str, str], dict[str, str]]:
    type_symbols: dict[str, str] = {}
    enum_symbols: dict[str, str] = {}

    for reflected_type in reflected_types:
        type_symbols[reflected_type.qualified_name] = reflected_type.qualified_name
        type_symbols.setdefault(reflected_type.short_name, reflected_type.qualified_name)

    for reflected_enum in reflected_enums:
        enum_symbols[reflected_enum.qualified_name] = reflected_enum.qualified_name
        enum_symbols.setdefault(reflected_enum.short_name, reflected_enum.qualified_name)

    return type_symbols, enum_symbols


def resolve_symbol_name(
    raw_name: str,
    current_namespace_parts: list[str],
    symbol_table: dict[str, str],
) -> str:
    normalized = normalize_type_name(strip_cvref_and_pointer(raw_name))
    if not normalized:
        return ""

    exact_match = symbol_table.get(normalized)
    if exact_match:
        return exact_match

    if current_namespace_parts:
        local_candidate = "::".join(current_namespace_parts + [normalized])
        local_match = symbol_table.get(local_candidate)
        if local_match:
            return local_match

    engine_candidate = f"Engine::{normalized}"
    engine_match = symbol_table.get(engine_candidate)
    if engine_match:
        return engine_match

    return ""


def resolve_type_ref(
    raw_type: str,
    current_namespace_parts: list[str],
    type_symbols: dict[str, str],
    enum_symbols: dict[str, str],
) -> ResolvedTypeRef:
    stripped = strip_cvref_and_pointer(raw_type)

    resolved_enum = resolve_symbol_name(stripped, current_namespace_parts, enum_symbols)
    if resolved_enum:
        return ResolvedTypeRef(
            raw_name=raw_type,
            resolved_name=stripped.split("::")[-1],
            resolved_qualified_name=resolved_enum,
            property_kind="Enum",
        )

    resolved_type = resolve_symbol_name(stripped, current_namespace_parts, type_symbols)
    if resolved_type:
        return ResolvedTypeRef(
            raw_name=raw_type,
            resolved_name=stripped.split("::")[-1],
            resolved_qualified_name=resolved_type,
            property_kind="Object" if normalize_cpp_type(raw_type).endswith("*") else "Struct",
        )

    return ResolvedTypeRef(
        raw_name=raw_type,
        resolved_name=stripped,
        resolved_qualified_name="",
        property_kind="",
    )


def resolve_container_node(
    raw_type: str,
    current_namespace_parts: list[str],
    type_symbols: dict[str, str],
    enum_symbols: dict[str, str],
) -> ContainerTypeNode:
    normalized = normalize_type_name(normalize_cpp_type(raw_type))
    lowered = normalized.lower()
    args = parse_template_args(normalized)

    if "map" in lowered:
        key_raw = args[0] if len(args) > 0 else "void"
        value_raw = args[1] if len(args) > 1 else "void"

        value_type: ResolvedTypeRef | None = None
        inner_container: ContainerTypeNode | None = None

        if is_container_type(value_raw):
            inner_container = resolve_container_node(
                value_raw,
                current_namespace_parts,
                type_symbols,
                enum_symbols,
            )
        else:
            value_type = resolve_type_ref(
                value_raw,
                current_namespace_parts,
                type_symbols,
                enum_symbols,
            )

        return ContainerTypeNode(
            container_kind="Map",
            raw_type=normalized,
            key_type=resolve_type_ref(
                key_raw,
                current_namespace_parts,
                type_symbols,
                enum_symbols,
            ),
            value_type=value_type,
            inner_container=inner_container,
        )

    inner_raw = args[0] if args else "void"

    if "set" in lowered:
        kind = "Set"
    elif lowered.startswith("list<"):
        kind = "List"
    else:
        kind = "Array"

    value_type: ResolvedTypeRef | None = None
    inner_container: ContainerTypeNode | None = None

    if is_container_type(inner_raw):
        inner_container = resolve_container_node(
            inner_raw,
            current_namespace_parts,
            type_symbols,
            enum_symbols,
        )
    else:
        value_type = resolve_type_ref(
            inner_raw,
            current_namespace_parts,
            type_symbols,
            enum_symbols,
        )

    return ContainerTypeNode(
        container_kind=kind,
        raw_type=normalized,
        value_type=value_type,
        inner_container=inner_container,
    )


def resolve_single_property_type(
    prop: ReflectedProperty,
    current_namespace_parts: list[str],
    type_symbols: dict[str, str],
    enum_symbols: dict[str, str],
) -> None:
    raw_type = normalize_type_name(prop.normalized_cpp_type())

    if is_container_type(raw_type):
        prop.container_type = resolve_container_node(
            raw_type,
            current_namespace_parts,
            type_symbols,
            enum_symbols,
        )
        prop.resolved_type = None
        return

    prop.resolved_type = resolve_type_ref(
        raw_type,
        current_namespace_parts,
        type_symbols,
        enum_symbols,
    )
    prop.container_type = None


def resolve_property_types(
    reflected_enums: list[ReflectedEnum],
    reflected_types: list[ReflectedType],
) -> None:
    type_symbols, enum_symbols = build_symbol_tables(reflected_enums, reflected_types)

    for reflected_type in reflected_types:
        for prop in reflected_type.properties:
            resolve_single_property_type(
                prop=prop,
                current_namespace_parts=reflected_type.namespace_parts,
                type_symbols=type_symbols,
                enum_symbols=enum_symbols,
            )