import csv
import json
import os

csv_path = 'Resources/Data/Localization.csv'
json_path = 'Resources/Data/Localization.json'

data = {
    "English": {},
    "Korean": {}
}

# CSV 읽기 (인코딩 주의: 엑셀 CSV는 보통 'cp949' 또는 'utf-8-sig')
with open(csv_path, 'r', encoding='utf-8-sig') as f:
    reader = csv.DictReader(f)
    for row in reader:
        key = row['Key']
        
        # 비어있지 않다면 추가
        if row['English']:
            data['English'][key] = row['English']
        if row['Korean']:
            data['Korean'][key] = row['Korean']

# JSON 저장
with open(json_path, 'w', encoding='utf-8') as f:
    json.dump(data, f, indent=4, ensure_ascii=False)

print(f"변환 완료: {json_path}")