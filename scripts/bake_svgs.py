#!/usr/bin/env python3
import os
import subprocess
from pathlib import Path

def bake_svgs(source_dir : Path, output_dir : Path, extra_flags: list):
    if not source_dir.is_dir():
        print(f"Error: Source directory '{source_dir}' does not exist.")
        return

    if not output_dir.exists():
        print(f"Creating output directory '{output_dir}'...")
        output_dir.mkdir(parents=True, exist_ok=True)

    svg_files = list(source_dir.rglob('*.svg'))

    if not svg_files:
        print(f"No .svg files found in '{source_dir}' or its subdirectories.")
        return

    print(f"Scanning for .svg files in: '{source_dir}'")
    print(f"Outputting .tx files to:   '{output_dir}'")

    original_dir = Path.cwd()
    os.chdir(output_dir)

    count = 0

    for file_path in svg_files:
        print("-" * 40)
        print(f"Baking: {file_path.name}")
        
        try:
            #command_str = ['svgtx'] + extra_flags + ["-i", str(file_path)]
            command_str = ["msdfgen", "mtsdf", "-svg", str(file_path), "-autoframe", "-dimensions", "512", "512", "-format", "tiff", "-o", str(file_path) + ".tiff" ]
            #print(command_str)
            result = subprocess.run(
                command_str, 
                capture_output=True, 
                text=True
            )
            if result.stdout:
                print(result.stdout.strip())
            
            if result.returncode == 0:
                #print(f"Success: Created {file_path.with_suffix('.tx').name}")
                count += 1
            else:
                print(f"Error: Failed to bake {file_path.name}")
                if result.stderr:
                    print(result.stderr.strip())
                    
        except FileNotFoundError:
            print("Error: 'svgtx' command not found. Is it in your system PATH?")
            break 

    os.chdir(original_dir)

    print("-" * 40)
    if count == 0:
        print("No .svg files were baked successfully.")
    else:
        print(f"Finished! Successfully baked {count} file(s).")

if __name__ == "__main__":
    source_env = os.environ.get('SVG_SOURCE_DIR', '.')
    
    mk11_root = os.environ.get('MK11_ROOT')

    if mk11_root:
        input_env = Path(mk11_root) / "mk11" / "mk11-look" / "look-photoviz" / "photoviz-logos"
    else:
        input_env = os.environ.get('SVG_INPUT_DIR', source_env)
    
    input_dir = Path(input_env).resolve()

    if mk11_root:
        output_env = Path(mk11_root) / "mk11" / "mk11-look" / "look-photoviz" / "photoviz-logos" / "baked-msdf"
    else:
        output_env = os.environ.get('SVG_OUTPUT_DIR', source_env)
    
    output_dir = Path(output_env).resolve()

    #extra_args = ['--width', '4096'] 
    extra_args = [] 
    bake_svgs(input_dir, output_dir, extra_args)