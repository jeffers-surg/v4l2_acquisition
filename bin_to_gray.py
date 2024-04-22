import numpy as np
import argparse
from PIL import Image

def bin_to_png(bin_file, width, height, output_png):
    # Read binary data from file
    with open(bin_file, 'rb') as f:
        binary_data = f.read()

    # Convert binary data to numpy array
    image_data = np.frombuffer(binary_data, dtype=np.uint8)

    # Chop off the embedded metadata lines
    #image_data = image_data[:(width * height * 3)]
    #image_start = width * height * 1
    #image_end = width * height * 2
    #image_data = image_data[image_start:image_end]
    
    #Isolate a specific channel
    image_start = 0
    #image_end = width * height * 2
    image_end = width * height * 3 + image_start
    image_data = image_data[image_start:image_end:3]


    # Reshape array to match image dimensions
    image_data = image_data.reshape((height, width))
    
    # Create PIL image
    image = Image.fromarray(image_data, mode='L')  # 'L' mode for grayscale
    
    # Save image as PNG
    image.save(output_png)

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='Convert 8-bit binary per pixel bin file to grayscale PNG')
    parser.add_argument('bin_file', type=str, help='Input binary file')
    parser.add_argument('width', type=int, help='Width of the image')
    parser.add_argument('height', type=int, help='Height of the image')
    parser.add_argument('output_png', type=str, help='Output PNG file')
    args = parser.parse_args()

    bin_to_png(args.bin_file, args.width, args.height, args.output_png)