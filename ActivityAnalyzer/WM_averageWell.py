"""
    Extract a stack of images of a wormotel for training the classifier
"""

import cv2
import os
import matplotlib.pyplot as plt
import numpy as np
import tkinter
from CNN_EvaluateManualSegmentations import safeImportBoundingBoxes



""" core engine function """

def generateKernel(xfracstart,xfracend,yfracstart,yfracend,outnum, rollx, rolly, pout):

    # Extract, resize, and add to stack all well images
    well_imgs = np.zeros(shape=(0,0))

    for row in well_coords:

        # Extract well images
        well_size = 65
        buff=int(well_size*1.0)
        x0 = int(row[2]-buff)
        y0 = int(row[3]-buff)

        x1 = int(x0 + 2*buff)
        y1 = int(y0 + 2*buff)

        crop_img = img[y0:y1,x0:x1]

        # Keep only central wells
        if (x0 < xfracstart*C) or (x1 > xfracend*C) or (y0 < yfracstart*R) or (y1 > yfracend*R):
            continue

        # Resize to standard size - 50x50
        crop_img = cv2.resize(crop_img,(50,50))

        # Normalize to the same mean (128)
        crop_img = cv2.normalize(crop_img,0,255,norm_type=cv2.NORM_MINMAX,dtype=cv2.CV_8U)


        # Add to the list
        if well_imgs.shape[0] == 0:
            well_imgs = crop_img
        else:
            well_imgs = np.dstack((well_imgs,crop_img))

    # Get the average and view
    well_avg = np.mean(well_imgs,axis=2).astype(np.uint8)

    # Roll the image to keep it centered
    well_avg = np.roll(well_avg,(rolly,rollx))

    fout = os.path.join(pout,'WellStandard_' + str(outnum).zfill(2) + '.png')
    cv2.imwrite(fout,well_avg)

    cv2.namedWindow('PREVIEW',cv2.WINDOW_NORMAL)
    cv2.imshow('PREVIEW', well_avg)
    cv2.waitKey(1);



""" script """

# Get file names
pimg = r'C:\Dropbox\WormWatcher (shared data)\Wormotel simulations\SampleImages\Training Samples'
pout = r'C:\Dropbox\WormWatcher (shared data)\Wormotel simulations\SampleImages\WS'
fimg = '161005_2016-11-06 (00-50-25).PNG'
fcsv = fimg.upper().replace('PNG','csv').lower()
fcsv = os.path.join(pimg,fcsv)

# Load image
img = cv2.imread(os.path.join(pimg,fimg),cv2.IMREAD_GRAYSCALE)
#img = cv2.Laplacian(img,cv2.CV_32F);
R, C = img.shape

# Load the CSV with rectangles drawn
old_rects = np.zeros(shape = (1,5)) # preallocate dummy old rects for comparing to previous CSV
well_coords = np.genfromtxt(fcsv, delimiter=',',skip_header = 1)

# Create "Centered" well kernel
idx = 0
generateKernel(0.4,     0.6,    0.4,    0.6,    idx, 0 , 0, pout) # center
keyarr = np.zeros((17,2))
keyarr[0,:] = [0.5,0.5]

# Create several kernels at different offsets
offsets = [0,0,0,0] #[2,1,-1,-2]
for (xstart, rollx) in zip(np.arange(0,0.76,0.25),offsets):
    for (ystart,rolly) in zip(np.arange(0,0.76,0.25),offsets):
        idx+=1
        generateKernel(xstart,     xstart+0.25,    ystart,    ystart + 0.25,    idx, rollx, rolly, pout) # center
        keyarr[idx,:] = [xstart+0.125,ystart+0.125]

# Write csv file
csvout = os.path.join(pout,"wellrgnkey.csv")
np.savetxt(csvout, keyarr, delimiter=",", fmt = '%.2f')
