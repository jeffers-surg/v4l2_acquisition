
import numpy as np

bin_file1 = "out_0.ppm"
bin_file2 = "out_20.ppm"

width = 4096
height = 1540

# Read binary data from file
with open(bin_file1, 'rb') as f:
    bin_data1 = f.read()

# Read binary data from file
with open(bin_file2, 'rb') as f:
    bin_data2 = f.read()

# Convert binary data to numpy array
image_data1 = np.frombuffer(bin_data1, dtype=np.uint8)

#Print the image size
print(image_data1.shape)

# Convert binary data to numpy array
image_data2 = np.frombuffer(bin_data2, dtype=np.uint8)

#Print the image size
print(image_data2.shape)

#for i in range(0, 64):
#    print(f"image_data1[{i}]: {hex(image_data1[i])}")
#    print(f"image_data2[{i}]: {hex(image_data2[i])}")


#Calculate the difference between the pixels in the first row
for i in range(0,2*width*4):
    index = i
    pixel_diff = image_data2[index] - image_data1[index]

    if (pixel_diff != 0):
        print(f"Pixel diff at index {index}: {pixel_diff}")