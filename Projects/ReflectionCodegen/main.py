import argparse
import os
import sys

from config.job_schema import ReflectionCodegenProfile, ReflectionJob, set_active_profile
from emitter import emit_generated_file
from parser import parse_enums, parse_types
from parser.property_parser import resolve_property_types
from utils import collect_bitflag_enums, collect_header_files, write_text_file
from validation import validate_reflected_symbols


def parse_args(argv: list[str]) -> ReflectionJob:
    parser = argparse.ArgumentParser(
        prog="ReflectionCodegen",
        description="Generate reflection registration code for a source module.",
    )

    parser.add_argument(
        "--module",
        required=True,
        help="Logical module name. Example: Engine, Editor, Client",
    )
    parser.add_argument(
        "--source-root",
        required=True,
        help="Root directory to scan for reflected headers.",
    )
    parser.add_argument(
        "--output",
        required=True,
        help="Output .gen.cpp file path.",
    )
    parser.add_argument(
        "--working-directory",
        default="",
        help="Optional working directory used to resolve relative paths.",
    )

    parser.add_argument(
        "--strip-namespace",
        action="append",
        default=[],
        help="Namespace prefix to strip during loose type normalization. Repeatable.",
    )
    parser.add_argument(
        "--namespace-fallback",
        action="append",
        default=[],
        help="Namespace fallback to try when resolving unqualified symbols. Repeatable.",
    )
    parser.add_argument(
        "--resource-handle-template",
        action="append",
        default=[],
        help="Template name treated as a resource handle property kind. Repeatable.",
    )
    parser.add_argument(
        "--ignore-header",
        action="append",
        default=[],
        help="Header filename to skip while scanning. Repeatable.",
    )

    args = parser.parse_args(argv)

    working_directory = args.working_directory or os.getcwd()

    source_root = args.source_root
    if not os.path.isabs(source_root):
        source_root = os.path.abspath(os.path.join(working_directory, source_root))
    else:
        source_root = os.path.abspath(source_root)

    output_file = args.output
    if not os.path.isabs(output_file):
        output_file = os.path.abspath(os.path.join(working_directory, output_file))
    else:
        output_file = os.path.abspath(output_file)

    profile = ReflectionCodegenProfile().extend(
        stripped_namespaces=args.strip_namespace,
        namespace_fallbacks=args.namespace_fallback,
        resource_handle_templates=args.resource_handle_template,
        ignored_headers=args.ignore_header,
    )

    return ReflectionJob(
        module_name=args.module,
        source_root=source_root,
        output_file=output_file,
        working_directory=os.path.abspath(working_directory),
        profile=profile,
    )


def generate_reflection_code(job: ReflectionJob) -> int:
    set_active_profile(job.profile)

    print(f"[Reflection] Module: {job.module_name}")
    print(f"[Reflection] Scanning source root: {job.source_root}")
    print(f"[Reflection] Output file: {job.output_file}")
    print(
        "[Reflection] Profile: "
        f"strip={list(job.profile.stripped_namespaces)}, "
        f"fallback={list(job.profile.namespace_fallbacks)}, "
        f"resource_handles={list(job.profile.resource_handle_templates)}"
    )

    if not os.path.isdir(job.source_root):
        raise FileNotFoundError(f"Source root does not exist: {job.source_root}")

    header_contents = collect_header_files(job.source_root)
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

    write_text_file(job.output_file, generated_code, encoding="utf-8")

    print(
        f"[Reflection] Success: "
        f"{len(reflected_enums)} enums, {len(reflected_types)} types -> {job.output_file}"
    )
    return 0


def main(argv: list[str] | None = None) -> int:
    job = parse_args(argv if argv is not None else sys.argv[1:])
    return generate_reflection_code(job)


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exception:
        print(f"[Reflection] Error: {exception}")
        raise SystemExit(1)