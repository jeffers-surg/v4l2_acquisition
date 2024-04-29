import numpy as np
import argparse
from PIL import Image
import cv2

def debayer_image(raw_image_path):
    # Load the raw image
    raw_image = cv2.imread(raw_image_path, cv2.IMREAD_GRAYSCALE)

    # Apply debayering to convert the raw image to a color image
    color_image = cv2.cvtColor(raw_image, cv2.COLOR_BayerGR2BGR)  # Adjust Bayer pattern based on your camera

    return color_image

def bin_to_png(bin_file, width, height, output_png):
    # Read binary data from file
    with open(bin_file, 'rb') as f:
        binary_data = f.read()

    # Convert binary data to numpy array
    image_data = np.frombuffer(binary_data, dtype=np.uint8)
    
    #Print the image size
    print(image_data.shape)

    #Create a new array to store the new pixel data
    reduced_image_data = np.zeros((height, width), dtype=np.uint8)

    print(reduced_image_data.shape)

    #Reduce the size of each pixel from 10 bits down to 8 bits (1 byte)
    for y_index in range(0, height):
        print("On line: ", y_index, " of ", height, " lines.")
        for x_index in range(0, width):
            #Get the base index of the pixel
            base_pixel_index = x_index * 2 + y_index * width * 2
            #Combine both pixels
            pixel_data = image_data[base_pixel_index] + (image_data[base_pixel_index + 1] << 8)
            #Shift the pixel data over by 2 bits
            pixel_data = pixel_data >> 2
            #Save the pixel data back to a new image array
            reduced_image_data[y_index, x_index] = pixel_data

    # image_start = 0
    # image_end = width * height * 2 + image_start
    # image_step = 2
    # image_data = image_data[image_start:image_end:image_step]

    # #Reshape again
    # image_data = image_data[image_start: width * height]

    # # Reshape array to match image dimensions
    # image_data = image_data.reshape((height, width))
    
    # # Create PIL image
    image = Image.fromarray(reduced_image_data, mode='L')  # 'L' mode for grayscale
    
    # # Save image as PNG
    image.save(output_png)

if __name__ == "__main__":
    # parser = argparse.ArgumentParser(description='Convert 8-bit binary per pixel bin file to grayscale PNG')
    # parser.add_argument('bin_file', type=str, help='Input binary file')
    # parser.add_argument('width', type=int, help='Width of the image')
    # parser.add_argument('height', type=int, help='Height of the image')
    # parser.add_argument('output_png', type=str, help='Output PNG file')
    # args = parser.parse_args()

    #bin_to_png(args.bin_file, args.width, args.height, args.output_png)

    bin_file = "out.ppm"
    width = 4096
    height = 1540
    output_png = "output_image_gray.png"

    bin_to_png(bin_file, width, height, output_png)