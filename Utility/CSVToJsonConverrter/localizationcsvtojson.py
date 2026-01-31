import csv
import json
import os
import glob
import argparse

def main():
    # 1. 인자 설정
    parser = argparse.ArgumentParser(description='Convert CSV localization files to separated JSON files (e.g., English.json).')
    parser.add_argument('--input', '-i', default='Resources/Data/Localization', help='Input directory or single CSV file.')
    parser.add_argument('--output', '-o', default=None, help='Output directory or file path. Files will be saved as {Language}.json in this folder.')
    
    args = parser.parse_args()
    input_path = args.input
    output_arg = args.output
    
    # 2. 출력 디렉토리 결정
    # 사용자가 출력 경로를 입력하지 않으면 -> 입력 경로의 폴더를 사용
    if output_arg is None:
        if os.path.isfile(input_path):
            output_dir = os.path.dirname(input_path)
        else:
            output_dir = input_path # 폴더인 경우 그 폴더 그대로 사용
    else:
        # 사용자가 파일 경로(예: Data/Loc.json)를 입력했더라도 폴더(Data/)만 추출해서 사용
        # 사용자가 폴더(Data/)를 입력했다면 그 폴더 사용
        ext = os.path.splitext(output_arg)[1]
        if ext: # 확장자가 있으면 파일로 간주하고 부모 폴더 추출
            output_dir = os.path.dirname(output_arg)
        else:
            output_dir = output_arg

    # 출력 폴더가 비어있으면(현재 폴더) '.' 로 설정
    if not output_dir:
        output_dir = '.'

    # 데이터 구조 (필요한 언어 추가 가능)
    data = {
        "English": {},
        "Korean": {}
    }

    # 3. CSV 파일 목록 수집
    csv_files = []
    if os.path.isfile(input_path):
        csv_files = [input_path]
    elif os.path.isdir(input_path):
        csv_files = glob.glob(os.path.join(input_path, '*.csv'))
    
    if not csv_files:
        print(f"[Warning] No CSV files found in {input_path}")
        return

    print(f"Sources: {len(csv_files)} files found. Target Directory: '{output_dir}'")

    # 4. CSV 읽기 및 데이터 병합
    for file_path in csv_files:
        filename = os.path.basename(file_path)
        file_category = os.path.splitext(filename)[0]
        current_sub_group = "" 

        # 인코딩 처리
        try:
            f = open(file_path, 'r', encoding='utf-8-sig')
            f.read(1); f.seek(0)
        except:
            f = open(file_path, 'r', encoding='cp949')

        with f:
            reader = csv.DictReader(f)
            for row in reader:
                raw_key = row.get('Key', '').strip()
                if not raw_key: continue

                # 셀 병합 처리
                raw_group = row.get('Group', '').strip()
                if raw_group: current_sub_group = raw_group
                
                # 키 생성 (카테고리.그룹.키)
                if current_sub_group: final_key = f"{file_category}.{current_sub_group}.{raw_key}"
                else: final_key = f"{file_category}.{raw_key}"

                # 언어별 데이터 수집
                for lang in data.keys():
                    if row.get(lang):
                        data[lang][final_key] = row[lang]

    # 5. 언어별로 파일 저장 (파일명: English.json, Korean.json)
    if output_dir:
        os.makedirs(output_dir, exist_ok=True)

    for lang, content in data.items():
        if not content:
            continue 

        # [변경점] 파일명에서 BaseName 제거 -> 순수하게 {Language}.json 사용
        lang_filename = f"{lang}.json"
        lang_filepath = os.path.join(output_dir, lang_filename)

        # JSON 구조: { "English": { ... } }
        final_json_structure = { lang: content }

        try:
            with open(lang_filepath, 'w', encoding='utf-8') as f:
                json.dump(final_json_structure, f, indent=4, ensure_ascii=False)
            print(f" - Saved: {lang_filepath} (Keys: {len(content)})")
        except Exception as e:
            print(f" [Error] Failed to save {lang}: {e}")

if __name__ == "__main__":
    main()