import os
import sys
import shutil

def move_all_files_to_root(target_dir):
    # 1. 입력받은 경로가 유효한 폴더인지 확인
    if not os.path.exists(target_dir) or not os.path.isdir(target_dir):
        print(f"❌ 오류: 유효한 폴더 경로가 아닙니다 -> {target_dir}")
        return

    # 2. 절대 경로로 변환하여 안전하게 처리
    target_dir = os.path.abspath(target_dir)
    print(f"📂 작업 대상 최상위 폴더: {target_dir}\n")
    
    moved_count = 0
    
    # 3. 하위 폴더를 재귀적으로 탐색
    for root, dirs, files in os.walk(target_dir):
        # 최상위 폴더 자체에 있는 파일은 이미 제자리에 있으므로 건너뜀
        if root == target_dir:
            continue
            
        for file in files:
            source_path = os.path.join(root, file)
            dest_path = os.path.join(target_dir, file)
            
            # [안전 장치] 만약 같은 이름의 파일이 이미 최상위에 존재한다면, 이름 뒤에 _1, _2 등을 붙임
            counter = 1
            while os.path.exists(dest_path):
                name, ext = os.path.splitext(file)
                dest_path = os.path.join(target_dir, f"{name}_{counter}{ext}")
                counter += 1
                
            # 파일 이동
            shutil.move(source_path, dest_path)
            print(f"✅ 이동 완료: {file}")
            moved_count += 1

    print(f"\n🎉 총 {moved_count}개의 파일을 성공적으로 이동했습니다!")

if __name__ == "__main__":
    # 터미널에서 경로 인자를 받았는지 확인
    if len(sys.argv) < 2:
        print("💡 사용법: python move_files.py [최상위_폴더_경로]")
        print("💡 예시: python move_files.py ./Engine/Source")
        print("💡 예시: python move_files.py \"C:/BamEngine/Engine/Source\"")
    else:
        # 입력받은 첫 번째 인자(경로)를 함수에 전달
        input_path = sys.argv[1]
        move_all_files_to_root(input_path)