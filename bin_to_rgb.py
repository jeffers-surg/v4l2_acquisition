import struct
from PIL import Image

def convert_binary_rgb_to_png(input_file, width, height, output_file):
    # Open the binary file for reading
    with open(input_file, "rb") as f:
        # Read the binary data
        binary_data = f.read()
    
    # Initialize an empty list to store RGB tuples
    rgb_data = []
    
    # Unpack the binary data and convert it to RGB tuples
    #for i in range(0, len(binary_data), 3):
    for i in range(0, 4096*1536*3, 3):
        r, g, b = struct.unpack("BBB", binary_data[i:i+3])
        rgb_data.append((r, g, b))
    
    # Create a new RGB image
    image = Image.new("RGB", (width, height))
    
    # Set pixel data
    image.putdata(rgb_data)
    
    # Save image as PNG
    image.save(output_file, format="PNG")
    print("Conversion completed. PNG image saved as:", output_file)

# Example usage:
if __name__ == "__main__":
    # Input binary file containing 24-bit RGB data
    input_file = "out.ppm"
    
    # Dimensions of the image
    width = 4096
    height = 1536
    
    # Output PNG file name
    output_file = "output_image.png"
    
    # Convert and save as PNG
    convert_binary_rgb_to_png(input_file, width, height, output_file)
