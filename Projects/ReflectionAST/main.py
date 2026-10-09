import argparse
import os
import time

import clang.cindex

from parser import (
    init_libclang,
    collect_type_markers,
    parse_node,
    group_header_roots,
)
from generator import generate_cpp_code


def collect_candidate_headers(source_root):
    headers = []

    for root, directories, files in os.walk(source_root):
        directories.sort()

        for filename in sorted(files):
            if not filename.endswith(".h"):
                continue

            filepath = os.path.abspath(os.path.join(root, filename))

            with open(
                filepath,
                "r",
                encoding="utf-8-sig",
                errors="ignore",
            ) as source_file:
                original_code = source_file.read()

            # 이번 단계에서는 기존 후보 선정 기준을 유지합니다.
            if not any(
                marker in original_code
                for marker in ("CLASS", "STRUCT", "ENUM")
            ):
                continue

            relative_path = os.path.relpath(
                filepath, source_root
            ).replace("\\", "/")

            headers.append(
                (filepath, relative_path, original_code)
            )

    headers.sort(key=lambda header: header[1])
    return headers


def check_diagnostics(translation_unit):
    has_error = False
    printed_warnings = set()

    for diagnostic in translation_unit.diagnostics:
        if diagnostic.severity >= clang.cindex.Diagnostic.Error:
            print(f"[Clang] {diagnostic}", flush=True)
            has_error = True

        elif diagnostic.severity == clang.cindex.Diagnostic.Warning:
            location = diagnostic.location

            path = (
                os.path.normcase(
                    os.path.abspath(location.file.name)
                )
                if location.file
                else ""
            )

            key = (
                path,
                location.line,
                location.column,
                diagnostic.spelling,
            )

            if key not in printed_warnings:
                printed_warnings.add(key)
                print(f"[Clang] {diagnostic}", flush=True)

    return has_error


def main():
    argument_parser = argparse.ArgumentParser(
        description="Generate reflection data via batched Clang AST"
    )

    argument_parser.add_argument(
        "--source-root",
        default=r"I:\BamtoliyaGithub\BamEngine\Engine\Source",
    )
    argument_parser.add_argument(
        "--output",
        default=(
            r"I:\BamtoliyaGithub\BamEngine\Engine\Source"
            r"\Core\Reflection\Private\Reflection.gen.cpp"
        ),
    )
    argument_parser.add_argument("--module")
    argument_parser.add_argument("--include-dirs-file")

    # 기존 CMake 호출과의 호환성을 위해 유지합니다.
    argument_parser.add_argument(
        "--strip-namespace", action="append"
    )
    argument_parser.add_argument(
        "--namespace-fallback", action="append"
    )
    argument_parser.add_argument("--resource-handle-template")

    arguments = argument_parser.parse_args()

    module = arguments.module or "Engine"
    source_root = os.path.abspath(arguments.source_root)
    output_path = os.path.abspath(arguments.output)
    overall_started = time.perf_counter()

    extra_include_directories = []

    if arguments.include_dirs_file:
        with open(
            arguments.include_dirs_file,
            "r",
            encoding="utf-8-sig",
        ) as include_file:
            extra_include_directories = [
                line.strip()
                for line in include_file
                if line.strip()
            ]

    scan_started = time.perf_counter()
    headers = collect_candidate_headers(source_root)

    print(
        f"[Reflection:{module}] 대상 헤더 {len(headers)}개 | "
        f"탐색 {time.perf_counter() - scan_started:.2f}s",
        flush=True,
    )

    parsed_classes = []
    parsed_enums = []
    included_headers = {
        relative_path
        for _, relative_path, _ in headers
    }

    if headers:
        init_libclang()
        index = clang.cindex.Index.create()

        include_arguments = [
            "-xc++",
            "-std=c++23",
            "-DBAM_REFLECTION_PARSER=1",
            "-DBAM_PLATFORM_WINDOWS",
            "-D_CRT_SECURE_NO_WARNINGS",
            "-Wno-pragma-once-outside-header",
            f"-I{source_root}",
        ]

        include_arguments.extend(
            f"-I{directory}"
            for directory in dict.fromkeys(
                extra_include_directories
            )
        )

        # 생성된 C++와 같은 공통 헤더를 먼저 포함합니다.
        batch_source = '#include "Engine_Includes.h"\n'

        for filepath, _, _ in headers:
            include_path = filepath.replace("\\", "/")
            batch_source += f'#include "{include_path}"\n'

        # Clang에 전달할 가상 파일의 이름입니다.
        # 해당 경로에 파일을 생성하지 않습니다.
        virtual_path = output_path + ".batch-input.cpp"

        parse_started = time.perf_counter()

        print(
            f"[Reflection:{module}] 묶음 AST 파싱 시작",
            flush=True,
        )

        try:
            translation_unit = index.parse(
                virtual_path,
                args=include_arguments,
                unsaved_files=[
                    (virtual_path, batch_source)
                ],
                options=(
                    clang.cindex.TranslationUnit
                    .PARSE_DETAILED_PROCESSING_RECORD
                    | clang.cindex.TranslationUnit
                    .PARSE_SKIP_FUNCTION_BODIES
                ),
            )
        except clang.cindex.TranslationUnitLoadError as error:
            print(
                f"[실패] 묶음 AST를 생성하지 못했습니다: {error}",
                flush=True,
            )
            print("기존 생성 파일은 변경하지 않았습니다.")
            raise SystemExit(1)

        print(
            f"[Reflection:{module}] 묶음 AST 파싱 종료 | "
            f"{time.perf_counter() - parse_started:.2f}s",
            flush=True,
        )

        if check_diagnostics(translation_unit):
            print("[실패] Clang 파싱 오류가 발생했습니다.")
            print("기존 생성 파일은 변경하지 않았습니다.")
            raise SystemExit(1)

        extraction_started = time.perf_counter()
        failed_headers = []

        grouping_started = time.perf_counter()

        roots_by_file = group_header_roots(
            translation_unit,
            [filepath for filepath, _, _ in headers],
        )

        print(
            f"[Reflection:{module}] AST 파일별 분류 | "
            f"{time.perf_counter() - grouping_started:.2f}s",
            flush=True,
        )

        for current, header in enumerate(headers, start=1):
            filepath, relative_path, original_code = header
            header_started = time.perf_counter()

            try:
                header_roots = roots_by_file[
                    os.path.normcase(os.path.abspath(filepath))
                ]

                target_classes, target_enums = (
                    collect_type_markers(
                        translation_unit, filepath, root_nodes=header_roots,
                    )
                )

                header_classes = []
                header_enums = []

                if target_classes or target_enums:
                    for root_node in header_roots:
                        parse_node(
                            root_node,
                            target_classes,
                            target_enums,
                            header_classes,
                            header_enums,
                            filepath,
                            relative_path,
                            original_code,
                        )

                if target_classes or target_enums:
                    raise RuntimeError(
                        "일부 타입 매크로를 AST 선언으로 "
                        "변환하지 못했습니다."
                    )

                parsed_classes.extend(header_classes)
                parsed_enums.extend(header_enums)

                print(
                    f"[Reflection:{module}] "
                    f"[{current}/{len(headers)}] "
                    f"{relative_path} | "
                    f"클래스 {len(header_classes)}, "
                    f"열거형 {len(header_enums)} | "
                    f"추출 "
                    f"{time.perf_counter() - header_started:.2f}s",
                    flush=True,
                )

            except RuntimeError as error:
                failed_headers.append(relative_path)
                print(
                    f"[AST 실패] {relative_path}: {error}",
                    flush=True,
                )

        print(
            f"[Reflection:{module}] 전체 정보 추출 | "
            f"{time.perf_counter() - extraction_started:.2f}s",
            flush=True,
        )

        if failed_headers:
            print("[실패] 다음 헤더의 정보 추출에 실패했습니다.")

            for header in failed_headers:
                print(f"  - {header}")

            print("기존 생성 파일은 변경하지 않았습니다.")
            raise SystemExit(1)

    generation_started = time.perf_counter()

    try:
        generated_code = generate_cpp_code(
            parsed_classes,
            parsed_enums,
            included_headers,
        )
    except (RuntimeError, ValueError) as error:
        print(f"[생성 실패] {error}", flush=True)
        print("기존 생성 파일은 변경하지 않았습니다.")
        raise SystemExit(1)

    os.makedirs(os.path.dirname(output_path), exist_ok=True)

    with open(output_path, "w", encoding="utf-8") as output_file:
        output_file.write(generated_code)

    print(
        f"[Reflection:{module}] 코드 생성 및 저장 | "
        f"{time.perf_counter() - generation_started:.2f}s",
        flush=True,
    )
    print(
        f"[성공:{module}] 클래스 {len(parsed_classes)}개, "
        f"열거형 {len(parsed_enums)}개 | "
        f"전체 {time.perf_counter() - overall_started:.2f}s",
        flush=True,
    )


if __name__ == "__main__":
    main()