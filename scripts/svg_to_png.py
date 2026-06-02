import os
import csv
from glob import glob
import cairosvg
import OpenImageIO as oiio
from lxml import etree

def load_svg_width_table(csv_path):
    table = {}
    with open(csv_path, newline='') as f:
        reader = csv.DictReader(f)
        for row in reader:
            for svg_path in eval(row["svgs"]):
                table[svg_path] = row["size"]
    return table

def print_oiio_info(filepath, label="Output", dpi=72):
    buf = oiio.ImageBuf(filepath)
    spec = buf.spec()
    w, h = spec.width, spec.height
    x_res = spec.getattribute("XResolution") or dpi
    y_res = spec.getattribute("YResolution") or dpi
    print(f"  {label}: {w}x{h}px | {w/x_res:.2f} x {h/y_res:.2f} inches @ {x_res}dpi")

def set_physical_size_inches(filepath, width_inches):
    buf = oiio.ImageBuf(filepath)
    spec = buf.spec()
    px_w, px_h = spec.width, spec.height
    dpi = px_w / width_inches
    height_inches = px_h / dpi
    buf.specmod().attribute("XResolution", dpi)
    buf.specmod().attribute("YResolution", dpi)
    buf.specmod().attribute("ResolutionUnit", 2)
    buf.write(filepath)
    print(f"  Physical size: {width_inches:.2f} x {height_inches:.2f} inches @ {dpi:.1f}dpi")
    print(f"  Pixels unchanged: {px_w}x{px_h}px")

def convert_svgs_to_pngs(input_folder, output_folder, csv_path, base_resolution=2048, dpi=72, fallback_width=1):
    os.makedirs(output_folder, exist_ok=True)
    if not os.path.exists(input_folder):
        raise RuntimeError(f"Input folder does not exist: {input_folder}")

    width_table = load_svg_width_table(csv_path)

    search_path = os.path.join(input_folder, "**/*.svg")
    files = glob(search_path, recursive=True)
    print(f"Found {len(files)} SVG(s) in {input_folder}\n")

    for filename in files:
        filebase = os.path.basename(filename)
        output_path = os.path.join(output_folder, filebase.replace(".svg", ".png"))
        print(f"[{filebase}]")

        raw_width = width_table.get(filename)
        if raw_width is None or raw_width == "null":
            print(f"  No width in CSV, using fallback {fallback_width}in")
            width_inches = fallback_width
        else:
            width_inches = float(raw_width)
            print(f"  Width from CSV: {width_inches}in")

        try:
            cairosvg.svg2png(
                url=filename,
                write_to=output_path,
                output_width=base_resolution
            )
            set_physical_size_inches(output_path, width_inches=width_inches)
            print_oiio_info(output_path, label="Actual PNG ", dpi=dpi)
            print(f"  Saved -> {output_path}")
        except Exception as e:
            print(f"  Failed to convert {filename}: {e}")
        print()

if __name__ == "__main__":
    mk11_root = os.getenv("MK11_ROOT")
    if mk11_root is None:
        raise RuntimeError("MK11_ROOT environment variable is not set.")

    input_folder  = f"{mk11_root}/mk11/mk11-look/look-photoviz/photoviz-logos"
    output_folder = f"{mk11_root}/mk11/mk11-look/look-photoviz/photoviz-logos-pngs"
    csv_path      = f"{mk11_root}/mk11/mk11-look/look-photoviz/photoviz-logos/logos-manufacturing-tracker.csv"
    base_resolution = 4096

    convert_svgs_to_pngs(input_folder, output_folder, csv_path, base_resolution)