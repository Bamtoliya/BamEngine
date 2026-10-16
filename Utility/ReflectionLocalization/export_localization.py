import argparse
import csv
import io
import json
import os
from pathlib import Path
import sys
import tempfile


FIELDS = ("Group", "Key", "English", "Korean")
DEFAULT_METADATA = {"DisplayName", "Category", "Tooltip"}


def collect_keys(paths, metadata_names):
    keys = set()

    for path in paths:
        document = json.loads(path.read_text(encoding="utf-8"))

        if not isinstance(document, dict) or document.get("Version") != 1:
            raise ValueError(f"Unsupported metadata manifest: {path}")

        entries = document.get("Entries")
        if not isinstance(entries, list):
            raise ValueError(f"Entries must be an array: {path}")

        for entry in entries:
            if not isinstance(entry, dict):
                raise ValueError(f"Invalid metadata entry: {path}")

            if entry.get("Metadata") not in metadata_names:
                continue

            key = entry.get("Value")
            owner = entry.get("Owner", "?")

            if not isinstance(key, str):
                raise ValueError(f"Localization metadata must be a string: {path}: {owner}")

            if not key:
                continue

            if key != key.strip() or "\0" in key:
                raise ValueError(f"Invalid localization key: {path}: {owner}: {key!r}")

            keys.add(key)

    return keys


def load_catalogs(directory, base_path):
    owners = {}
    base_rows = {}
    found_base = False

    for path in sorted(directory.glob("*.csv")):
        path = path.resolve()
        is_base = path == base_path
        found_base = found_base or is_base

        try:
            text = path.read_text(encoding="utf-8-sig")
        except UnicodeDecodeError:
            text = path.read_text(encoding="cp949")

        reader = csv.DictReader(io.StringIO(text, newline=""), strict=True)
        headers = reader.fieldnames or []

        if any(headers.count(field) != 1 for field in FIELDS):
            raise ValueError(f"CSV must contain each required column once: {path}")

        unknown = {name for name in headers if name and name not in FIELDS}
        if unknown:
            raise ValueError(f"Unsupported CSV columns: {path}: {sorted(unknown)}")

        current_group = ""

        for row in reader:
            key = (row.get("Key") or "").strip()
            if not key:
                continue

            group = (row.get("Group") or "").strip()
            if group:
                current_group = group

            final_key = f"{current_group}.{key}" if current_group else key

            if final_key in owners:
                previous_path, previous_line = owners[final_key]
                raise ValueError(
                    f"Duplicate key {final_key!r}: "
                    f"{previous_path}:{previous_line}, {path}:{reader.line_num}"
                )

            owners[final_key] = (path, reader.line_num)

            if is_base:
                base_rows[final_key] = {
                    "Group": current_group,
                    "Key": key,
                    "English": row.get("English") or "",
                    "Korean": row.get("Korean") or "",
                }

    if not found_base:
        raise ValueError(f"Base CSV was not found in the catalog directory: {base_path}")

    return owners, base_rows


def write_csv(path, rows):
    stream = io.StringIO(newline="")
    writer = csv.DictWriter(stream, fieldnames=FIELDS, lineterminator="\n")
    writer.writeheader()

    # Ungrouped rows must precede grouped rows because the converter inherits groups.
    writer.writerows(sorted(rows, key=lambda row: (row["Group"], row["Key"])))
    content = stream.getvalue().encode("utf-8-sig")

    if path.exists() and path.read_bytes() == content:
        return False

    path.parent.mkdir(parents=True, exist_ok=True)
    temporary_path = None

    try:
        with tempfile.NamedTemporaryFile(dir=path.parent, mode="wb", delete=False) as temporary:
            temporary_path = Path(temporary.name)
            temporary.write(content)

        os.replace(temporary_path, path)
    finally:
        if temporary_path is not None:
            temporary_path.unlink(missing_ok=True)

    return True


def main():
    parser = argparse.ArgumentParser(description="Merge reflection localization keys into a CSV.")
    parser.add_argument("--input", type=Path, nargs="+", required=True)
    parser.add_argument("--catalog-dir", type=Path, required=True)
    parser.add_argument("--base", default="Engine.csv")
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--in-place", action="store_true", help="Allow updating the base CSV itself.")
    parser.add_argument("--field", action="append", default=[], help="Additional metadata field to export.")
    args = parser.parse_args()

    try:
        catalog_dir = args.catalog_dir.resolve()
        base_path = (catalog_dir / args.base).resolve()
        output_path = args.output.resolve()

        if not catalog_dir.is_dir():
            raise ValueError(f"Catalog directory does not exist: {catalog_dir}")

        if base_path.parent != catalog_dir:
            raise ValueError("--base must name a CSV directly inside --catalog-dir")

        if args.in_place and output_path != base_path:
            raise ValueError("--in-place requires --output to match the base CSV")

        if output_path.is_relative_to(catalog_dir) and not (args.in_place and output_path == base_path):
            raise ValueError("Updating the base CSV requires --in-place; other outputs must be outside the catalog")

        metadata_names = DEFAULT_METADATA | set(args.field)
        keys = collect_keys(args.input, metadata_names)
        owners, rows = load_catalogs(catalog_dir, base_path)
        missing_keys = keys - owners.keys()

        for key in sorted(missing_keys):
            rows[key] = {"Group": "", "Key": key, "English": "", "Korean": ""}

        changed = write_csv(output_path, rows.values())
        reused_elsewhere = sum(key in owners and key not in rows for key in keys)

        print(f"Collected keys: {len(keys)}")
        print(f"New keys: {len(missing_keys)}")
        print(f"Existing keys in other catalogs: {reused_elsewhere}")
        print(f"Output {'updated' if changed else 'unchanged'}: {output_path}")
        return 0
    except (OSError, ValueError, UnicodeError, csv.Error) as error:
        print(f"[ERROR] {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
