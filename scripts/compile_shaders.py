import os
import subprocess
from pathlib import Path

def compile_shaders(source_dir : Path, output_dir : Path, extra_flags: list):
    if not source_dir.is_dir():
        print(f"Error: Source directory '{source_dir}' does not exist.")
        return

    if not output_dir.exists():
        print(f"Creating output directory '{output_dir}'...")
        output_dir.mkdir(parents=True, exist_ok=True)

    osl_files = list(source_dir.rglob('*.osl'))

    if not osl_files:
        print(f"No .osl files found in '{source_dir}' or its subdirectories.")
        return

    print(f"Scanning for .osl files in: '{source_dir}'")
    print(f"Outputting .oso files to:   '{output_dir}'")

    original_dir = Path.cwd()
    os.chdir(output_dir)

    count = 0

    for file_path in osl_files:
        print("-" * 40)
        print(f"Compiling: {file_path.name}")
        
        try:
            command_str = ['oslc'] + extra_flags + [str(file_path)]
            #print(command_str)
            result = subprocess.run(
                command_str, 
                capture_output=True, 
                text=True
            )
            if result.stdout:
                print(result.stdout.strip())
            
            if result.returncode == 0:
                #print(f"Success: Created {file_path.with_suffix('.oso').name}")
                count += 1
            else:
                print(f"Error: Failed to compile {file_path.name}")
                if result.stderr:
                    print(result.stderr.strip())
                    
        except FileNotFoundError:
            print("Error: 'oslc' command not found. Is it in your system PATH?")
            break 

    os.chdir(original_dir)

    print("-" * 40)
    if count == 0:
        print("No .osl files were compiled successfully.")
    else:
        print(f"Finished! Successfully compiled {count} file(s).")

if __name__ == "__main__":
    source_env = os.environ.get('OSL_SOURCE_DIR', '.')
    
    source_dir = Path(source_env).resolve()

    mk11_root = os.environ.get('MK11_ROOT')
    if mk11_root:
        output_env = Path(mk11_root) / '_env' / 'shaders'
    else:
        output_env = os.environ.get('OSL_OUTPUT_DIR', source_env)
    
    output_dir = Path(output_env).resolve()

    extra_args = ['-O2'] 
    compile_shaders(source_dir, output_dir, extra_args)