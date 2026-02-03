
import argparse
import os
import subprocess
import sys
from pathlib import Path
from typing import List, Optional

# color codes
RED = '\033[91m'
RESET = '\033[0m'


def print_error(message: str):
    print(f"{RED}{message}{RESET}", file=sys.stderr)


def find_osl_files(src_dir: Path) -> List[Path]:
    return list(src_dir.rglob("*.osl"))


def compile_osl_file(
    osl_file: Path,
    src_dir: Path,
    output_dir: Path,
    include_paths: List[str],
    no_flatten: bool,
    extra_args: List[str],
    verbose: bool,
    quiet: bool,
) -> bool:
    if no_flatten:
        relative_path = osl_file.relative_to(src_dir)
        output_file = output_dir / relative_path.with_suffix(".oso")
    else:
        output_file = output_dir / f"{osl_file.stem}.oso"

    output_file.parent.mkdir(parents=True, exist_ok=True)
    
    cmd = ["oslc"]
    
    for include_path in include_paths:
        cmd.append(f"-I{include_path}")
    
    cmd.extend(["-o", str(output_file)])

    cmd.extend(extra_args)
    
    cmd.append(str(osl_file))
    
    if verbose and not quiet:
        print(f"Compiling: {osl_file}")
        print(f"Command: {' '.join(cmd)}")
    elif not quiet:
        print(f"Compiling: {osl_file}")
    
    try:
        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            check=False
        )
        
        if result.returncode != 0:
            if not quiet:
                print_error(f"ERROR compiling {osl_file}:")
                print(result.stderr, file=sys.stderr)
            return False
        
        if verbose and result.stdout and not quiet:
            print(result.stdout)
        
        if not quiet:
            print(f"  -> {output_file}")
        
        return True
    
    except FileNotFoundError:
        print_error("ERROR: oslc compiler not found. Make sure it's in your PATH.")
        sys.exit(1)
    except Exception as e:
        if not quiet:
            print_error(f"ERROR compiling {osl_file}: {e}")
        return False


def main():
    parser = argparse.ArgumentParser(
        description="Batch compile Open Shading Language files",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Compile all .osl files from src/ to oso/
  %(prog)s
  
  # Compile with custom directories
  %(prog)s --src shaders --output build/oso --include headers
  
  # Flatten output directory structure
  %(prog)s --flatten
  
  # Pass through oslc options
  %(prog)s -O2 -Werror --embed-source
  
  # Verbose mode with optimization
  %(prog)s -v -O2
        """
    )
    
    # Directory options
    parser.add_argument(
        "--src",
        type=str,
        default="src",
        help="Source directory containing .osl files (default: src)"
    )
    
    parser.add_argument(
        "--output",
        type=str,
        default="oso",
        help="Output directory for compiled .oso files (default: oso)"
    )
    
    parser.add_argument(
        "--no_flatten",
        action="store_true",
        help="Flatten output directory structure (all .oso files in output root) as opposed to maintaing the oringinal structure in src"
    )
    
    # oslc passthrough options
    parser.add_argument(
        "-I",
        action="append",
        dest="include_paths",
        default=["include"],
        help="Add path to #include search path"
    )

    parser.add_argument(
        "-v", "--verbose",
        action="store_true",
        help="Verbose mode"
    )
    
    parser.add_argument(
        "-q", "--quiet",
        action="store_true",
        help="Quiet mode"
    )
    
    parser.add_argument(
        "-D",
        action="append",
        dest="defines",
        default=[],
        help="Define preprocessor symbol (e.g., -DDEBUG or -DVALUE=1)"
    )
    
    parser.add_argument(
        "-U",
        action="append",
        dest="undefines",
        default=[],
        help="Undefine preprocessor symbol"
    )
    
    parser.add_argument(
        "-O",
        type=str,
        choices=["0", "1", "2"],
        dest="optimization",
        help="Set optimization level (0, 1, or 2)"
    )
    
    parser.add_argument(
        "-d", "--debug",
        action="store_true",
        help="Debug mode"
    )
    
    parser.add_argument(
        "-E", "--preprocess-only",
        action="store_true",
        help="Only preprocess the input"
    )
    
    parser.add_argument(
        "-Werror",
        action="store_true",
        dest="warnings_as_errors",
        help="Treat all warnings as errors"
    )
    
    parser.add_argument(
        "--embed-source",
        action="store_true",
        help="Embed preprocessed source in the oso file"
    )
    
    parser.add_argument(
        "--buffer",
        action="store_true",
        help="Force compile from buffer (debugging)"
    )
    
    parser.add_argument(
        "-MD",
        action="store_true",
        dest="write_depfile",
        help="Write a depfile containing headers used"
    )
    
    parser.add_argument(
        "-MMD",
        action="store_true",
        dest="write_depfile_no_system",
        help="Write a depfile, excluding system headers"
    )
    
    parser.add_argument(
        "-M",
        action="store_true",
        dest="depfile_stdout",
        help="Like -MD, but write depfile to stdout"
    )
    
    parser.add_argument(
        "-MM",
        action="store_true",
        dest="depfile_stdout_no_system",
        help="Like -MMD, but write depfile to stdout"
    )
    
    parser.add_argument(
        "-MF",
        type=str,
        dest="depfile_name",
        help="Specify the name of the depfile to output"
    )
    
    parser.add_argument(
        "-MT",
        type=str,
        dest="depfile_target",
        help="Specify a custom dependency target name"
    )
    
    args = parser.parse_args()
    
    src_dir = Path(args.src)
    output_dir = Path(args.output)
    
    if not src_dir.exists():
        print_error(f"ERROR: Source directory '{src_dir}' does not exist")
        sys.exit(1)
    
    if not src_dir.is_dir():
        print_error(f"ERROR: Source path '{src_dir}' is not a directory")
        sys.exit(1)
    
    osl_files = find_osl_files(src_dir)
    
    if not osl_files:
        print(f"No .osl files found in {src_dir}", file=sys.stderr)
        sys.exit(1)
    
    if not args.quiet:
        print(f"Found {len(osl_files)} .osl file(s) in {src_dir}")
        print(f"Output directory: {output_dir}")
        if args.include_paths:
            print(f"Include paths: {', '.join(args.include_paths)}")
        if not args.no_flatten:
            print("Output structure: FLATTENED")
        else:
            print("Output structure: PRESERVED")
        print()
    
    extra_args = []
    
    for define in args.defines:
        extra_args.append(f"-D{define}")
    
    for undefine in args.undefines:
        extra_args.append(f"-U{undefine}")
    
    if args.optimization:
        extra_args.append(f"-O{args.optimization}")
    
    if args.debug:
        extra_args.append("-d")
    
    if args.preprocess_only:
        extra_args.append("-E")
    
    if args.warnings_as_errors:
        extra_args.append("-Werror")
    
    if args.embed_source:
        extra_args.append("-embed-source")
    
    if args.buffer:
        extra_args.append("-buffer")
    
    if args.write_depfile:
        extra_args.append("-MD")
    
    if args.write_depfile_no_system:
        extra_args.append("-MMD")
    
    if args.depfile_stdout:
        extra_args.append("-M")
    
    if args.depfile_stdout_no_system:
        extra_args.append("-MM")
    
    if args.depfile_name:
        extra_args.extend(["-MF", args.depfile_name])
    
    if args.depfile_target:
        extra_args.extend(["-MT", args.depfile_target])
    
    if args.quiet:
        extra_args.append("-q")
    
    #compile all the files
    success_count = 0
    failed_files = []
    
    for osl_file in osl_files:
        if compile_osl_file(
            osl_file,
            src_dir,
            output_dir,
            args.include_paths,
            args.no_flatten,
            extra_args,
            args.verbose,
            args.quiet,
        ):
            success_count += 1
        else:
            failed_files.append(osl_file)
    
    # Print summary
    if not args.quiet:
        print()
        print(f"Compilation complete: {success_count}/{len(osl_files)} succeeded")
        
        if failed_files:
            print(f"\nFailed files:")
            for failed_file in failed_files:
                print(f"  - {failed_file}")
            sys.exit(1)
    
    sys.exit(0 if success_count == len(osl_files) else 1)


if __name__ == "__main__":
    main()