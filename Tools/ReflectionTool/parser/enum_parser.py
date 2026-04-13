import re

from model import ReflectedEnum

from .namespace_tracker import NamespaceTracker, make_qualified_name
from .patterns import ENUM_PATTERN
from utils import mask_comments_and_strings

def find_body(full_content: str, start_index: int) -> tuple[str, int]:
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


def parse_enum_entries(enum_body: str) -> list[str]:
    clean_body = re.sub(r'//.*', "", enum_body)
    clean_body = re.sub(r'/\*.*?\*/', "", clean_body, flags=re.DOTALL)

    entries: list[str] = []
    for item in [entry.strip() for entry in clean_body.split(",")]:
        if not item:
            continue

        name = item.split("=")[0].strip()
        if name and re.match(r'^\w+$', name):
            entries.append(name)

    return entries


def parse_enums(content: str, source_file: str) -> list[ReflectedEnum]:
    if "ENUM" not in content:
        return []

    tracker = NamespaceTracker(content)
    reflected_enums: list[ReflectedEnum] = []

    for match in ENUM_PATTERN.finditer(content):
        enum_name = match.group(2).strip()
        enum_body, _ = find_body(content, match.end())
        entries = parse_enum_entries(enum_body)

        if not entries:
            continue

        namespace_parts = tracker.get_namespace_at(match.start())
        qualified_name = make_qualified_name(namespace_parts, enum_name)

        reflected_enums.append(
            ReflectedEnum(
                name=enum_name,
                qualified_name=qualified_name,
                namespace_parts=namespace_parts,
                entries=entries,
                source_file=source_file,
            )
        )

    return reflected_enums