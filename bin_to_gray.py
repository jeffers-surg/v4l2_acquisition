import numpy as np
import argparse
from PIL import Image
import cv2

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

    #Embedded metadata line offset
    emb_metadata_offset = width * 400

    #Are the last 2 bits of the even index always 0?

    last_2_bits_are_zero = True

    for i in range(emb_metadata_offset, width * height * 2 - emb_metadata_offset):
        #Determine if the element is an even index
        if (i % 2 == 0):
            if (image_data[i] & 0x3 != 0):
                last_2_bits_are_zero = False
                print(f"Found a nonzero at index: {i}")
                print(f"image_data[{i}]: {bin(image_data[i])}")
                break

    print(f"Are the last 2 bits of the even index always 0? {last_2_bits_are_zero}")

    #What is the largest odd number?
    largest_odd_number = 0

    for i in range(i, width * height * 2 - emb_metadata_offset):
        #Determine if the element is an even index
        if (i % 2 == 0):
            if (image_data[i] > largest_odd_number):
                largest_odd_number = image_data[i]

    print(f"What is the largest number at an odd index? {bin(largest_odd_number)}")


    #Print the first 12 elements of the array
    for i in range(emb_metadata_offset, emb_metadata_offset + 4):
        print("**************************************")
        print("First Line: ")
        print(f"image_data[{i - emb_metadata_offset}]: {bin(image_data[i])}")
        #print("Next line: ")
        #print(f"image_data[{i - emb_metadata_offset}]: {bin(image_data[i + width])}")

    # Reduce the size of each pixel from 10 bits down to 8 bits (1 byte)
    for y_index in range(0, 100):
    #for y_index in range(0, 20):
        if (y_index % 20 == 0):
            print("On line: ", y_index, " of ", height, " lines.")
        for x_index in range(0, width):
            #Get the base index of the pixel
            base_pixel_index = x_index * 2 + y_index * width * 2
            #Combine both pixels
            #pixel_data = image_data[base_pixel_index] & 0xFF + ((image_data[base_pixel_index + 1] >> 6) & 0x03) << 8
            #pixel_data = (image_data[base_pixel_index] & 0x3) + ((image_data[base_pixel_index + 1]) & 0xFF) << 8
            #pixel_data = (image_data[base_pixel_index])
            pixel_data = (image_data[base_pixel_index]) + ((image_data[base_pixel_index + 1]) << 8)
            #pixel_data = (image_data[base_pixel_index + 1]) + ((image_data[base_pixel_index]) << 8)
            #Shift the pixel data over by 2 bits
            #pixel_data = pixel_data << 2
            #pixel_data = image_data[base_pixel_index + 1] + 64

            pixel_data = (pixel_data  / 65535) * 255

            #Save the pixel data back to a new image array
            reduced_image_data[y_index, x_index] = pixel_data

    # Apply debayering to convert the raw image to a color image
    debayered_image = cv2.cvtColor(reduced_image_data,  cv2.COLOR_BayerRG2BGR)  # Adjust Bayer pattern based on your camera

    # Convert to RGB
    #debayered_image = cv2.cvtColor(debayered_image,  cv2.COLOR_BGR2RGB)

    # # Create PIL image
    image = Image.fromarray(debayered_image, mode='RGB')  # 'L' mode for grayscale
    
    

    # # Save image as PNG
    image.save(output_png)

if __name__ == "__main__":
    bin_file = "out.ppm"
    width = 4096
    height = 1540
    output_png = "output_image_gray.png"

    bin_to_png(bin_file, width, height, output_png)