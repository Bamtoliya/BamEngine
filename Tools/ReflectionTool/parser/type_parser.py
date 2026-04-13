import re

from model import ReflectedType

from .function_parser import parse_functions
from .namespace_tracker import NamespaceTracker, make_qualified_name
from .patterns import CLASS_STRUCT_PATTERN
from .property_parser import parse_properties
from utils import mask_comments_and_strings


def normalize_type_name(type_name: str) -> str:
    return type_name.replace("std::", "").replace("Engine::", "").strip()


def resolve_parent_qualified_name(current_namespace: list[str], parent_name: str) -> str:
    normalized_parent = normalize_type_name(parent_name)
    if not normalized_parent or normalized_parent == "None":
        return ""

    if "::" in normalized_parent:
        return normalized_parent

    if not current_namespace:
        return normalized_parent

    return "::".join(current_namespace + [normalized_parent])


def find_class_body(full_content: str, start_index: int) -> tuple[str, int]:
    masked = mask_comments_and_strings(full_content)
    open_brace_index = masked.find("{", start_index)
    if open_brace_index == -1:
        return "", start_index

    depth = 0
    for index in range(open_brace_index, len(masked)):
        if masked[index] == "{":
            depth += 1
        elif masked[index] == "}":
            depth -= 1
            if depth == 0:
                return full_content[open_brace_index + 1:index], open_brace_index

    return "", start_index


def find_parent_class(full_content: str, class_name: str, search_end_index: int) -> str:
    search_start = max(0, search_end_index - 300)
    pre_text = mask_comments_and_strings(full_content[search_start:search_end_index])

    pattern = (
        r'(class|struct)\s+(?:[A-Z0-9_]+\s+)*'
        + re.escape(class_name)
        + r'\s*(?:final|abstract)?\s*:\s*public\s+([A-Za-z_]\w*(?:::\w+)*)'
    )

    match = re.search(pattern, pre_text)
    if not match:
        return ""

    return match.group(2).strip()


def has_reflection_marker(scope_content: str) -> bool:
    return any(
        marker in scope_content
        for marker in ("REFLECT_CLASS", "REFLECT_STRUCT", "REFLECT_BASE")
    )


def parse_types(content: str, source_file: str) -> list[ReflectedType]:
    if not any(token in content for token in ("CLASS", "STRUCT", "PROPERTY", "FUNCTION")):
        return []

    tracker = NamespaceTracker(content)
    reflected_types: list[ReflectedType] = []

    for match in CLASS_STRUCT_PATTERN.finditer(content):
        kind = match.group(1).strip()
        type_name = match.group(2).strip()

        scope_content, brace_start_index = find_class_body(content, match.end())
        if not scope_content:
            continue

        if not has_reflection_marker(scope_content):
            continue

        namespace_parts = tracker.get_namespace_at(match.start())
        qualified_name = make_qualified_name(namespace_parts, type_name)

        parent_name = find_parent_class(content, type_name, brace_start_index)
        parent_qualified_name = resolve_parent_qualified_name(namespace_parts, parent_name)

        reflected_type = ReflectedType(
            kind=kind,
            name=type_name,
            qualified_name=qualified_name,
            namespace_parts=namespace_parts,
            parent_name=parent_name,
            parent_qualified_name=parent_qualified_name,
            source_file=source_file,
        )

        for prop in parse_properties(scope_content):
            reflected_type.add_property(prop)

        for func in parse_functions(
            scope_content=scope_content,
            owner_name=type_name,
            owner_qualified_name=qualified_name,
        ):
            reflected_type.add_function(func)

        reflected_types.append(reflected_type)

    return reflected_types