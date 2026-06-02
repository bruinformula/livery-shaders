from pxr import Usd, UsdShade, Sdf
from glob import glob
import csv
import os
import sys

def find_autolib_decal_shaders(stage_path: str) -> list[dict]:
    stage = Usd.Stage.Open(stage_path)
    if not stage:
        raise RuntimeError(f"Failed to open stage: {stage_path}")

    results = []
    for prim in stage.Traverse():
        if not prim.IsA(UsdShade.Shader):
            continue

        shader = UsdShade.Shader(prim)
        info_id_attr = prim.GetAttribute("info:id")
        if not info_id_attr or not info_id_attr.IsValid():
            continue

        info_id = info_id_attr.Get()
        if info_id != "arnold:AutolibDecal":
            continue

        filename_input = shader.GetInput("Filename")
        filename_path = None
        svgs = []

        if filename_input and filename_input.GetAttr().IsValid():
            attr = filename_input.GetAttr()
            asset_val = attr.Get()
            if asset_val is not None:
                filename_path = asset_val.resolvedPath or asset_val.path
                parent_dir = os.path.dirname(filename_path)
                svgs = glob(os.path.join(parent_dir, "*.svg"))

        results.append({
            "svgs":      svgs,
            "size":      "null",
            "prim_path": str(prim.GetPath()),
            "filename":  filename_path,
        })

    return results


if __name__ == "__main__":

    mk11_root = os.getenv("MK11_ROOT")
    if mk11_root is None:
        raise RuntimeError("MK11_ROOT environment variable is not set.")

    stage_file  = f"{mk11_root}/mk11/mk11-look/look-photoviz/photoviz-materials.usdc"
    output_file = f"{mk11_root}/mk11/mk11-look/look-photoviz/photoviz-logos/logos-manufacturing-tracker.csv"

    shaders = find_autolib_decal_shaders(stage_file)

    if not shaders:
        print("No arnold:AutolibDecal shaders found.")
        sys.exit(0)

    print(f"Found {len(shaders)} arnold:AutolibDecal prims")

    existing_prim_paths = set()
    fieldnames = list(shaders[0].keys())

    if os.path.exists(output_file):
        with open(output_file, mode='r', newline='') as f:
            reader = csv.DictReader(f)
            for row in reader:
                existing_prim_paths.add(row["prim_path"])

    new_shaders = [s for s in shaders if s["prim_path"] not in existing_prim_paths]

    if not new_shaders:
        print("No new prims to add — all entries already exist in the CSV.")
        sys.exit(0)

    print(f"Adding {len(new_shaders)} new entries ({len(shaders) - len(new_shaders)} already present)")

    file_is_new = not os.path.exists(output_file) or os.path.getsize(output_file) == 0

    with open(output_file, mode='a', newline='') as f:
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        if file_is_new:
            writer.writeheader()
        writer.writerows(new_shaders)