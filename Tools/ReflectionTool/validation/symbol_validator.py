from collections import defaultdict

from model import ReflectedEnum, ReflectedType


class ReflectionValidationError(Exception):
    pass


def _build_duplicate_map(values: list[str]) -> dict[str, list[int]]:
    index_map: dict[str, list[int]] = defaultdict(list)

    for index, value in enumerate(values):
        index_map[value].append(index)

    return {
        value: indices
        for value, indices in index_map.items()
        if len(indices) > 1
    }


def _format_sources(items: list[object], indices: list[int]) -> str:
    sources: list[str] = []

    for index in indices:
        item = items[index]
        source_file = getattr(item, "source_file", "")
        qualified_name = getattr(item, "qualified_name", "")
        sources.append(f"{qualified_name} ({source_file})")

    return ", ".join(sources)


def validate_duplicate_enum_qualified_names(reflected_enums: list[ReflectedEnum]) -> None:
    qualified_names = [item.qualified_name for item in reflected_enums]
    duplicates = _build_duplicate_map(qualified_names)

    if not duplicates:
        return

    messages: list[str] = []
    for qualified_name, indices in duplicates.items():
        messages.append(
            f"Duplicate enum qualified name '{qualified_name}': "
            f"{_format_sources(reflected_enums, indices)}"
        )

    raise ReflectionValidationError("\n".join(messages))


def validate_duplicate_type_qualified_names(reflected_types: list[ReflectedType]) -> None:
    qualified_names = [item.qualified_name for item in reflected_types]
    duplicates = _build_duplicate_map(qualified_names)

    if not duplicates:
        return

    messages: list[str] = []
    for qualified_name, indices in duplicates.items():
        messages.append(
            f"Duplicate type qualified name '{qualified_name}': "
            f"{_format_sources(reflected_types, indices)}"
        )

    raise ReflectionValidationError("\n".join(messages))


def validate_short_name_collisions(
    reflected_enums: list[ReflectedEnum],
    reflected_types: list[ReflectedType],
) -> None:
    short_name_map: dict[str, list[str]] = defaultdict(list)

    for reflected_enum in reflected_enums:
        short_name_map[reflected_enum.short_name].append(reflected_enum.qualified_name)

    for reflected_type in reflected_types:
        short_name_map[reflected_type.short_name].append(reflected_type.qualified_name)

    collisions: dict[str, list[str]] = {}

    for short_name, qualified_names in short_name_map.items():
        unique_names = sorted(set(qualified_names))
        if len(unique_names) > 1:
            collisions[short_name] = unique_names

    if not collisions:
        return

    messages: list[str] = []
    for short_name, qualified_names in sorted(collisions.items()):
        messages.append(
            f"Ambiguous short name '{short_name}': {', '.join(qualified_names)}"
        )

    raise ReflectionValidationError("\n".join(messages))


def validate_parent_links(reflected_types: list[ReflectedType]) -> None:
    known_types = {reflected_type.qualified_name for reflected_type in reflected_types}
    warnings: list[str] = []
    errors: list[str] = []

    for reflected_type in reflected_types:
        if not reflected_type.parent_qualified_name:
            continue

        if reflected_type.parent_qualified_name == reflected_type.qualified_name:
            errors.append(
                f"Type '{reflected_type.qualified_name}' cannot inherit from itself."
            )
            continue

        if reflected_type.parent_qualified_name not in known_types:
            warnings.append(
                f"[Reflection][Warn] Type '{reflected_type.qualified_name}' has unresolved parent "
                f"'{reflected_type.parent_qualified_name}'. Assuming external/non-reflected base."
            )

    for warning in warnings:
        print(warning)

    if errors:
        raise ReflectionValidationError("\n".join(errors))


def validate_empty_qualified_names(
    reflected_enums: list[ReflectedEnum],
    reflected_types: list[ReflectedType],
) -> None:
    messages: list[str] = []

    for reflected_enum in reflected_enums:
        if not reflected_enum.qualified_name.strip():
            messages.append(
                f"Enum '{reflected_enum.name}' from '{reflected_enum.source_file}' "
                f"has an empty qualified name."
            )

    for reflected_type in reflected_types:
        if not reflected_type.qualified_name.strip():
            messages.append(
                f"Type '{reflected_type.name}' from '{reflected_type.source_file}' "
                f"has an empty qualified name."
            )

    if messages:
        raise ReflectionValidationError("\n".join(messages))


def validate_reflected_symbols(
    reflected_enums: list[ReflectedEnum],
    reflected_types: list[ReflectedType],
) -> None:
    validate_empty_qualified_names(reflected_enums, reflected_types)
    validate_duplicate_enum_qualified_names(reflected_enums)
    validate_duplicate_type_qualified_names(reflected_types)
    validate_short_name_collisions(reflected_enums, reflected_types)
    validate_parent_links(reflected_types)