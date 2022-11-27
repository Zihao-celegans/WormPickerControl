


from verifyDirlist import *
from csvio import *
import os
import numpy as np
from shutil import copyfile
import matplotlib.pyplot as plt
import cv2

from ImageProcessing import removeObjectsBySize


# load image
fimg = r'C:\Dropbox\WormWatcher (shared data)\Well score training\ImagesToLabel_Unassigned\C4_3C_4_2019-06-03 (23-53-50)_ROI=9.png'
img = cv2.imread(fimg,cv2.IMREAD_GRAYSCALE)

# global Otsu segmentation
thresh, bw = cv2.threshold(img,0,255,cv2.THRESH_BINARY+cv2.THRESH_OTSU)

# Regional Otsu segmentation

bwall = np.zeros(img.shape,dtype = np.uint8)
radius = 20
jump = 10

for i in range(radius,img.shape[0]-radius-1,jump):
    for j in range(radius,img.shape[1]-radius-1,jump):

        # Set crop window
        r0 = i-radius
        r1 = i+radius
        c0 = j-radius
        c1 = j+radius

        # Get crop image and otsu threshold it
        crop_img = img[r0:r1,c0:c1]
        thresh, bw = cv2.threshold(crop_img,0,255,cv2.THRESH_BINARY+cv2.THRESH_OTSU)

        # Filter small objects
        bw=cv2.erode(bw,(3,3))
        bw=removeObjectsBySize(bw,20,5000)

        # Any white pixels are transferred to the global image
        bwall[r0:r1,c0:c1] = bwall[r0:r1,c0:c1] + bw




plt.figure(1)
plt.imshow(bwall)
plt.figure(2)
plt.imshow(img)
plt.show()



