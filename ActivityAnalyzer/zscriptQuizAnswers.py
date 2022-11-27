"""
    Interview quiz: Python / numpy and image handling basics

"""

# What are these three libraries?

import numpy as np
import matplotlib.pyplot as plt
import cv2


# Load the image file below into the variable img
fimg = r'C:\Dropbox\UPENN - Mobile DB\Interview Materials\sample_plate.png'
img = cv2.imread(fimg)

# The image might be color or grayscale. 
#   - How is each type represented?
#   - Force img to be grayscale

if len(img.shape) == 3:
    img = np.mean(img,axis=2)


# Array indexing
#   - What information is encoded in val?
#   - What kinds of mistakes might cause the script to crash at the below command?
val = img[500,500]


# Array slicing
#   - What information is encoded in vect?
vect = img[500,:]


# Plots
#   - Generate an appropriate plot to visualize vect.
#   - Describe what the resulting plot means

plt.plot(vect)
plt.show()


# Lighting can be nonuniform across an image. How could we measure whether this is the case?

