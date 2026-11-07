import os
import sys

from emitter import emit_generated_file
from parser import parse_enums, parse_types
from utils import collect_bitflag_enums, collect_header_files, write_text_file
from validation import validate_reflected_symbols
from parser.property_parser import resolve_property_types


ENGINE_SOURCE_DIR = "Engine/Source"
OUTPUT_FILE = os.path.abspath("Engine/Source/Core/Reflection/Private/Reflection.gen.cpp")


def generate_reflection_code() -> int:
    print(f"[Reflection] Scanning source root: {ENGINE_SOURCE_DIR}")

    header_contents = collect_header_files(ENGINE_SOURCE_DIR)
    bitflag_enums: set[str] = set()

    for _relative_path, content in header_contents:
        bitflag_enums.update(collect_bitflag_enums(content))

    reflected_enums = []
    reflected_types = []
    included_headers: set[str] = set()

    for relative_path, content in header_contents:
        file_enums = parse_enums(content, relative_path)
        file_types = parse_types(content, relative_path)

        if file_enums or file_types:
            included_headers.add(relative_path)

        reflected_enums.extend(file_enums)
        reflected_types.extend(file_types)

    validate_reflected_symbols(reflected_enums, reflected_types)

    resolve_property_types(
        reflected_enums=reflected_enums,
        reflected_types=reflected_types,
    )

    generated_code = emit_generated_file(
        included_headers=sorted(included_headers),
        reflected_enums=reflected_enums,
        reflected_types=reflected_types,
        bitflag_enums=bitflag_enums,
    )

    write_text_file(OUTPUT_FILE, generated_code, encoding="utf-8")

    print(
        f"[Reflection] Success: "
        f"{len(reflected_enums)} enums, {len(reflected_types)} types -> {OUTPUT_FILE}"
    )
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(generate_reflection_code())
    except Exception as exception:
        print(f"[Reflection] Error: {exception}")
        raise SystemExit(1)