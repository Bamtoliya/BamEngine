import csv
import io
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


REPOSITORY = Path(__file__).resolve().parents[2]
EXPORTER = Path(__file__).with_name("export_localization.py")
CONVERTER = REPOSITORY / "Utility/CSVToJsonConverter/localizationcsvtojson.py"
FIELDS = ("Group", "Key", "English", "Korean")


class LocalizationExportTests(unittest.TestCase):
    def setUp(self):
        workspace = REPOSITORY / "out"
        workspace.mkdir(exist_ok=True)
        self.temporary = tempfile.TemporaryDirectory(prefix="localization-test-", dir=workspace)
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.catalog = self.root / "catalog"
        self.catalog.mkdir()
        self.manifest = self.root / "metadata.json"
        self.output = self.root / "review/Engine.csv"

    def write_catalog(self, name, rows, encoding="utf-8-sig"):
        stream = io.StringIO(newline="")
        writer = csv.writer(stream)
        writer.writerow(FIELDS)
        writer.writerows(rows)
        (self.catalog / name).write_text(stream.getvalue(), encoding=encoding, newline="")

    def write_manifest(self, entries):
        document = {"Version": 1, "Module": "Test", "Entries": entries}
        self.manifest.write_text(json.dumps(document, ensure_ascii=False), encoding="utf-8")

    def run_export(self, *extra):
        command = [
            sys.executable, "-B", str(EXPORTER), "--input", str(self.manifest),
            "--catalog-dir", str(self.catalog), "--output", str(self.output), *extra,
        ]
        return subprocess.run(command, capture_output=True, encoding="utf-8", errors="replace")

    def read_output(self):
        with self.output.open(encoding="utf-8-sig", newline="") as source:
            return list(csv.DictReader(source))

    def test_preserves_translations_and_sources_deduplicates_keys_and_avoids_rewrites(self):
        self.write_catalog("Engine.csv", [("", "EXISTING", "Position", "위치"), ("", "OLD", "Old", "이전")])
        self.write_catalog("Editor.csv", [("", "SHARED", "Shared", "공유")])
        self.write_manifest([
            {"Metadata": name, "Value": key}
            for name, key in [
                ("DisplayName", "EXISTING"), ("Category", "NEW"), ("DisplayName", "NEW"),
                ("Tooltip", "SHARED"), ("Units", "METRES"),
            ]
        ])
        original = {path: path.read_bytes() for path in self.catalog.glob("*.csv")}
        result = self.run_export("--field", "Units")
        self.assertEqual(result.returncode, 0, result.stderr)
        rows = {row["Key"]: row for row in self.read_output()}
        self.assertEqual(set(rows), {"EXISTING", "OLD", "NEW", "METRES"})
        self.assertEqual(rows["EXISTING"]["Korean"], "위치")
        self.assertEqual(rows["NEW"]["English"], "")
        self.assertEqual(original, {path: path.read_bytes() for path in original})
        modified = self.output.stat().st_mtime_ns
        result = self.run_export("--field", "Units")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(self.output.stat().st_mtime_ns, modified)

    def test_groups_and_quoted_multiline_keys_work_with_existing_converter(self):
        self.write_catalog("Engine.csv", [("Menu", "OPEN", "Open", "열기"), ("", "CLOSE", "Close", "닫기")])
        key = 'Tooltip, "quoted"\n두 번째 줄'
        self.write_manifest([{"Metadata": "Tooltip", "Value": key}])
        result = self.run_export()
        self.assertEqual(result.returncode, 0, result.stderr)
        rows = self.read_output()
        self.assertEqual(rows[0]["Key"], key)
        rows[0].update(English="New tooltip", Korean="새 툴팁")
        with self.output.open("w", encoding="utf-8-sig", newline="") as destination:
            writer = csv.DictWriter(destination, fieldnames=FIELDS)
            writer.writeheader()
            writer.writerows(rows)
        result = subprocess.run(
            [sys.executable, str(CONVERTER), "--input", str(self.output), "--output", str(self.root / "json")],
            capture_output=True, encoding="utf-8", errors="replace",
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        korean = json.loads((self.root / "json/Korean.json").read_text(encoding="utf-8"))["Korean"]
        self.assertEqual(korean[key], "새 툴팁")
        self.assertEqual(korean["Menu.CLOSE"], "닫기")

    def test_duplicate_catalog_key_preserves_existing_output(self):
        self.write_catalog("Engine.csv", [("", "DUPLICATE", "Original", "원본")])
        self.write_catalog("Editor.csv", [("", "DUPLICATE", "Other", "다른 항목")])
        self.write_manifest([])
        self.output.parent.mkdir()
        self.output.write_bytes(b"existing output")
        result = self.run_export()
        self.assertEqual(result.returncode, 1)
        self.assertIn("Duplicate key", result.stderr)
        self.assertEqual(self.output.read_bytes(), b"existing output")

    def test_invalid_metadata_preserves_existing_output(self):
        self.write_catalog("Engine.csv", [])
        self.write_manifest([{"Metadata": "DisplayName", "Value": 42}])
        self.output.parent.mkdir()
        self.output.write_bytes(b"existing output")
        result = self.run_export()
        self.assertEqual(result.returncode, 1)
        self.assertIn("must be a string", result.stderr)
        self.assertEqual(self.output.read_bytes(), b"existing output")

    def test_in_place_merge_preserves_translations_and_is_idempotent(self):
        self.write_catalog("Engine.csv", [("", "NAME", "Name", "이름")])
        self.write_catalog("Editor.csv", [("", "SHARED", "Shared", "공유")])
        self.write_manifest([
            {"Metadata": "DisplayName", "Value": "NEW"},
            {"Metadata": "Category", "Value": "SHARED"},
        ])
        self.output = self.catalog / "Engine.csv"
        other_catalog = (self.catalog / "Editor.csv").read_bytes()
        result = self.run_export("--in-place")
        self.assertEqual(result.returncode, 0, result.stderr)
        rows = {row["Key"]: row for row in self.read_output()}
        self.assertEqual(set(rows), {"NAME", "NEW"})
        self.assertEqual(rows["NAME"]["English"], "Name")
        self.assertEqual(rows["NAME"]["Korean"], "이름")
        self.assertEqual(rows["NEW"]["Korean"], "")
        self.assertEqual((self.catalog / "Editor.csv").read_bytes(), other_catalog)
        modified = self.output.stat().st_mtime_ns
        result = self.run_export("--in-place")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(self.output.stat().st_mtime_ns, modified)

    def test_in_place_rejects_a_different_catalog(self):
        self.write_catalog("Engine.csv", [("", "NAME", "Name", "이름")])
        self.write_catalog("Editor.csv", [("", "SHARED", "Shared", "공유")])
        self.write_manifest([])
        other = self.catalog / "Editor.csv"
        original = other.read_bytes()
        result = self.run_export("--in-place", "--output", str(other))
        self.assertEqual(result.returncode, 1)
        self.assertEqual(other.read_bytes(), original)

    def test_cp949_catalog_and_source_output_protection(self):
        self.write_catalog("Engine.csv", [("", "NAME", "Name", "이름")], encoding="cp949")
        self.write_manifest([])
        result = self.run_export()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(self.read_output()[0]["Korean"], "이름")
        source = self.catalog / "Engine.csv"
        original = source.read_bytes()
        result = self.run_export("--output", str(source))
        self.assertEqual(result.returncode, 1)
        self.assertEqual(source.read_bytes(), original)


if __name__ == "__main__":
    unittest.main()
