import re

from model import ContainerTypeNode, ReflectedProperty, ResolvedTypeRef

from .common import get_property_type, normalize_cpp_type, normalize_type_name


def emitted_leaf_type_name(type_ref: ResolvedTypeRef | None, fallback: str) -> str:
    if type_ref is None:
        return fallback
    if type_ref.resolved_qualified_name:
        return type_ref.resolved_qualified_name
    if type_ref.resolved_name:
        return type_ref.resolved_name
    return fallback


def emitted_container_type_name(node: ContainerTypeNode | None, fallback: str = "void") -> str:
    if node is None:
        return fallback
    if node.emitted_type_name:
        return node.emitted_type_name
    if node.raw_type:
        return node.raw_type
    return fallback


def metadata_span_expr(owner_name: str, prop: ReflectedProperty) -> str:
    if not prop.has_attributes():
        return "std::span<const Engine::MetadataEntry>{}"
    return f"std::span<const Engine::MetadataEntry>{{{owner_name}_{prop.name}_Meta}}"


def emit_metadata_block(owner_name: str, prop: ReflectedProperty) -> str:
    if not prop.has_attributes():
        return ""

    lines: list[str] = []
    lines.append(f"BEGIN_METADATA({owner_name}, {prop.name})")

    for attr in re.split(r",(?![^(]*\))", prop.attributes_raw):
        attr = attr.strip()
        if attr:
            lines.append(f"\t{attr}")

    lines.append("END_METADATA")
    lines.append("")
    return "\n".join(lines)


def container_accessor_expr(node: ContainerTypeNode) -> str:
    if node.container_kind == "Map":
        return f"Engine::MapAccessor<{node.raw_type}>::Get()"

    if node.value_type is not None:
        inner_cpp_type = node.value_type.raw_name
    elif node.inner_container is not None:
        inner_cpp_type = node.inner_container.raw_type
    else:
        inner_cpp_type = "void"

    if node.container_kind == "Set":
        return f"Engine::SetAccessor<{node.raw_type}, {inner_cpp_type}>::Get()"
    if node.container_kind == "List":
        return f"Engine::ListAccessor<{inner_cpp_type}>::Get()"
    return f"Engine::LinearContainerAccessor<{node.raw_type}, {inner_cpp_type}>::Get()"


def emit_container_info_decl(
    owner_name: str,
    prop_name: str,
    node: ContainerTypeNode,
    suffix: str,
    bitflag_enums: set[str],
) -> tuple[list[str], str]:
    lines: list[str] = []
    data_name = f"{owner_name}_{prop_name}_{suffix}_ContainerData"
    accessor_expr = container_accessor_expr(node)

    if node.container_kind == "Map":
        key_type_name = emitted_leaf_type_name(node.key_type, "void")
        key_enum_name = get_property_type(key_type_name, bitflag_enums)[0]

        if node.inner_container is not None:
            child_lines, child_data_name = emit_container_info_decl(
                owner_name=owner_name,
                prop_name=prop_name,
                node=node.inner_container,
                suffix=f"{suffix}_Value",
                bitflag_enums=bitflag_enums,
            )
            lines.extend(child_lines)

            value_type_name = emitted_container_type_name(node.inner_container, "void")
            value_enum_name = get_property_type(value_type_name, bitflag_enums)[0]

            lines.append(
                f'DECLARE_MAP_NESTED_VALUE_INFO({owner_name}, {prop_name}_{suffix}, "{key_type_name}", '
                f'Engine::EPropertyType::{key_enum_name}, "{value_type_name}", '
                f'Engine::EPropertyType::{value_enum_name}, &{child_data_name}, {accessor_expr})'
            )
            return lines, data_name

        value_type_name = emitted_leaf_type_name(node.value_type, "void")
        value_enum_name = get_property_type(value_type_name, bitflag_enums)[0]

        lines.append(
            f'DECLARE_MAP_INFO({owner_name}, {prop_name}_{suffix}, "{key_type_name}", '
            f'Engine::EPropertyType::{key_enum_name}, "{value_type_name}", '
            f'Engine::EPropertyType::{value_enum_name}, {accessor_expr})'
        )
        return lines, data_name

    if node.inner_container is not None:
        child_lines, child_data_name = emit_container_info_decl(
            owner_name=owner_name,
            prop_name=prop_name,
            node=node.inner_container,
            suffix=f"{suffix}_Inner",
            bitflag_enums=bitflag_enums,
        )
        lines.extend(child_lines)

        inner_type_name = emitted_container_type_name(node.inner_container, "void")
        inner_enum_name = get_property_type(inner_type_name, bitflag_enums)[0]

        lines.append(
            f'DECLARE_NESTED_CONTAINER_INFO({owner_name}, {prop_name}_{suffix}, "{inner_type_name}", '
            f'Engine::EPropertyType::{inner_enum_name}, &{child_data_name}, {accessor_expr})'
        )
        return lines, data_name

    inner_type_name = emitted_leaf_type_name(node.value_type, "void")
    inner_enum_name = get_property_type(inner_type_name, bitflag_enums)[0]

    lines.append(
        f'DECLARE_CONTAINER_INFO({owner_name}, {prop_name}_{suffix}, "{inner_type_name}", '
        f'Engine::EPropertyType::{inner_enum_name}, {accessor_expr})'
    )
    return lines, data_name


def emit_property_declarations(
    owner_name: str,
    properties: list[ReflectedProperty],
    bitflag_enums: set[str],
) -> tuple[str, list[str]]:
    declaration_lines: list[str] = []
    property_lines: list[str] = []

    for prop in properties:
        metadata_block = emit_metadata_block(owner_name, prop)
        if metadata_block:
            declaration_lines.append(metadata_block)

        full_type = normalize_cpp_type(prop.cpp_type)
        raw_type = normalize_type_name(full_type)
        metadata_expr = metadata_span_expr(owner_name, prop)

        if prop.is_container() and prop.container_type is not None:
            container_decl_lines, container_data_name = emit_container_info_decl(
                owner_name=owner_name,
                prop_name=prop.name,
                node=prop.container_type,
                suffix="Root",
                bitflag_enums=bitflag_enums,
            )
            declaration_lines.extend(container_decl_lines)

            container_type_name = emitted_container_type_name(prop.container_type, full_type)

            property_lines.append(
                f'REFLECT_CONTAINER_PROPERTY({owner_name}, {prop.name}, "{container_type_name}", '
                f'Engine::EPropertyType::{prop.container_type.container_kind}, &{container_data_name}, {metadata_expr})'
            )
            continue

        property_type_name, reflected_type_name = get_property_type(raw_type, bitflag_enums)

        emitted_type_name = reflected_type_name
        if prop.resolved_type is not None:
            emitted_type_name = emitted_leaf_type_name(prop.resolved_type, reflected_type_name)

        property_lines.append(
            f'REFLECT_PROPERTY({owner_name}, {prop.name}, "{emitted_type_name}", '
            f'Engine::EPropertyType::{property_type_name}, {metadata_expr})'
        )

    declaration_code = ""
    if declaration_lines:
        declaration_code = "\n".join(declaration_lines) + "\n"

    return declaration_code, property_lines


def emit_properties_block(
    owner_name: str,
    properties: list[ReflectedProperty],
    bitflag_enums: set[str],
) -> str:
    declaration_code, property_lines = emit_property_declarations(owner_name, properties, bitflag_enums)

    lines: list[str] = []
    if declaration_code:
        lines.append(declaration_code.rstrip())

    if not property_lines:
        lines.append(f"EMPTY_PROPERTIES({owner_name})")
        lines.append("")
        return "\n".join(lines)

    lines.append(f"BEGIN_PROPERTIES({owner_name})")
    for property_line in property_lines:
        lines.append(f"\t{property_line}")
    lines.append("END_PROPERTIES")
    lines.append("")
    return "\n".join(lines)