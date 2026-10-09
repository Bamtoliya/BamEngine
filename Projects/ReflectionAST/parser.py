import clang.cindex
import os
import re
from models import ClassInfo, PropertyInfo, FunctionInfo, EnumInfo, EnumEntryInfo

CLASS_PATTERN = re.compile(r'(?:STRUCT|CLASS)\s*\([^)]*\)\s*(?:struct|class)\s+(?:[A-Z_0-9]+\s+)?(\w+)')
ENUM_PATTERN = re.compile(r'ENUM\s*\([^)]*\)\s*enum\s+(?:class\s+)?(\w+)')

def group_header_roots(translation_unit, filepaths):
    def normalize(path):
        return os.path.normcase(os.path.abspath(path))

    roots_by_file = {
        normalize(filepath): []
        for filepath in filepaths
    }

    def visit(node):
        location_file = node.location.file

        if location_file is not None:
            path = normalize(location_file.name)

            if path in roots_by_file:
                roots_by_file[path].append(node)

            # 파일 위치가 있는 노드는 여기서 분류를 끝냅니다.
            # 하위 선언은 기존 헤더별 순회가 처리합니다.
            return

        # translation unit 등 파일 위치가 없는 상위 노드만 통과합니다.
        for child in node.get_children():
            visit(child)

    visit(translation_unit.cursor)
    return roots_by_file

def init_libclang():
    clang_dll = (
        r"C:\Program Files\Microsoft Visual Studio"
        r"\18\Community\VC\Tools\Llvm\x64\bin\libclang.dll"
    )

    if not os.path.isfile(clang_dll):
        raise FileNotFoundError(
            f"libclang.dll을 찾을 수 없습니다: {clang_dll}"
        )

    clang.cindex.Config.set_library_file(clang_dll)
    print(f"[AST Parser] libclang: {clang_dll}")

# ==================================================
# 내부 헬퍼 함수: Enum 노드 상세 파싱
# ==================================================
def _parse_enum_decl(node, enum_infos):
    new_enum = EnumInfo(name=node.spelling, attributes="")
    
    for child in node.get_children():
        if child.kind == clang.cindex.CursorKind.ANNOTATE_ATTR and child.spelling.startswith("reflect_enum:"):
            new_enum.attributes = child.spelling.replace("reflect_enum:", "", 1).strip()
            
        elif child.kind == clang.cindex.CursorKind.ENUM_CONSTANT_DECL:
            new_enum.entries.append(EnumEntryInfo(name=child.spelling, value=child.enum_value))
            
    enum_infos.append(new_enum)

# ==================================================
# 내부 헬퍼 함수: Class/Struct 노드 상세 파싱
# ==================================================
def _parse_class_decl(node, class_infos, header_path, original_code):
    # Get namespace
    ns = []
    p = node.semantic_parent
    while p and p.kind == clang.cindex.CursorKind.NAMESPACE:
        ns.insert(0, p.spelling)
        p = p.semantic_parent
    namespace_str = "::".join(ns)
    
    # Check if this class has REFLECT_CLASS/STRUCT/BASE in the file
    # (Since we just need to know if the file declares the macro for this class,
    # and mostly it's one class per file, checking the whole file is an okay heuristic.
    # To be safer, we check if the macro strings exist).
    has_reflect = any(m in original_code for m in ["REFLECT_CLASS", "REFLECT_STRUCT", "REFLECT_BASE"])

    new_class = ClassInfo(name=node.spelling, header_path=header_path, namespace=namespace_str, has_reflect_macro=has_reflect)
    
    for child in node.get_children():
        try:
            child_kind = child.kind
        except ValueError:
            continue
            
        # 1. 프로퍼티(변수) 파싱
        if child_kind == clang.cindex.CursorKind.FIELD_DECL:
            is_property = False
            attr_str = ""
            for c in child.get_children():
                if c.kind == clang.cindex.CursorKind.ANNOTATE_ATTR and c.spelling.startswith("reflect_property:"):
                    is_property = True
                    attr_str = c.spelling.replace("reflect_property:", "", 1).strip()
                    break
            if is_property and child.spelling:
                new_class.properties.append(PropertyInfo(name=child.spelling, type_name=child.type.spelling, attributes=attr_str))

        
        # 2. 함수(메서드) 파싱
        elif child_kind == clang.cindex.CursorKind.CXX_METHOD:
            is_function = False
            attr_str = ""
            for c in child.get_children():
                if c.kind == clang.cindex.CursorKind.ANNOTATE_ATTR and c.spelling.startswith("reflect_function:"):
                    is_function = True
                    attr_str = c.spelling.replace("reflect_function:", "", 1).strip()
                    break
            if is_function:
                new_class.functions.append(
                    FunctionInfo(
                        name=child.spelling,
                        attributes=attr_str,
                        return_type=child.result_type.get_canonical().spelling,
                        parameter_types=[
                            argument.type.get_canonical().spelling
                            for argument in child.get_arguments()
                        ],
                        is_const=child.is_const_method(),
                        is_static=child.is_static_method(),
                    )
                )
                
    class_infos.append(new_class)

# ==================================================
# 메인 AST 순회 함수
# ==================================================
def parse_node(node, target_classes, target_enums, class_infos, enum_infos, target_filepath, header_path, original_code):
    try:
        node_kind = node.kind
    except ValueError:
        return

    # 외부 헤더 파일(STL 등)은 무시하여 속도 최적화
    if node.location.file:
        node_file = os.path.normcase(
            os.path.abspath(node.location.file.name)
        )
        target_file = os.path.normcase(
            os.path.abspath(target_filepath)
        )
        if node_file != target_file:
            return

        declaration_offset = node.location.offset

        if (
            node_kind == clang.cindex.CursorKind.ENUM_DECL
            and declaration_offset in target_enums
        ):
            _parse_enum_decl(node, enum_infos)
            enum_infos[-1].attributes = target_enums.pop(declaration_offset)

        elif (
            node_kind in (
                clang.cindex.CursorKind.CLASS_DECL,
                clang.cindex.CursorKind.STRUCT_DECL,
            )
            and declaration_offset in target_classes
        ):
            _parse_class_decl(
                node, class_infos, header_path, original_code
            )
            class_infos[-1].attributes = target_classes.pop(
                declaration_offset
            )
            
    # 자식 노드 재귀 탐색
    for child in node.get_children():
        parse_node(child, target_classes, target_enums, class_infos, enum_infos, target_filepath, header_path, original_code)


def collect_type_markers(translation_unit, filepath, root_nodes=None):
    target_path = os.path.normcase(os.path.abspath(filepath))
    kind = clang.cindex.CursorKind

    expected_kinds = {
        "CLASS": kind.CLASS_DECL,
        "STRUCT": kind.STRUCT_DECL,
        "ENUM": kind.ENUM_DECL,
    }

    markers = []
    declarations = []

    def visit(node):
        try:
            node_kind = node.kind
        except ValueError:
            return

        if node.location.file:
            node_path = os.path.normcase(
                os.path.abspath(node.location.file.name)
            )

            if node_path != target_path:
                return

        if (
            node_kind == kind.MACRO_INSTANTIATION
            and node.spelling in expected_kinds
        ):
            markers.append(node)

        elif node_kind in expected_kinds.values():
            declarations.append(node)

        for child in node.get_children():
            visit(child)

    if root_nodes is None:
        visit(translation_unit.cursor)
    else:
        for root_node in root_nodes:
            visit(root_node)

    # Clang의 offset은 바이트 기준이므로 바이너리로 읽습니다.
    with open(filepath, "rb") as source_file:
        source = source_file.read()

    declarations.sort(key=lambda node: node.extent.start.offset)

    class_targets = {}
    enum_targets = {}

    for marker in sorted(
        markers, key=lambda node: node.extent.start.offset
    ):
        marker_end = marker.extent.end.offset

        declaration = next(
            (
                node for node in declarations
                if node.extent.start.offset >= marker_end
            ),
            None,
        )

        if declaration is None:
            raise RuntimeError(
                f"{marker.location}: 타입 매크로 뒤에 선언이 없습니다."
            )

        # 매크로와 타입 선언 사이에는 공백과 주석만 허용합니다.
        gap = source[
            marker_end:declaration.extent.start.offset
        ].decode("utf-8")

        gap = re.sub(r"/\*.*?\*/|//[^\r\n]*", "", gap, flags=re.S)

        if gap.strip():
            raise RuntimeError(
                f"{marker.location}: 타입 매크로 바로 뒤에 "
                "class/struct/enum 선언이 필요합니다."
            )

        if declaration.kind != expected_kinds[marker.spelling]:
            raise RuntimeError(
                f"{marker.location}: 매크로 종류와 선언 종류가 다릅니다."
            )

        if not declaration.is_definition():
            raise RuntimeError(
                f"{marker.location}: 전방 선언 대신 타입 정의에 "
                "리플렉션 매크로를 붙여주세요."
            )

        tokens = list(marker.get_tokens())

        if (
            len(tokens) < 3
            or tokens[1].spelling != "("
            or tokens[-1].spelling != ")"
        ):
            raise RuntimeError(
                f"{marker.location}: 타입 매크로 인자를 읽지 못했습니다."
            )

        # 원문을 잘라 중첩 괄호와 문자열을 그대로 보존합니다.
        attributes = source[
            tokens[1].extent.end.offset:
            tokens[-1].extent.start.offset
        ].decode("utf-8").strip()

        targets = (
            enum_targets
            if marker.spelling == "ENUM"
            else class_targets
        )

        declaration_offset = declaration.location.offset

        if declaration_offset in targets:
            raise RuntimeError(
                f"{marker.location}: 한 타입에 매크로가 중복됐습니다."
            )

        targets[declaration_offset] = attributes

    return class_targets, enum_targets