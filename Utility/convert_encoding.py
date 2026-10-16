import os
import sys
import chardet
import codecs

def convert_to_utf8_bom(dir_path):
    """
    지정된 디렉토리의 .cpp 및 .h 파일들을 UTF-8 BOM 인코딩으로 변환합니다.
    이미 UTF-8 BOM인 파일은 건너뜁니다.
    """
    if not os.path.isdir(dir_path):
        print(f"[오류] 디렉토리 '{dir_path}'를 찾을 수 없습니다.")
        return

    for root, _, files in os.walk(dir_path):
        for file in files:
            # 검사할 확장자 목록
            if file.lower().endswith((".cpp", ".h", ".hpp", ".c", ".cc", ".cxx")):
                file_path = os.path.join(root, file)
                try:
                    convert_file_encoding_to_utf8_bom(file_path)
                except Exception as e:
                    print(f"[오류] 파일 '{file_path}' 처리 중 예외 발생 - {e}")

def convert_file_encoding_to_utf8_bom(file_path):
    """
    단일 파일을 검사하여 UTF-8 BOM이 아니면 변환합니다.
    """
    try:
        with open(file_path, 'rb') as f:
            raw_data = f.read()

        # [최적화] 파일이 이미 UTF-8 BOM으로 시작하는지 확인
        if raw_data.startswith(codecs.BOM_UTF8):
            # print(f"[패스] '{file_path}' - 이미 UTF-8 BOM입니다.") # 로그가 너무 많으면 주석 처리
            return

        # --- 이 아래는 변환이 필요한 경우에만 실행됩니다 ---

        # 인코딩 감지
        result = chardet.detect(raw_data)
        original_encoding = result['encoding']

        # chardet가 None을 반환하거나 신뢰도가 낮을 경우 대비
        if not original_encoding:
            original_encoding = 'unknown'

        try:
            # 감지된 인코딩으로 디코딩 시도
            text = raw_data.decode(original_encoding)
        except (UnicodeDecodeError, TypeError):
            # 실패 시 한국어 윈도우 기본 인코딩인 cp949로 재시도
            try:
                text = raw_data.decode('cp949')
                original_encoding = 'cp949 (fallback)'
            except UnicodeDecodeError:
                # 그래도 안되면 에러 무시하고 강제 디코딩 (데이터 손실 가능성 있음)
                text = raw_data.decode('utf-8', errors='ignore')
                original_encoding = 'utf-8 (forced)'

        # UTF-8 with BOM으로 쓰기 ('utf-8-sig'가 BOM을 자동으로 붙여줌)
        with open(file_path, 'w', encoding='utf-8-sig', newline='\n') as f:
            f.write(text)

        print(f"[변환] '{file_path}' ({original_encoding}) → UTF-8 BOM")

    except Exception as e:
        raise Exception(f"파일 처리 오류: {e}")

if __name__ == "__main__":
    # 1. 스크립트가 위치한 현재 절대 경로 (프로젝트 루트로 가정)
    base_path = os.path.dirname(os.path.abspath(__file__))
    target_path = base_path

    # 2. 매개변수가 있는지 확인
    if len(sys.argv) == 2:
        folder_name = sys.argv[1]
        # 입력받은 폴더명을 현재 경로와 합침 (예: base_path/Client)
        target_path = os.path.join(base_path, folder_name)

    print(f"📁 변환 대상 경로: {target_path}")
    print("-" * 60)
    
    convert_to_utf8_bom(target_path)
    
    print("-" * 60)
    print("✅ 모든 작업 완료.")