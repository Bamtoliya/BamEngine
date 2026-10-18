# --------------------------------------------------------------------------------
# Utility/CSVToJsonConverter/localizationcsvtojson.py 수정안
# --------------------------------------------------------------------------------

import csv
import json
import os
import glob
import argparse
import sys

def main():
    parser = argparse.ArgumentParser(description='Convert CSV to JSON with Duplicate & Missing Key Check.')
    parser.add_argument('--input', '-i', default='Resources/Data/Localization', help='Input directory or single CSV file.')
    parser.add_argument('--output', '-o', default=None, help='Output directory or file path.')
    parser.add_argument('--strict', action='store_true', help='Fail conversion if missing translations are detected.')
    
    args = parser.parse_args()
    input_path = args.input
    output_arg = args.output
    
    if output_arg is None:
        output_dir = os.path.dirname(input_path) if os.path.isfile(input_path) else input_path
    else:
        ext = os.path.splitext(output_arg)[1]
        output_dir = os.path.dirname(output_arg) if ext else output_arg

    if not output_dir:
        output_dir = '.'

    data = {"English": {}, "Korean": {}}
    seen_keys = set()
    
    # 누락된 항목을 수집할 딕셔너리
    missing_data = {"English": [], "Korean": []}

    csv_files = []
    if os.path.isfile(input_path):
        csv_files = [input_path]
    elif os.path.isdir(input_path):
        csv_files = glob.glob(os.path.join(input_path, '*.csv'))
    
    if not csv_files:
        print(f"[Warning] No CSV files found in {input_path}")
        return

    print(f"Checking keys and converting {len(csv_files)} files...")

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
                if not raw_key:
                    continue

                raw_group = row.get('Group', '').strip()
                if raw_group:
                    current_sub_group = raw_group
                
                final_key = f"{current_sub_group}.{raw_key}" if current_sub_group else raw_key

                # 중복 키 검사
                if final_key in seen_keys:
                    print(f"\n[ERROR] Duplicate Key Detected in {filename}:{line_count + 1} -> '{final_key}'")
                    sys.exit(1)
                
                seen_keys.add(final_key)

                # 언어별 데이터 수집 및 누락 검사
                for lang in data.keys():
                    text_val = (row.get(lang) or '').strip()
                    if text_val:
                        data[lang][final_key] = text_val
                    else:
                        missing_data[lang].append((filename, line_count + 1, final_key))

    # 누락된 데이터 리포트 출력
    total_missing = sum(len(v) for v in missing_data.values())
    if total_missing > 0:
        print(f"\n[WARNING] Missing Localization Data Detected ({total_missing} issues):")
        for lang, items in missing_data.items():
            if not items:
                continue
            print(f"  * [{lang}] Missing count: {len(items)}")
            for fname, line_num, key_name in items:
                print(f"      - {fname}:{line_num} -> Key: '{key_name}'")

        if args.strict:
            print("\n[ERROR] Aborting due to --strict flag with missing translations.")
            sys.exit(1)

    # JSON 저장
    if output_dir:
        os.makedirs(output_dir, exist_ok=True)

    print(f"\n[Success] Saving files...")
    for lang, content in data.items():
        if not content:
            continue
        lang_filepath = os.path.join(output_dir, f"{lang}.json")
        try:
            with open(lang_filepath, 'w', encoding='utf-8') as f:
                json.dump({lang: content}, f, indent=4, ensure_ascii=False)
            print(f" - Saved: {lang_filepath} (Keys: {len(content)})")
        except Exception as e:
            print(f" [Error] Failed to save {lang}: {e}")

if __name__ == "__main__":
    main()