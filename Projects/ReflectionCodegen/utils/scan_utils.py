import os
import re

from .file_utils import read_file_content


BITMASK_ENUM_PATTERN = re.compile(
    r"ENABLE_BITMASK_OPERATORS\s*\(\s*([a-zA-Z_]\w*(?:::\w+)*)\s*\)"
)


def normalize_type_name(type_name: str) -> str:
    return type_name.replace("std::", "").replace("Engine::", "").strip()


def get_unqualified_name(type_name: str) -> str:
    normalized = normalize_type_name(type_name)
    return normalized.split("::")[-1]


def collect_bitflag_enums(content: str) -> set[str]:
    result: set[str] = set()

    for match in BITMASK_ENUM_PATTERN.finditer(content):
        full_name = match.group(1).strip()
        result.add(normalize_type_name(full_name))
        result.add(get_unqualified_name(full_name))

    return result


def collect_header_files(source_root: str) -> list[tuple[str, str]]:
    header_contents: list[tuple[str, str]] = []

    for root, _dirs, files in os.walk(source_root):
        for file_name in files:
            if not file_name.endswith(".h"):
                continue
            if file_name == "ReflectionMacro.h":
                continue

            filepath = os.path.join(root, file_name)
            content = read_file_content(filepath)
            if not content:
                continue

            relative_path = os.path.relpath(filepath, source_root).replace("\\", "/")
            header_contents.append((relative_path, content))

    header_contents.sort(key=lambda item: item[0])
    return header_contents