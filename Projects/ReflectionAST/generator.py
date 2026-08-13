from models import ClassInfo, EnumInfo
from typing import List, Set

def generate_cpp_code(class_infos: List[ClassInfo], enum_infos: List[EnumInfo], included_headers: Set[str]) -> str:
    cpp_code = "// ==================================================\n"
    cpp_code += "// 자동 생성된 리플렉션 코드 (수정하지 마세요!)\n"
    cpp_code += "// ==================================================\n\n"
    
    cpp_code += "#include \"Engine_Includes.h\"\n"
    cpp_code += "#include <entt/entt.hpp>\n"
    
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
    for cls in class_infos:
        cpp_code += f"    BEGIN_ENTT_REFLECT({cls.name})\n"
        cpp_code += f"        .ctor<>()\n"
        if hasattr(cls, 'parent_name') and cls.parent_name:
            cpp_code += f"        .base<{cls.parent_name}>()\n"
        
        for prop in cls.properties:
            # 속성(Attribute)이 있으면 _WITH_ATTR 매크로 사용
            if prop.attributes:
                # 문자열 안에 큰따옴표가 들어갈 수 있으므로 이스케이프 처리
                escaped_attrs = prop.attributes.replace('"', '\\"')
                cpp_code += f"        ENTT_PROPERTY_WITH_ATTR({cls.name}, {prop.name}, \"{escaped_attrs}\")\n"
            else:
                cpp_code += f"        ENTT_PROPERTY({cls.name}, {prop.name})\n"
                
        for func in cls.functions:
            cpp_code += f"        ENTT_FUNCTION({cls.name}, {func.name})\n"
            
        cpp_code += f"    END_ENTT_REFLECT()\n\n"
    
    cpp_code += "}\n\n"
    cpp_code += "} // namespace Engine\n\n"

    return cpp_code