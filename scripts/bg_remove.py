import os
from PIL import Image

def remove_background(input_folder, output_folder, threshold=20):
    if not os.path.exists(output_folder):
        os.makedirs(output_folder)

    for filename in os.listdir(input_folder):
        if filename.lower().endswith(".jpg") or filename.lower().endswith(".jpeg") or filename.lower().endswith(".png"):
            input_path = os.path.join(input_folder, filename)
            output_path = os.path.join(output_folder, os.path.splitext(filename)[0] + ".png")

            try:
                img = Image.open(input_path)
                img = img.convert("RGBA")
                data = img.getdata()

                # Get the color of the top-left pixel as the background color
                background_color = data[0]

                new_data = []
                for item in data:
                    # Check if the color difference is within the threshold
                    if all(abs(item[i] - background_color[i]) <= threshold for i in range(3)):
                        # Make the pixel transparent
                        new_data.append((255, 255, 255, 0))
                    else:
                        new_data.append(item)

                img.putdata(new_data)
                img.save(output_path, "PNG")
                print(f"Processed: {filename} -> {output_path}")

            except Exception as e:
                print(f"Error processing {filename}: {e}")

if __name__ == "__main__":
    input_folder = "./"
    output_folder = "./" 
    threshold = 20 

    remove_background(input_folder, output_folder, threshold)

