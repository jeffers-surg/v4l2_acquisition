import cv2
import numpy as np

def raw10_to_rgb(raw10_data, width, height):
    # Convert RAW10 data to 16-bit
    raw16 = np.frombuffer(raw10_data, dtype=np.uint16)

    # Reshape the 16-bit array into an image
    raw_shape = (height, width // 2, 4)
    raw16 = np.reshape(raw16, raw_shape)

    # Split the 16-bit values into 10-bit values
    raw10 = np.zeros_like(raw16, dtype=np.uint16)
    raw10[:, :, 0] = ((raw16[:, :, 0] << 2) & 0xFFC)
    raw10[:, :, 1] = ((raw16[:, :, 1] << 2) & 0xFFC)
    raw10[:, :, 2] = ((raw16[:, :, 2] << 2) & 0xFFC)
    raw10[:, :, 3] = ((raw16[:, :, 3] << 2) & 0xFFC)

    # Convert to 8-bit Bayer pattern
    raw8_bayer = raw10.astype(np.uint8)

    # Perform Bayer pattern conversion
    bayer_pattern = np.zeros((height, width), dtype=np.uint8)
    bayer_pattern[:, ::2] = raw8_bayer[:, :, 1]  # R or G1
    bayer_pattern[:, 1::2] = raw8_bayer[:, :, 2]  # G2 or B

    # Demosaicing using OpenCV
    raw_image = cv2.cvtColor(bayer_pattern, cv2.COLOR_BayerBG2BGR)

    # Convert to 8-bit RGB
    rgb_image = cv2.normalize(raw_image, None, 0, 255, cv2.NORM_MINMAX, dtype=cv2.CV_8U)

    return rgb_image

def main():
    # Specify input file name and image dimensions
    input_file = "out.ppm"
    width = 4096  # Width of the image
    height = 1540  # Height of the image

    # Read RAW10 data from binary file
    with open(input_file, 'rb') as f:
        raw10_data = f.read()

    # Convert RAW10 to RGB
    rgb_image = raw10_to_rgb(raw10_data, width, height)

    # Save RGB image as PNG
    output_file = "output_rgb_image.png"
    cv2.imwrite(output_file, rgb_image)
    print(f"RGB image saved as {output_file}")

if __name__ == "__main__":
    main()