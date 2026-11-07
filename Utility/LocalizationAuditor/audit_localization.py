#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
audit_localization.py
BamEngine 다국어(Localization) 및 리플렉션 키 정적 감사 도구

검사 기능:
1. 누락 키 (Missing Keys): 코드에서 LOCAL(...) / DISPLAY_NAME 등으로 불렀으나 CSV에 없는 키
2. 미번역 항목 (Untranslated): CSV에 키는 있으나 English 또는 Korean이 비어있는 행
3. 하드코딩된 UI (Hardcoded UI): 자체 소스의 ImGui 표시 문자열·단축키 힌트와 알림 제목·본문
4. 고아 키 (Orphan Keys, 옵션): CSV에 존재하지만 소스코드 어디에서도 사용되지 않는 키
5. 중복 키: 모든 CSV 원본에서 중복된 키와 전체 위치
6. 미등록 액션: 단축키 힌트·명령 호출의 리터럴 액션 ID
7. ImGui 입력 직접 조회: 키·마우스 상태 API와 IO 필드 (UI 문맥과 백엔드 상태 쓰기는 제외)
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
    r'Begin|BeginPopupModal|BeginCombo|Combo|ListBox|TreeNode(?:Ex)?|CollapsingHeader|Selectable|RadioButton|TableSetupColumn|'
    r'Slider\w*|Drag(?:Float|Int|Scalar)\w*|Input\w*|ColorEdit\w*|PlotLines|PlotHistogram|Set(?:Item)?Tooltip)\s*\(')
RE_PRINTF = re.compile(r'%(?:[-+ #0]*\d*(?:\.\d+)?(?:hh|ll|[hlzjtL])?[a-zA-Z]|%)')
RE_NOTIFICATION = re.compile(r'\b(AddNotification)\s*\(')
RE_DRAW_TEXT = re.compile(r'->(AddText)\s*\(')


def nonlocal_key_positions(source: str):
    """Action IDs and workspace keys are identifiers, not localization references."""
    positions = set()
    calls = re.compile(r'\b(GetShortcutHint|ExecuteAction|CanExecuteAction|FindAction|GetActiveChord|'
        r'SetSplitter|GetSplitter|ACTION_MENU_ITEM|DrawActionTooltip)\s*\(')
    for match in calls.finditer(source):
        args = call_arguments(source, match.end())
        index = 1 if match[1] in ('ACTION_MENU_ITEM', 'DrawActionTooltip') else 0
        if len(args) <= index:
            continue
        offset = match.end() + sum(len(arg) + 1 for arg in args[:index])
        positions.update(offset + token.start() for token in re.finditer(r'"(?:\\.|[^"\\])*"', args[index]))
    for match in re.finditer(r'\.Id\s*=\s*("[^"\n]+")', source):
        positions.add(match.start(1))
    return positions


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
    for match in chain(RE_IMGUI.finditer(source), RE_NOTIFICATION.finditer(source), RE_DRAW_TEXT.finditer(source)):
        name = match[1]
        args = call_arguments(source, match.end())
        if name in ('AddNotification', 'InputTextWithHint', 'MenuItem'):
            indices = (0, 1)
        elif name == 'AddText':
            indices = (4,) if len(args) >= 5 else (2,)
        elif name == 'TextColored':
            indices = (1,)
        elif name in ('Combo', 'ListBox'):
            indices = (0, 2)
        elif name == 'TreeNodeEx':
            indices = (2,) if len(args) >= 3 else (0,)
        elif name == 'TreeNode':
            indices = (1,) if len(args) >= 2 else (0,)
        else:
            indices = (0,)
        for index in indices:
            if len(args) > index:
                for text in hardcoded_literals(args[index]):
                    yield source.count('\n', 0, match.start()) + 1, name, text


def hardcoded_literals(expression: str):
    # Remove localized subexpressions, but still inspect other ternary branches.
    localized = re.compile(r'\b(?:LOCAL\w*|LocalizeInspectorText|GetText|Text|GetShortcutHint)\s*\(')
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

def _iter_catalog_rows(file_path: Path, *, newline):
    if not file_path.is_file():
        return

    with file_path.open(encoding="utf-8-sig", newline=newline) as stream:
        reader = csv.reader(stream)
        next(reader, None)
        group = ""

        for row in reader:
            if len(row) < 2:
                continue

            group = row[0].strip() or group
            key = row[1].strip()
            if not key:
                continue

            full_key = f"{group}.{key}" if group else key
            yield full_key, row, reader.line_num


def load_csv_catalog(file_path: Path):
    catalog = {}
    empty_entries = []

    for key, row, line_no in _iter_catalog_rows(file_path, newline=None):
        en = row[2].strip() if len(row) > 2 else ""
        ko = row[3].strip() if len(row) > 3 else ""
        catalog[key] = (en, ko, file_path.name, line_no)

        if not en or not ko:
            empty_entries.append((file_path.name, line_no, key, en, ko))

    return catalog, empty_entries


def duplicate_catalog_keys(files):
    locations = {}

    for file_path in files:
        for key, _, line_no in _iter_catalog_rows(file_path, newline=""):
            locations.setdefault(key, []).append((file_path.name, line_no))

    return {key: spots for key, spots in locations.items() if len(spots) > 1}


def editor_catalog_layout_issues(file_path):
    """Editor rows explicitly own their namespace; Engine root/metadata keys keep their existing contract."""
    if not file_path.is_file():
        return
    with file_path.open(encoding='utf-8-sig', newline='') as stream:
        reader = csv.DictReader(stream)
        for row in reader:
            key = (row.get('Key') or '').strip()
            if not key:
                continue
            group = (row.get('Group') or '').strip()
            if not group:
                yield reader.line_num, key, 'Editor keys require an explicit Group'
            elif key.startswith(group + '.'):
                yield reader.line_num, key, 'Key repeats its Group prefix'


def find_unregistered_actions(sources):
    """Check literal action references against all action registrations, including menu helpers."""
    sources = {path: strip_comments(source) for path, source in sources.items()}
    registered = {match[1] for source in sources.values()
        for match in re.finditer(r'\.Id\s*=\s*"([^"\n]+)"', source)}
    calls = re.compile(r'\b(GetShortcutHint|ExecuteAction|CanExecuteAction|FindAction|GetActiveChord|'
        r'ACTION_MENU_ITEM|DrawActionTooltip)\s*\(')
    for path, source in sources.items():
        for match in calls.finditer(source):
            args = call_arguments(source, match.end())
            index = 1 if match[1] in ('ACTION_MENU_ITEM', 'DrawActionTooltip') else 0
            if len(args) <= index:
                continue
            action = re.fullmatch(r'\s*"([^"\n]+)"\s*', args[index])
            if action and action[1] not in registered:
                yield path, source.count('\n', 0, match.start()) + 1, match[1], action[1]


def find_panel_display_name_issues(sources):
    """Catch fixed IImGui names exposed through GetDisplayName's untranslated fallback."""
    sources = {Path(path): strip_comments(source) for path, source in sources.items()}
    for path, header in sources.items():
        if path.suffix != '.h' or 'Editor' not in path.parts:
            continue
        for declaration in re.finditer(r'\bclass\s+(\w+)(?:\s+final)?\s*:\s*public\s+IImGui\b', header):
            name = declaration[1]
            # Inspect the class's header and implementation together. Derived viewport names
            # are supplied by BaseViewportPanel's descriptor and are not fixed panel captions.
            files = [(p, text) for p, text in sources.items() if p.stem == name]
            combined = '\n'.join(text for _, text in files)
            if re.search(r'\bm_DisplayNameKey\s*=', combined):
                continue
            if re.search(r'GetDisplayName\([^)]*\)\s*const\s*override|::GetDisplayName\s*\(', combined):
                continue
            for owner, source in files:
                literal = re.search(r'\bm_Name\s*=\s*L?"([^"\n]+)"', source)
                if literal:
                    yield owner, source.count('\n', 0, literal.start()) + 1, name, literal[1]
                    break


def find_imgui_input_reads(source):
    source = strip_comments(source)
    calls = re.compile(r'\bImGui::(?:IsKey\w*|IsMouse\w*|GetMouse(?:Pos\w*|DragDelta|ClickedCount)|'
        r'GetKey(?:Index|PressedAmount|Data))\s*\(')
    for match in calls.finditer(source):
        # IsMousePosValid is also a raw pointer-state query; window/item hover APIs are intentionally allowed.
        yield source.count('\n', 0, match.start()) + 1, match[0].rstrip('(').strip()
    fields = re.compile(r'\b(?:ImGui::GetIO\(\)|io)\.(?:Key(?:Ctrl|Shift|Alt|Super)|'
        r'Mouse(?:Pos|Delta|WheelH?|Down))\b(?!\s*=(?!=))')
    for match in fields.finditer(source):
        yield source.count('\n', 0, match.start()) + 1, match[0]

def is_dynamic_prefix_matched(prefix: str, catalog: dict) -> bool:
    """LogLevel. 처럼 동적으로 런타임에 접미사가 붙는 키 접두사 검증"""
    if not prefix.endswith("."):
        return False
    for k in catalog.keys():
        if k.startswith(prefix) and len(k) > len(prefix):
            return True
    return False

def audit_project(root_dir: Path, check_orphans: bool = False, strict: bool = False):
    loc_dir = root_dir / "Resources" / "Data" / "Localization"
    catalog_paths = sorted(loc_dir.glob('*.csv'))
    combined_catalog, all_empty = {}, []
    for path in catalog_paths:
        catalog, empty = load_csv_catalog(path)
        combined_catalog.update(catalog)
        all_empty.extend(empty)
    catalog_groups = {key.split('.', 1)[0] for key in combined_catalog if '.' in key}
    duplicates = duplicate_catalog_keys(catalog_paths)
    editor_layout_issues = list(editor_catalog_layout_issues(loc_dir / 'Editor.csv'))

    missing_keys = []
    hardcoded_ui = []
    referenced_keys = set()
    sources = {}
    imgui_input_reads = []

    scan_dirs = [
        root_dir / "Editor" / "Source",
        root_dir / "Engine" / "Source",
        root_dir / "Client" / "Source",
        root_dir / "Shared" / "Source"
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
                sources[rel_path] = source
                imgui_input_reads.extend((rel_path, line, expression)
                    for line, expression in find_imgui_input_reads(source))
                for field in re.finditer(r'\b(DisplayNameKey|CategoryKey)\s*=\s*"([^"\n]+)"', source):
                    key = field[2]
                    referenced_keys.add(key)
                    if key not in combined_catalog:
                        missing_keys.append((rel_path, source.count('\n', 0, field.start()) + 1, field[1], key))

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
                identifier_positions = nonlocal_key_positions(source)
                for token in RE_CPP_TOKEN.finditer(source):
                    if not token[0].startswith('"'):
                        continue
                    if token.start() in identifier_positions:
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

    unregistered_actions = list(find_unregistered_actions(sources))
    panel_display_name_issues = list(find_panel_display_name_issues(sources))

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

    if duplicates:
        print(f"[!] DUPLICATE CSV KEYS ({len(duplicates)} keys):")
        for key, spots in sorted(duplicates.items()):
            print(f"    - {key}: " + ', '.join(f'{file}:{line}' for file, line in spots))
    else:
        print('[V] Duplicate CSV Keys: None')

    if editor_layout_issues:
        print(f'[!] EDITOR CATALOG LAYOUT ({len(editor_layout_issues)} locations):')
        for line, key, reason in editor_layout_issues:
            print(f'    - Editor.csv:{line} [{key}] -> {reason}')
    else:
        print('[V] Editor Catalog Groups: Explicit, without repeated prefixes')

    if panel_display_name_issues:
        print(f'[!] UNLOCALIZED PANEL DISPLAY NAMES ({len(panel_display_name_issues)} locations):')
        for path, line, panel, name in panel_display_name_issues:
            print(f'    - {path}:{line} [{panel}] -> {name}')
    else:
        print('[V] Fixed Panel Display Names: No untranslated fallback in checked classes')

    if unregistered_actions:
        print(f'[!] UNREGISTERED ACTION REFERENCES ({len(unregistered_actions)} locations):')
        for path, line, call, action in unregistered_actions:
            print(f'    - {path}:{line} [{call}] -> {action}')
    else:
        print('[V] Unregistered Shortcut Actions: None in literal action references')

    if imgui_input_reads:
        print(f'[!] RAW IMGUI INPUT READS ({len(imgui_input_reads)} locations):')
        for path, line, expression in imgui_input_reads:
            print(f'    - {path}:{line} -> {expression}')
    else:
        print('[V] Raw ImGui Input Reads: None in checked APIs')

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
        return 1 if (missing_keys or all_empty or hardcoded_ui or duplicates or editor_layout_issues or panel_display_name_issues or
            unregistered_actions or imgui_input_reads) else 0
    return 1 if (missing_keys or all_empty or duplicates or editor_layout_issues or panel_display_name_issues or
        unregistered_actions or imgui_input_reads) else 0

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Audit BamEngine Localization")
    parser.add_argument("--root", type=str, default=".", help="Project root directory")
    parser.add_argument("--orphans", action="store_true", help="Check for unused orphan keys in CSV")
    parser.add_argument("--strict", action="store_true", help="Fail if any hardcoded string is found")
    args = parser.parse_args()

    project_root = Path(args.root).resolve()
    exit_code = audit_project(project_root, check_orphans=args.orphans, strict=args.strict)
    sys.exit(exit_code)
