import os
import struct
import subprocess
import sys
import zlib
from pathlib import Path
from tempfile import TemporaryDirectory


def _png_chunk(kind, data):
    return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF)


def _write_test_png(path, width=4, height=4):
    pixels = b"".join(b"\x00" + bytes((255, 0, 0, 255) * width) for _ in range(height))
    data = b"\x89PNG\r\n\x1a\n"
    data += _png_chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0))
    data += _png_chunk(b"IDAT", zlib.compress(pixels))
    data += _png_chunk(b"IEND", b"")
    path.write_bytes(data)


def main():
    worker = Path(sys.argv[1]).resolve()
    output_check = Path(sys.argv[2]).resolve()
    with TemporaryDirectory(prefix="BamAssetWorkerTexture-") as directory:
        root = Path(directory)
        output_dir = root / "output"
        output_dir.mkdir()
        source = root / "texture.png"
        staged_bamtex = output_dir / "texture-stage.bamtex"
        staged_dds = output_dir / "texture-stage.dds"
        cached_bamtex = output_dir / "cached-bam.tmp"
        cached_dds = output_dir / "cached-dds.tmp"
        second_source = root / "texture-second.png"
        second_bamtex = output_dir / "second-stage.bamtex"
        second_dds = output_dir / "second-stage.dds"
        no_mip_bamtex = output_dir / "no-mip-stage.bamtex"
        no_mip_dds = output_dir / "no-mip-stage.dds"
        rgba_bamtex = output_dir / "rgba-stage.bamtex"
        rgba_dds = output_dir / "rgba-stage.dds"
        fast_bamtex = output_dir / "fast-stage.bamtex"
        fast_dds = output_dir / "fast-stage.dds"
        fast_hit_bamtex = output_dir / "fast-hit.bamtex"
        fast_hit_dds = output_dir / "fast-hit.dds"
        gpu_bamtex = output_dir / "gpu-stage.bamtex"
        gpu_dds = output_dir / "gpu-stage.dds"
        gpu_hit_bamtex = output_dir / "gpu-hit.bamtex"
        gpu_hit_dds = output_dir / "gpu-hit.dds"
        final_bamtex = output_dir / "texture.bamtex"
        final_dds = output_dir / "texture.dds"
        rejected_bamtex = output_dir / "missing-stage.bamtex"
        rejected_dds = output_dir / "missing-stage.dds"
        _write_test_png(source)
        _write_test_png(second_source, 7, 5)

        commands = [
            "TEXTURE_WORKER_VERSION\t2",
            f"IMPORT_TEXTURE\t1\t{source}\t{final_bamtex}\t{final_dds}\t{staged_bamtex}\t{staged_dds}\t18\t11",
            f"IMPORT_TEXTURE\t2\t{source}\t{final_bamtex}\t{final_dds}\t{cached_bamtex}\t{cached_dds}\t18\t11",
            f"IMPORT_TEXTURE\t3\t{second_source}\t{final_bamtex}\t{final_dds}\t{second_bamtex}\t{second_dds}\t17\t67",
            f"IMPORT_TEXTURE\t4\t{second_source}\t{final_bamtex}\t{final_dds}\t{no_mip_bamtex}\t{no_mip_dds}\t18\t10",
            f"IMPORT_TEXTURE\t5\t{second_source}\t{final_bamtex}\t{final_dds}\t{rgba_bamtex}\t{rgba_dds}\t4\t1",
            f"IMPORT_TEXTURE\t6\t{source}\t{final_bamtex}\t{final_dds}\t{fast_bamtex}\t{fast_dds}\t18\t139",
            f"IMPORT_TEXTURE\t7\t{source}\t{final_bamtex}\t{final_dds}\t{fast_hit_bamtex}\t{fast_hit_dds}\t18\t139",
            f"IMPORT_TEXTURE\t8\t{source}\t{final_bamtex}\t{final_dds}\t{gpu_bamtex}\t{gpu_dds}\t18\t15",
            f"IMPORT_TEXTURE\t9\t{source}\t{final_bamtex}\t{final_dds}\t{gpu_hit_bamtex}\t{gpu_hit_dds}\t18\t15",
            f"IMPORT_TEXTURE\t10\t{root / 'missing.png'}\t{final_bamtex}\t{final_dds}\t{rejected_bamtex}\t{rejected_dds}\t18\t11",
            f"IMPORT_TEXTURE\t11\t{source}\t{final_bamtex}\t{final_dds}\t{rejected_bamtex}\t{rejected_dds}\t18\t143",
            "QUIT",
        ]
        run = subprocess.run(
            [str(worker), "--daemon", "--project-root", str(root)],
            cwd=root,
            input="\n".join(commands) + "\n",
            encoding="utf-8",
            errors="replace",
            capture_output=True,
            timeout=120,
        )
        responses = run.stdout.splitlines()
        required = ["TEXTURE_WORKER_VERSION\t2"]
        for request_id in (1, 2, 3, 4, 5, 6, 7, 8, 9):
            required.extend(f"PROGRESS\t{request_id}\t{step}" for step in (1, 2, 3, 4))
            required.append(f"RESULT\t{request_id}\t1")
        required.extend(("RESULT\t10\t0", "RESULT\t11\t0"))
        actual = [line for line in responses if line in required]
        if run.returncode != 0 or actual != required:
            raise RuntimeError(f"Unexpected worker response: {run.returncode}\n{run.stdout}\n{run.stderr}")
        if "BACKEND\t1\t0\t0" not in responses or "BACKEND\t2\t4\t0" not in responses or \
                "BACKEND\t6\t1\t0" not in responses or "BACKEND\t7\t5\t0" not in responses or \
                not any(line.startswith(("BACKEND\t8\t2\t", "BACKEND\t8\t3\t")) for line in responses):
            raise RuntimeError(f"Unexpected compression backend or cache result:\n{run.stdout}\n{run.stderr}")
        if "BACKEND\t8\t2\t0" in responses:
            if "BACKEND\t9\t6\t0" not in responses:
                raise RuntimeError(f"GPU result was not cached:\n{run.stdout}\n{run.stderr}")
        elif not any(line.startswith(("BACKEND\t9\t2\t", "BACKEND\t9\t3\t")) for line in responses):
            raise RuntimeError(f"CPU fallback was incorrectly cached as GPU:\n{run.stdout}\n{run.stderr}")
        outputs = ((staged_bamtex, staged_dds, "BC7SRGB", 4, 4, 3),
                   (cached_bamtex, cached_dds, "BC7SRGB", 4, 4, 3),
                   (second_bamtex, second_dds, "BC5", 7, 5, 3),
                   (no_mip_bamtex, no_mip_dds, "BC7SRGB", 7, 5, 1),
                   (rgba_bamtex, rgba_dds, "RGBA", 7, 5, 3),
                   (fast_bamtex, fast_dds, "BC7SRGB", 4, 4, 3),
                   (fast_hit_bamtex, fast_hit_dds, "BC7SRGB", 4, 4, 3),
                   (gpu_bamtex, gpu_dds, "BC7SRGB", 4, 4, 3),
                   (gpu_hit_bamtex, gpu_hit_dds, "BC7SRGB", 4, 4, 3))
        for bamtex, dds, format_name, width, height, mips in outputs:
            if not bamtex.is_file() or bamtex.stat().st_size <= 8:
                raise RuntimeError(f"Successful texture conversion must produce a BAM texture staging file: {bamtex}")
            if not dds.is_file() or dds.read_bytes()[:4] != b"DDS ":
                raise RuntimeError(f"Successful texture conversion must produce a valid DDS staging file: {dds}")
            validation = subprocess.run(
                [str(output_check), str(bamtex), str(dds), format_name, str(width), str(height), str(mips)],
                capture_output=True, timeout=10,
            )
            if validation.returncode != 0:
                raise RuntimeError(f"BAM texture and DDS differ: {validation.stderr.decode(errors='replace')}")
        if final_bamtex.exists() or final_dds.exists() or rejected_bamtex.exists() or rejected_dds.exists():
            raise RuntimeError("Worker must stage outputs and leave no outputs for a failed conversion.")

        # A cache hit must not decode the original image again, including when writing DDS.
        original = source.read_bytes()
        original_stat = source.stat()
        source.write_bytes(bytes(len(original)))
        os.utime(source, ns=(original_stat.st_atime_ns, original_stat.st_mtime_ns))
        if source.stat().st_mtime_ns != original_stat.st_mtime_ns:
            raise RuntimeError("Could not preserve source timestamp for cache-hit validation.")
        hit_bamtex = output_dir / "hit-stage.bamtex"
        hit_dds = output_dir / "hit-stage.dds"
        hit_commands = [
            f"IMPORT_TEXTURE\t1\t{source}\t{final_bamtex}\t{final_dds}\t{hit_bamtex}\t{hit_dds}\t18\t11",
            "QUIT",
        ]
        hit = subprocess.run(
            [str(worker), "--daemon", "--project-root", str(root)], cwd=root,
            input="\n".join(hit_commands) + "\n", encoding="utf-8", errors="replace",
            capture_output=True, timeout=45,
        )
        if hit.returncode != 0 or "RESULT\t1\t1" not in hit.stdout.splitlines():
            raise RuntimeError(f"Cached conversion failed: {hit.returncode}\n{hit.stdout}\n{hit.stderr}")
        if hit_dds.read_bytes() != staged_dds.read_bytes():
            raise RuntimeError("Cached DDS differs from the original output.")
        validation = subprocess.run(
            [str(output_check), str(hit_bamtex), str(hit_dds), "BC7SRGB", "4", "4", "3"],
            capture_output=True, timeout=10,
        )
        if validation.returncode != 0:
            raise RuntimeError(f"Cached outputs differ: {validation.stderr.decode(errors='replace')}")

        source.write_bytes(original)
        os.utime(source, ns=(original_stat.st_atime_ns, original_stat.st_mtime_ns))
        cache_files = list((root / ".bam" / "cache" / "ddc" / "textures").glob("*.ddc"))
        if not cache_files:
            raise RuntimeError("No texture cache files were created.")
        for cache_file in cache_files:
            cache_file.write_bytes(cache_file.read_bytes()[:-1])
        repaired = subprocess.run(
            [str(worker), "--daemon", "--project-root", str(root)], cwd=root,
            input="\n".join(hit_commands) + "\n", encoding="utf-8", errors="replace",
            capture_output=True, timeout=45,
        )
        if repaired.returncode != 0 or "RESULT\t1\t1" not in repaired.stdout.splitlines():
            raise RuntimeError(f"Conversion after cache corruption failed: {repaired.stdout}\n{repaired.stderr}")
        source.write_bytes(bytes(len(original)))
        os.utime(source, ns=(original_stat.st_atime_ns, original_stat.st_mtime_ns))
        repaired_hit = subprocess.run(
            [str(worker), "--daemon", "--project-root", str(root)], cwd=root,
            input="\n".join(hit_commands) + "\n", encoding="utf-8", errors="replace",
            capture_output=True, timeout=45,
        )
        if repaired_hit.returncode != 0 or "RESULT\t1\t1" not in repaired_hit.stdout.splitlines():
            raise RuntimeError(f"Repaired cache could not be reused: {repaired_hit.stdout}\n{repaired_hit.stderr}")


if __name__ == "__main__":
    main()
