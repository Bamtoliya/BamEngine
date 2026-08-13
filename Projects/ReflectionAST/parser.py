import clang.cindex
import os
import re
from models import ClassInfo, PropertyInfo, FunctionInfo, EnumInfo, EnumEntryInfo

CLASS_PATTERN = re.compile(r'(?:STRUCT|CLASS)\s*\([^)]*\)\s*(?:struct|class)\s+(?:[A-Z_0-9]+\s+)?(\w+)')
ENUM_PATTERN = re.compile(r'ENUM\s*\([^)]*\)\s*enum\s+(?:class\s+)?(\w+)')

def init_libclang():
    clang_dll = r"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\Llvm\x64\bin\libclang.dll"
    if os.path.exists(clang_dll):
        clang.cindex.Config.set_library_file(clang_dll)
    else:
        print("[AST Parser] 오류: libclang.dll을 찾을 수 없습니다.")

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
                new_class.functions.append(FunctionInfo(name=child.spelling, attributes=attr_str))
                
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
        node_file = os.path.abspath(node.location.file.name)
        target_file = os.path.abspath(target_filepath)
        if node_file != target_file:
            return

    # Enum 식별
    if node_kind == clang.cindex.CursorKind.ENUM_DECL:
        enum_name = node.spelling
        if enum_name in target_enums:
            if not any(e.name == enum_name for e in enum_infos):
                _parse_enum_decl(node, enum_infos)

    # Class/Struct 식별
    elif node_kind in (clang.cindex.CursorKind.CLASS_DECL, clang.cindex.CursorKind.STRUCT_DECL):
        class_name = node.spelling
        if class_name in target_classes:
            if not any(c.name == class_name for c in class_infos):
                _parse_class_decl(node, class_infos, header_path, original_code)
            
    # 자식 노드 재귀 탐색
    for child in node.get_children():
        parse_node(child, target_classes, target_enums, class_infos, enum_infos, target_filepath, header_path, original_code)