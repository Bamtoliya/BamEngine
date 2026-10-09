import re
from models import ClassInfo, EnumInfo
from typing import List, Set

def split_metadata_attributes(text: str) -> List[str]:
    attributes = []
    start = 0
    stack = []
    quote = None
    escaped = False

    opening = {"(": ")", "[": "]", "{": "}"}
    closing = set(opening.values())

    def append_attribute(end: int):
        attribute = text[start:end].strip()

        if not attribute:
            raise ValueError("메타데이터에 빈 항목이 있습니다.")

        # 메타데이터 매크로 이름 또는 매크로 호출만 허용합니다.
        if not re.fullmatch(
            r"[A-Za-z_]\w*(?:\s*\(.*\))?",
            attribute,
            flags=re.DOTALL,
        ):
            raise ValueError(
                f"잘못된 메타데이터 항목: {attribute!r}. "
                '표시 이름은 NAME("...")으로 작성하세요.'
            )

        if re.match(r"COLOR\s*\(", attribute):
            raise ValueError(
                "COLOR는 인자를 받지 않습니다. COLOR() 대신 COLOR를 사용하세요."
            )

        attributes.append(attribute)

    if not text.strip():
        return attributes

    for index, character in enumerate(text):
        if quote is not None:
            if escaped:
                escaped = False
            elif character == "\\":
                escaped = True
            elif character == quote:
                quote = None

            continue

        if character in ('"', "'"):
            quote = character
        elif character in opening:
            stack.append(opening[character])
        elif character in closing:
            if not stack or stack.pop() != character:
                raise ValueError("메타데이터의 괄호가 일치하지 않습니다.")
        elif character == "," and not stack:
            append_attribute(index)
            start = index + 1

    if quote is not None or stack:
        raise ValueError("메타데이터의 문자열 또는 괄호가 닫히지 않았습니다.")

    append_attribute(len(text))
    return attributes

def generate_cpp_code(class_infos: List[ClassInfo], enum_infos: List[EnumInfo], included_headers: Set[str]) -> str:
    cpp_code = "// ==================================================\n"
    cpp_code += "// 자동 생성된 리플렉션 코드 (수정하지 마세요!)\n"
    cpp_code += "// ==================================================\n\n"
    
    cpp_code += "#include \"Engine_Includes.h\"\n"
    cpp_code += "#include <entt/entt.hpp>\n"
    cpp_code += ('#include "reflection/macros/ReflectionMetadataMacros.h"\n')
    
    for header in sorted(included_headers):
        cpp_code += f'#include "{header}"\n'
    
    cpp_code += "\nnamespace Engine {\n\n"
    cpp_code += "void RegisterReflection_EnTT() \n{\n"
    cpp_code += "    using namespace entt::literals;\n\n"
    
    # Enum 출력
    for enum in enum_infos:
        cpp_code += f"    BEGIN_ENTT_REFLECT_ENUM({enum.name})\n"
        for entry in enum.entries:
            cpp_code += f"        ENTT_ENUM_ENTRY({enum.name}, {entry.name})\n"
        cpp_code += f"    END_ENTT_REFLECT_ENUM()\n\n"
    
    # Class 출력
    # Class 출력
    for class_index, cls in enumerate(class_infos):
        metadata_names = {}

        for property_index, prop in enumerate(cls.properties):
            try:
                attributes = split_metadata_attributes(prop.attributes)
            except ValueError as error:
                raise ValueError(
                    f"{cls.header_path}: {cls.name}::{prop.name}: {error}"
                ) from error

            if not attributes:
                continue

            metadata_name = (
                f"propertyMetadata_{class_index}_{property_index}"
            )
            metadata_names[prop.name] = metadata_name

            cpp_code += (
                "    static constexpr reflection::MetadataEntry "
                f"{metadata_name}[] = {{\n"
            )

            for attribute in attributes:
                # 매크로 자체에 끝 쉼표가 있으므로 추가하지 않습니다.
                cpp_code += f"        {attribute}\n"

            cpp_code += "    };\n\n"

        cpp_code += f"    BEGIN_ENTT_REFLECT({cls.name})\n"
        cpp_code += f"        .ctor<>()\n"
        if hasattr(cls, 'parent_name') and cls.parent_name:
            cpp_code += f"        .base<{cls.parent_name}>()\n"
        
        for prop in cls.properties:
            cpp_code += (
                f"        ENTT_PROPERTY({cls.name}, {prop.name})\n"
            )

            metadata_name = metadata_names.get(prop.name)

            if metadata_name is not None:
                cpp_code += (
                    "        .custom<reflection::MetadataView>("
                    f"reflection::MetadataView{{{metadata_name}}})\n"
                )
        for func in cls.functions:
            owner_type = (
                f"::{cls.namespace}::{cls.name}"
                if cls.namespace
                else f"::{cls.name}"
            )

            parameters = ", ".join(func.parameter_types)

            if func.is_static:
                pointer_type = (
                    f"{func.return_type} (*)({parameters})"
                )
            else:
                qualifiers = " const" if func.is_const else ""
                pointer_type = (
                    f"{func.return_type} "
                    f"({owner_type}::*)({parameters}){qualifiers}"
                )

            cpp_code += (
                f"        .func<static_cast<{pointer_type}>"
                f"(&{owner_type}::{func.name})>"
                f"(\"{func.name}\")\n"
            )
            
        cpp_code += f"    END_ENTT_REFLECT()\n\n"
    
    cpp_code += "}\n\n"
    cpp_code += "} // namespace Engine\n\n"

    return cpp_code