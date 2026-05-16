import os
import subprocess
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor, as_completed

def bake_svg(file_path: Path, extra_flags: list) -> tuple[Path, bool]:
    try:
        command_str = ['svgtx'] + extra_flags + ["-i", file_path.name]
        result = subprocess.run(
            command_str,
            capture_output=True,
            text=True,
            cwd=file_path.parent
        )

        if result.stdout:
            print(f"[{file_path.name}] {result.stdout.strip()}")

        if result.returncode == 0:
            return file_path, True
        else:
            print(f"Error: Failed to bake {file_path.name}")
            if result.stderr:
                print(f"[{file_path.name}] {result.stderr.strip()}")
            return file_path, False

    except FileNotFoundError:
        print("Error: 'svgtx' command not found. Is it in your system PATH?")
        return file_path, False


def bake_svgs(source_dir: Path, extra_flags: list, max_workers: int = 8):
    if not source_dir.is_dir():
        print(f"Error: Source directory '{source_dir}' does not exist.")
        return

    svg_files = list(source_dir.rglob('*.svg'))
    if not svg_files:
        print(f"No .svg files found in '{source_dir}' or its subdirectories.")
        return

    print(f"Scanning for .svg files in: '{source_dir}'")
    print(f"Found {len(svg_files)} file(s). Baking with {max_workers} workers...\n")

    count = 0
    with ThreadPoolExecutor(max_workers=max_workers) as executor:
        futures = {executor.submit(bake_svg, f, extra_flags): f for f in svg_files}

        for future in as_completed(futures):
            file_path, success = future.result()
            print("-" * 40)
            print(f"{'✓' if success else '✗'} {file_path.name}")
            if success:
                count += 1

    print("-" * 40)
    if count == 0:
        print("No .svg files were baked successfully.")
    else:
        print(f"Finished! Successfully baked {count}/{len(svg_files)} file(s).")


if __name__ == "__main__":
    source_env = os.environ.get('SVG_SOURCE_DIR', '.')
    mk11_root = os.environ.get('MK11_ROOT')

    if mk11_root:
        input_env = Path(mk11_root) / "mk11" / "mk11-look" / "look-photoviz" / "photoviz-logos"
    else:
        input_env = os.environ.get('SVG_INPUT_DIR', source_env)

    input_dir = Path(input_env).resolve()
    extra_args = []
    bake_svgs(input_dir, extra_args, max_workers=8)