import csv
import json
import os
import glob
import argparse
import sys  # [추가] 종료를 위해 필요

def main():
    # 1. 인자 설정
    parser = argparse.ArgumentParser(description='Convert CSV to JSON with Duplicate Key Check.')
    parser.add_argument('--input', '-i', default='Resources/Data/Localization', help='Input directory or single CSV file.')
    parser.add_argument('--output', '-o', default=None, help='Output directory or file path.')
    
    args = parser.parse_args()
    input_path = args.input
    output_arg = args.output
    
    # 2. 출력 디렉토리 결정
    if output_arg is None:
        if os.path.isfile(input_path):
            output_dir = os.path.dirname(input_path)
        else:
            output_dir = input_path
    else:
        ext = os.path.splitext(output_arg)[1]
        if ext:
            output_dir = os.path.dirname(output_arg)
        else:
            output_dir = output_arg

    if not output_dir: output_dir = '.'

    # 데이터 구조
    data = {
        "English": {},
        "Korean": {}
    }

    # [추가] 중복 키 검사를 위한 집합 (Set)
    seen_keys = set()

    # 3. CSV 파일 목록 수집
    csv_files = []
    if os.path.isfile(input_path):
        csv_files = [input_path]
    elif os.path.isdir(input_path):
        csv_files = glob.glob(os.path.join(input_path, '*.csv'))
    
    if not csv_files:
        print(f"[Warning] No CSV files found in {input_path}")
        return

    print(f"Checking for duplicates and converting {len(csv_files)} files...")

    # 4. CSV 읽기 및 병합
    for file_path in csv_files:
        filename = os.path.basename(file_path)
        print(f" - Scanning: {filename}")

        current_sub_group = "" 

        try:
            f = open(file_path, 'r', encoding='utf-8-sig')
            f.read(1); f.seek(0)
        except:
            f = open(file_path, 'r', encoding='cp949')

        with f:
            reader = csv.DictReader(f)
            line_count = 0
            for row in reader:
                line_count += 1
                raw_key = row.get('Key', '').strip()
                if not raw_key: continue

                # 셀 병합 처리
                raw_group = row.get('Group', '').strip()
                if raw_group: current_sub_group = raw_group
                
                # 키 생성 (Group.Key 또는 Key)
                if current_sub_group:
                    final_key = f"{current_sub_group}.{raw_key}"
                else:
                    final_key = raw_key

                # -------------------------------------------------------
                # [핵심] 중복 키 검사 로직
                # -------------------------------------------------------
                if final_key in seen_keys:
                    print(f"\n[ERROR] Duplicate Key Detected!")
                    print(f"--------------------------------------------------")
                    print(f" File    : {filename}")
                    print(f" Line    : {line_count + 1} (approx)")
                    print(f" Key     : '{final_key}'")
                    print(f"--------------------------------------------------")
                    print(f"Aborting conversion to prevent data corruption.")
                    
                    # 스크립트 비정상 종료 (에러 코드 1 반환)
                    sys.exit(1)
                
                # 중복이 아니면 등록
                seen_keys.add(final_key)
                # -------------------------------------------------------

                # 언어별 데이터 수집
                for lang in data.keys():
                    if row.get(lang):
                        data[lang][final_key] = row[lang]

    # 5. 저장 (중복이 없을 때만 실행됨)
    if output_dir:
        os.makedirs(output_dir, exist_ok=True)

    print(f"\n[Success] No duplicates found. Saving files...")

    for lang, content in data.items():
        if not content: continue 

        lang_filepath = os.path.join(output_dir, f"{lang}.json")
        final_json_structure = { lang: content }

        try:
            with open(lang_filepath, 'w', encoding='utf-8') as f:
                json.dump(final_json_structure, f, indent=4, ensure_ascii=False)
            print(f" - Saved: {lang_filepath} (Keys: {len(content)})")
        except Exception as e:
            print(f" [Error] Failed to save {lang}: {e}")

if __name__ == "__main__":
    main()