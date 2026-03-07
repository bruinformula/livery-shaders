import os
import glob
import sys
import platform
import subprocess
import difflib
import filecmp
import shutil
from optparse import OptionParser
from typing import Optional


def make_relpath(path: str, start: str = os.curdir) -> str:
    p = os.path.relpath(path, start)
    return p if sys.platform != "Windows" else p.replace("\\", "/")


srcdir: str = "."
tmpdir: str = os.path.abspath(".")

AUTOLIB_TESTSWEET_DIR: str      = os.getenv("AUTOLIB_TESTSWEET_DIR", ".")
OpenImageIO_ROOT: Optional[str] = os.environ.get("OpenImageIO_ROOT", None)

parser = OptionParser()
parser.add_option("-p", "--path", dest="path", default="")
(options, args) = parser.parse_args()

if args and len(args) > 0:
    srcdir = os.path.abspath(args[0]) + "/"
if args and len(args) > 1:
    AUTOLIB_BUILD_DIR: str = os.path.normpath(args[1])

test_source_dir: str = os.path.abspath(srcdir)
print("test source dir = ", test_source_dir)

# Create symlinks in the build working dir 

if not os.path.lexists(os.path.join(tmpdir, "ref")):
    ref_src = os.path.join(test_source_dir, "ref")
    if os.path.exists(ref_src):
        os.symlink(ref_src, os.path.join(tmpdir, "ref"))
if not os.path.lexists(os.path.join(tmpdir, "data")):
    os.symlink(test_source_dir, os.path.join(tmpdir, "data"))

os.chdir(srcdir)

refdir:           str            = "ref/"
outputs:          list[str]      = ["out.txt"]
failthresh:       float          = 0.004
hardfail:         float          = 0.01
failpercent:      float          = 0.02
failureok:        int            = 0
filter_re:        Optional[str]  = None
skip_diff:        int            = int(os.environ.get("Usd_TESTSUITE_SKIP_DIFF", "0"))
image_extensions: list[str]      = [".tif", ".tx", ".exr", ".jpg", ".png"]


def oiio_app(app: str) -> str:
    if OpenImageIO_ROOT:
        return os.path.join(OpenImageIO_ROOT, "bin", app) + " "
    return app + " "


def oiiodiff(
    fileA: str, 
    fileB: str, 
    extraargs: str = "",
    silent: bool = True, 
    concat: bool = True
) -> str:
    threshargs = (f" -fail {failthresh} -failpercent {failpercent}"
                  f" -hardfail {hardfail} -warn {2*failthresh}"
                  f" -warnpercent {failpercent}")
    cmd = (oiio_app("oiiotool") + "-a" + threshargs
           + f" {extraargs} {os.path.abspath(fileA)}"
           + f" {os.path.abspath(fileB)}"
           + (" --diff"))
    if concat:
        cmd += " ;\n"
    return cmd

def text_diff(
    fromfile: str, 
    tofile: str,
    diff_file: Optional[str] = None,
    filter_re: Optional[str] = None
) -> int:
    try:
        fromlines = open(fromfile).readlines()
        tolines   = open(tofile).readlines()
    except Exception:
        return -1
    diff_lines = list(difflib.unified_diff(fromlines, tolines, fromfile, tofile))
    if not diff_lines:
        return 0
    if diff_file:
        open(diff_file, "w").writelines(diff_lines)
    return 1


def render(
    scene: str, 
    output: str, 
    renderer: str = "Arnold",
    render_settings: str = "/World/Render/RenderSettings",
    camera: Optional[str] = None
) -> str:
    scene_abs:  str = os.path.join(test_source_dir, scene)
    output_abs: str = os.path.join(tmpdir, output)

    cmd: list[str] = [
        "usdrecord",
        "--renderer",               renderer,
        "--renderSettingsPrimPath", render_settings,
        "--colorCorrectionMode",    "disabled",
        "--disableCameraLight"
    ]
    if camera:
        cmd += ["--camera", camera]
    cmd += [scene_abs, output_abs]

    print("CMD:", " ".join(cmd))

    out_txt = os.path.join(tmpdir, "out.txt")
    with open(out_txt, "w") as f:
        f.write(f"CMD: {' '.join(cmd)}\n")
        result = subprocess.run(cmd, cwd=tmpdir, stdout=f, stderr=subprocess.STDOUT)

    if result.returncode != 0:
        raise RuntimeError(f"usdrecord failed: renderer={renderer}")

    return output

def runtest(outputs: list[str], failureok: int = 0) -> int:
    if skip_diff:
        return 0

    err: int = 0
    for out in outputs:
        ext:       str       = os.path.splitext(out)[1]
        ok:        int       = 0
        out_abs:   str       = os.path.join(tmpdir, out)
        testfiles: list[str] = (
            [os.path.join(tmpdir, "ref", out)]
            + glob.glob(os.path.join(tmpdir, "ref", f"*{ext}"))
        )
        for testfile in testfiles:
            if ext in (".tif", ".exr"):
                cmpresult = os.system(oiiodiff(out_abs, testfile, concat=False, silent=True))
            elif ext == ".txt":
                cmpresult = text_diff(out_abs, testfile, out_abs + ".diff", filter_re=filter_re)
            else:
                cmpresult = 0 if filecmp.cmp(out_abs, testfile) else 1
            if cmpresult == 0:
                ok = 1
                break

        if ok:
            print("PASS:", out, "matches", testfile)
        else:
            err = 1
            print("FAIL:", out)
            if ext in (".tif", ".exr", ".jpg", ".png"):
                os.system(oiiodiff(out_abs, os.path.join(tmpdir, refdir, out), silent=False))

    return err


with open(os.path.join(test_source_dir, "run.py")) as f:
    exec(compile(f.read(), "run.py", "exec"))

for candidate in ["out.exr", "out-arnold.exr", "out-renderman.exr", "out-cycles.exr"]:
    ref_candidate: str = os.path.join(tmpdir, "ref", candidate)
    if os.path.exists(ref_candidate) and candidate not in outputs:
        outputs.append(candidate)

ret: int = runtest(outputs, failureok=failureok)
sys.exit(ret)