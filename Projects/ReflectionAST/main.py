import os
import argparse
import clang.cindex
from parser import init_libclang, CLASS_PATTERN, ENUM_PATTERN, parse_node
from generator import generate_cpp_code

def main():
    arg_parser = argparse.ArgumentParser(description="Generate reflection data via Clang AST")
    arg_parser.add_argument("--source-root", type=str, default=r"I:\BamtoliyaGithub\BamEngine\Engine\Source", help="Source directory to scan")
    arg_parser.add_argument("--output", type=str, default=r"I:\BamtoliyaGithub\BamEngine\Engine\Source\Core\Reflection\Private\Reflection.gen.cpp", help="Output cpp file path")
    # Ignored arguments from old macro
    arg_parser.add_argument("--module", type=str, help="Module name (ignored)")
    arg_parser.add_argument("--strip-namespace", action='append', help="Ignored")
    arg_parser.add_argument("--namespace-fallback", action='append', help="Ignored")
    arg_parser.add_argument("--resource-handle-template", type=str, help="Ignored")
    
    args = arg_parser.parse_args()
    
    init_libclang()
    engine_source_dir = os.path.abspath(args.source_root)
    output_path = os.path.abspath(args.output)
    
    index = clang.cindex.Index.create()
    all_parsed_classes = []
    all_parsed_enums = []
    included_headers = set()
    
    # =========================================================
    # Reflection Macro Injection
    # 모든 파일 맨 위에 아래 매크로들을 강제로 주입하여
    # include 실패로 인한 매크로 미전개 문제를 방지합니다.
    # =========================================================
    macro_injection = """
#ifndef __clang__
#define __clang__
#endif
#define CLASS(...)       __attribute__((annotate("reflect_class:" #__VA_ARGS__)))
#define STRUCT(...)      __attribute__((annotate("reflect_struct:" #__VA_ARGS__)))
#define PROPERTY(...)    __attribute__((annotate("reflect_property:" #__VA_ARGS__)))
#define FUNCTION(...)    __attribute__((annotate("reflect_function:" #__VA_ARGS__)))
#define ENUM(...)        __attribute__((annotate("reflect_enum:" #__VA_ARGS__)))
"""
    
    for root, _, files in os.walk(engine_source_dir):
        for file in files:
            if not file.endswith(".h"):
                continue
                
            filepath = os.path.join(root, file)
            with open(filepath, 'r', encoding='utf-8-sig', errors='ignore') as f:
                original_code = f.read()
                
            if not any(k in original_code for k in ["CLASS", "STRUCT", "ENUM"]):
                continue
                
            target_classes = set()
            target_enums = set()
            
            for match in CLASS_PATTERN.finditer(original_code):
                target_classes.add(match.group(1).strip())
            for match in ENUM_PATTERN.finditer(original_code):
                target_enums.add(match.group(1).strip())
                
            if not target_classes and not target_enums:
                continue
                
            rel_path = os.path.relpath(filepath, engine_source_dir).replace("\\", "/")
            included_headers.add(rel_path)
            
            # 파일 맨 위에 매크로 주입
            injected_code = macro_injection + original_code
            
            include_args = [
                '-xc++', 
                '-std=c++20',
                '-DENGINE_API=',
                '-DBAM_PLATFORM_WINDOWS',
                '-D_ALLOW_COMPILER_AND_STL_VERSION_MISMATCH',
                f'-I{engine_source_dir}'
            ]
            
            # unsaved_files를 사용하여 주입된 코드를 파싱
            translation_unit = index.parse(
                filepath, 
                args=include_args,
                unsaved_files=[(filepath, injected_code)]
            )
            for diag in translation_unit.diagnostics:
                if diag.severity >= clang.cindex.Diagnostic.Error:
                    print(f"[Error] {diag.location}: {diag.spelling}")
            
            
            parse_node(translation_unit.cursor, target_classes, target_enums, all_parsed_classes, all_parsed_enums, filepath, rel_path, original_code)
            print(f"[AST] Parsed: {rel_path} (Classes: {len(target_classes)}, Enums: {len(target_enums)})")

    final_cpp_code = generate_cpp_code(all_parsed_classes, all_parsed_enums, included_headers)
    
    os.makedirs(os.path.dirname(output_path), exist_ok=True)
    with open(output_path, "w", encoding='utf-8') as f:
        f.write(final_cpp_code)
        
    print(f"\n[성공] 클래스 {len(all_parsed_classes)}개, 열거형 {len(all_parsed_enums)}개 등록 완료!")

if __name__ == "__main__":
    main()