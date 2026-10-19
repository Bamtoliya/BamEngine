#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
audit_localization.py
BamEngine 다국어(Localization) 및 리플렉션 키 정적 감사 도구

검사 기능:
1. 누락 키 (Missing Keys): 코드에서 LOCAL(...) / DISPLAY_NAME 등으로 불렀으나 CSV에 없는 키
2. 미번역 항목 (Untranslated): CSV에 키는 있으나 English 또는 Korean이 비어있는 행
3. 하드코딩된 UI (Hardcoded UI): Engine·Editor의 ImGui 표시 문자열과 알림 제목·본문
4. 고아 키 (Orphan Keys, 옵션): CSV에 존재하지만 소스코드 어디에서도 사용되지 않는 키
"""

import os
import sys
import re
import csv
import argparse
from itertools import chain
from pathlib import Path

# 정규식 패턴 정의
RE_LOCAL = re.compile(r'(?<![:\w])(LOCAL|LOCAL_CSTR|LOCAL_LABEL|LOCAL_FMT|LocalizeInspectorText|GetText|Text)\s*\(\s*"([^"]+)"')
RE_REFL = re.compile(r'\b(DISPLAY_NAME|CATEGORY|TOOLTIP)\s*\(\s*"([^"]+)"')
RE_CPP_TOKEN = re.compile(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|//[^\n]*|/\*[\s\S]*?\*/')
RE_IMGUI = re.compile(r'\bImGui::(Text\w*|BulletText|Button|SmallButton|Checkbox|MenuItem|BeginMenu|'
    r'Begin|BeginPopupModal|BeginCombo|CollapsingHeader|Selectable|RadioButton|TableSetupColumn|'
    r'Slider\w*|Drag(?:Float|Int|Scalar)\w*|Input\w*|ColorEdit\w*|Set(?:Item)?Tooltip)\s*\(')
RE_PRINTF = re.compile(r'%(?:[-+ #0]*\d*(?:\.\d+)?(?:hh|ll|[hlzjtL])?[a-zA-Z]|%)')
RE_NOTIFICATION = re.compile(r'\b(AddNotification)\s*\(')


def strip_comments(source: str) -> str:
    return RE_CPP_TOKEN.sub(lambda m: re.sub(r'[^\n]', ' ', m[0]) if m[0].startswith(('//', '/*')) else m[0], source)


def call_arguments(source: str, start: int):
    """Read arguments after an opening parenthesis, including multiline/nested expressions."""
    args, depth, begin, pos = [], 0, start, start
    while pos < len(source):
        token = RE_CPP_TOKEN.match(source, pos)
        if token:
            pos = token.end()
            continue
        char = source[pos]
        if char in '([{':
            depth += 1
        elif char == ')' and depth == 0:
            args.append(source[begin:pos])
            return args
        elif char in ')]}':
            depth -= 1
        elif char == ',' and depth == 0:
            args.append(source[begin:pos])
            begin = pos + 1
        pos += 1
    return args


def find_hardcoded_ui(source: str):
    source = strip_comments(source)
    for match in chain(RE_IMGUI.finditer(source), RE_NOTIFICATION.finditer(source)):
        name = match[1]
        args = call_arguments(source, match.end())
        indices = (0, 1) if name == 'AddNotification' else (1,) if name == 'TextColored' else (0,)
        for index in indices:
            if len(args) > index:
                for text in hardcoded_literals(args[index]):
                    yield source.count('\n', 0, match.start()) + 1, name, text


def hardcoded_literals(expression: str):
    # Remove localized subexpressions, but still inspect other ternary branches.
    localized = re.compile(r'\b(?:LOCAL\w*|LocalizeInspectorText|GetText|Text)\s*\(')
    while wrapped := localized.search(expression):
        nested = call_arguments(expression, wrapped.end())
        length = sum(len(arg) for arg in nested) + max(len(nested) - 1, 0) + 1
        expression = expression[:wrapped.start()] + expression[wrapped.end() + length:]
    for literal in re.finditer(r'"((?:\\.|[^"\\])*)"', expression):
        text = literal[1]
        if not is_ignored_imgui_literal(text):
            yield text

# ImGui 호출 중 하드코딩 검사에서 제외할 특수 문자열들
IGNORED_IMGUI_STRINGS = {
    "", "##", "%s", "##Value", "##Shape", "##Column", "##Clear", "X", "2D", "3D", "100%", ">",
    "---", ": ", ", ", "/", "+", "-", "?", ".", "...", "...###", "BamEngine", "Y", "Z", "ms"
}

def is_ignored_imgui_literal(text: str) -> bool:
    s = re.sub(r'\\[nrt]', '', text.split('##', 1)[0]).strip()
    if not s or s in IGNORED_IMGUI_STRINGS:
        return True
    if s.startswith("##") or s.startswith("###"):
        return True
    if not re.search(r'[a-zA-Z가-힣]', RE_PRINTF.sub('', s)):
        return True
    if s.startswith("ID: %"):
        return True
    if RE_PRINTF.sub('', s).strip() in IGNORED_IMGUI_STRINGS:
        return True
    if s.startswith("ICON_"):  # FontAwesome 매크로
        return True
    return False

def load_csv_catalog(file_path: Path):
    catalog = {}
    empty_entries = []

    if not file_path.is_file():
        return catalog, empty_entries

    with open(file_path, "r", encoding="utf-8-sig") as f:
        reader = csv.reader(f)
        header = next(reader, None)
        group = ""
        for line_no, row in enumerate(reader, start=2):
            if not row or len(row) < 2:
                continue
            group = row[0].strip() or group
            key = row[1].strip()
            en = row[2].strip() if len(row) > 2 else ""
            ko = row[3].strip() if len(row) > 3 else ""

            if not key:
                continue

            full_key = f"{group}.{key}" if group else key
            catalog[full_key] = (en, ko, file_path.name, line_no)

            if not en or not ko:
                empty_entries.append((file_path.name, line_no, full_key, en, ko))

    return catalog, empty_entries

def is_dynamic_prefix_matched(prefix: str, catalog: dict) -> bool:
    """Console.Level. 처럼 동적으로 런타임에 접미사가 붙는 키 접두사 검증"""
    if not prefix.endswith("."):
        return False
    for k in catalog.keys():
        if k.startswith(prefix) and len(k) > len(prefix):
            return True
    return False

def audit_project(root_dir: Path, check_orphans: bool = False, strict: bool = False):
    loc_dir = root_dir / "Resources" / "Data" / "Localization"
    engine_csv = loc_dir / "Engine.csv"
    editor_csv = loc_dir / "Editor.csv"

    cat_engine, empty_engine = load_csv_catalog(engine_csv)
    cat_editor, empty_editor = load_csv_catalog(editor_csv)

    combined_catalog = {**cat_engine, **cat_editor}
    catalog_groups = {key.split('.', 1)[0] for key in combined_catalog if '.' in key}
    all_empty = empty_engine + empty_editor

    missing_keys = []
    hardcoded_ui = []
    referenced_keys = set()

    scan_dirs = [
        root_dir / "Editor" / "Source",
        root_dir / "Engine" / "Source"
    ]

    scanned_file_count = 0

    for s_dir in scan_dirs:
        if not s_dir.exists():
            continue
        for root, _, files in os.walk(s_dir):
            for file in files:
                if not file.endswith((".h", ".cpp", ".inl")):
                    continue
                scanned_file_count += 1
                file_path = Path(root) / file
                rel_path = file_path.relative_to(root_dir)

                try:
                    with open(file_path, "r", encoding="utf-8", errors="ignore") as f:
                        source = strip_comments(f.read())
                        lines = source.splitlines()
                except Exception:
                    continue

                for line_idx, line in enumerate(lines, start=1):
                    # 1. LOCAL(...) 매크로 키 검사
                    for match in RE_LOCAL.finditer(line):
                        macro_fn = match.group(1)
                        key = match.group(2).strip()
                        referenced_keys.add(key)
                        if key not in combined_catalog:
                            if not is_dynamic_prefix_matched(key, combined_catalog):
                                missing_keys.append((rel_path, line_idx, macro_fn, key))

                    # 2. DISPLAY_NAME/CATEGORY 리플렉션 키 검사
                    for match in RE_REFL.finditer(line):
                        macro_fn = match.group(1)
                        key = match.group(2).strip()
                        referenced_keys.add(key)
                        if key not in combined_catalog:
                            missing_keys.append((rel_path, line_idx, macro_fn, key))

                # Engine UI must follow the same catalog policy as Editor UI.
                for line_idx, fn_name, lit_text in find_hardcoded_ui(source):
                    hardcoded_ui.append((rel_path, line_idx, fn_name, lit_text))

                # Keys also arrive through arrays, display-name fields and graph diagnostics.
                for token in RE_CPP_TOKEN.finditer(source):
                    if not token[0].startswith('"'):
                        continue
                    key = token[0][1:-1].strip()
                    indirect_key = key.startswith('UI_') or ('Editor' in rel_path.parts and bool(
                        re.fullmatch(r'\w+\.[A-Z]\w*', key)) and key.split('.', 1)[0] in catalog_groups)
                    graph_message = 'Graph' in rel_path.parts and 'Engine' in rel_path.parts and bool(
                        re.match(r'[A-Z][A-Za-z0-9_]*(?:\s|\()', key))
                    if not (indirect_key or graph_message) or key in referenced_keys:
                        continue
                    referenced_keys.add(key)
                    if key not in combined_catalog and not is_dynamic_prefix_matched(key, combined_catalog):
                        missing_keys.append((rel_path, source.count('\n', 0, token.start()) + 1, 'Indirect', key))

    # 결과 리포트 출력
    print("=" * 80)
    print("             BAMENGINE LOCALIZATION AUDIT REPORT")
    print("=" * 80)
    print(f"* Scanned Files : {scanned_file_count} files")
    print(f"* Catalog Keys  : {len(combined_catalog)} keys")
    print()

    # 1. 누락 키 (Missing Keys)
    if missing_keys:
        print(f"[!] MISSING KEYS IN CSV ({len(missing_keys)} issues):")
        print("    Code calls a localization key that does not exist in CSV catalogs:")
        for fpath, lno, macro, key in missing_keys:
            print(f"    - {fpath}:{lno} [{macro}] -> '{key}'")
        print()
    else:
        print("[V] Missing Keys in CSV: None (All code keys exist in catalogs)")

    # 2. 미번역 항목 (Empty Translation in CSV)
    if all_empty:
        print(f"[!] UNTRANSLATED CSV ENTRIES ({len(all_empty)} issues):")
        print("    Keys exist in CSV but English or Korean column is empty:")
        for csv_file, lno, key, en, ko in all_empty:
            missing_lang = []
            if not en: missing_lang.append("English")
            if not ko: missing_lang.append("Korean")
            print(f"    - {csv_file}:{lno} -> '{key}' (Missing: {', '.join(missing_lang)})")
        print()
    else:
        print("[V] Untranslated CSV Entries: None (All CSV rows are fully translated)")

    # 3. 하드코딩 의심 UI 문자열 (Bypassed LOCAL in Editor)
    if hardcoded_ui:
        print(f"[?] HARDCODED UI STRINGS DETECTED ({len(hardcoded_ui)} locations):")
        print("    Consider wrapping with LOCAL_LABEL or LOCAL_CSTR:")
        for fpath, lno, fn, text in hardcoded_ui:
            call = fn if fn == 'AddNotification' else f'ImGui::{fn}'
            print(f"    - {fpath}:{lno} [{call}] -> \"{text}\"")
        print()
    else:
        print("[V] Hardcoded UI Strings: None in checked UI calls")

    # 4. 고아 키 검사 (옵션)
    if check_orphans:
        orphan_keys = [k for k in combined_catalog.keys() if k not in referenced_keys and "." not in k]
        if orphan_keys:
            print(f"[i] POTENTIAL ORPHAN KEYS IN CSV ({len(orphan_keys)} keys):")
            for k in orphan_keys[:20]:
                print(f"    - '{k}'")
            if len(orphan_keys) > 20:
                print(f"    ... and {len(orphan_keys) - 20} more")
            print()

    print("=" * 80)
    if strict:
        return 1 if (missing_keys or all_empty or hardcoded_ui) else 0
    return 1 if (missing_keys or all_empty) else 0

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Audit BamEngine Localization")
    parser.add_argument("--root", type=str, default=".", help="Project root directory")
    parser.add_argument("--orphans", action="store_true", help="Check for unused orphan keys in CSV")
    parser.add_argument("--strict", action="store_true", help="Fail if any hardcoded string is found")
    args = parser.parse_args()

    project_root = Path(args.root).resolve()
    exit_code = audit_project(project_root, check_orphans=args.orphans, strict=args.strict)
    sys.exit(exit_code)
