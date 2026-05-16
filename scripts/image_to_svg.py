import os
import subprocess
import cv2
import numpy as np

def raster_to_svg(image_path, output_path):
    img = cv2.imread(image_path, cv2.IMREAD_UNCHANGED)
    if img is None:
        print(f"Skipping '{image_path}': Unable to load image.")
        return

    # Ensure image has alpha channel
    if img.shape[2] < 4:
        print(f"Skipping '{image_path}': No alpha channel found.")
        return

    alpha = img[:, :, 3]
    inverted_alpha = 255 - alpha  # Invert: opaque becomes black, transparent becomes white

    _, binary = cv2.threshold(inverted_alpha, 240, 255, cv2.THRESH_BINARY)

    # Save the binary image as a PGM
    pgm_path = output_path.replace('.svg', '.pgm')
    cv2.imwrite(pgm_path, binary)

    # Use potrace to convert PGM to SVG
    try:
        subprocess.run(['potrace', pgm_path, '--svg', '-o', output_path], check=True)
        print(f"Converted: {image_path} -> {output_path}")
    except subprocess.CalledProcessError:
        print(f"Failed to convert: {image_path}")
    finally:
        if os.path.exists(pgm_path):
            os.remove(pgm_path)

def convert_folder(folder_path, output_folder):
    if not os.path.exists(output_folder):
        os.makedirs(output_folder)

    for filename in os.listdir(folder_path):
        if filename.lower().endswith(('.png', '.jpg', '.jpeg', '.bmp')):
            input_path = os.path.join(folder_path, filename)
            name_without_ext = os.path.splitext(filename)[0]
            output_path = os.path.join(output_folder, name_without_ext + '.svg')
            raster_to_svg(input_path, output_path)

if __name__ == "__main__":
    input_folder = "./"
    output_folder = "./"

    convert_folder(input_folder, output_folder)

