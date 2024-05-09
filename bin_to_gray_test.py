import numpy as np
import argparse
from PIL import Image
import cv2

def bin_to_png(bin_file, width, height, output_png):
    # Read binary data from file
    with open(bin_file, 'rb') as f:
        binary_data = f.read()

    # Convert binary data to numpy array
    image_data = np.frombuffer(binary_data, dtype=np.uint16) >> 6
    image_data = image_data << 6
    
    #Print the image size
    print(image_data.shape)
    print(width*height)
    
    norm_image = cv2.normalize(image_data, None, alpha = 0, beta = 255, norm_type = cv2.NORM_MINMAX, dtype = cv2.CV_16U)

    small = norm_image.astype(np.uint8)
    
    #Print the image size
    # print(image_data.shape)

    img = small.reshape((height,width))

    # Apply debayering to convert the raw image to a color image
    debayered_image = cv2.demosaicing(img, cv2.COLOR_BayerRG2RGB_VNG)  # Adjust Bayer pattern based on your camera
    r,g,b = cv2.split(debayered_image)
    r = cv2.equalizeHist(r)
    g = cv2.equalizeHist(g)
    b = cv2.equalizeHist(b)

    debayered_image = cv2.merge([r,g,b])



    # Convert to RGB
    #debayered_image = cv2.cvtColor(debayered_image,  cv2.COLOR_BGR2RGB)

    # # Create PIL image
    image = Image.fromarray(debayered_image, mode='RGB')  # 'L' mode for grayscale
    
    

    # # Save image as PNG
    image.save(output_png)

if __name__ == "__main__":
    bin_file = "out_0.ppm"
    width = 4096
    height = 1540
    output_png = "output_image_gray_t.png"

    bin_to_png(bin_file, width, height, output_png)