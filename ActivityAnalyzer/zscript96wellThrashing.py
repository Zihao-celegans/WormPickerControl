

from verifyDirlist import verifyDirlist
import csvio
import cv2
import numpy as np
import os
from tkinter import filedialog
import tkinter as tk
from scipy.signal import medfilt2d, correlate2d
import matplotlib.pyplot as plt


# Load image series, keeping only a crop of one well
pimg = r'E:\96well\2020-03-03_96well_BFrig_2in_SequenceLong'
d_img,__ = verifyDirlist(pimg,False,'tif')
d_img = d_img[0:300]


# Define well crop
w = 288
radius = 10
x0 = 294        
x1 = x0 + w
y0 = 234
y1 = y0 + w

# set output video
out = cv2.VideoWriter(os.path.join(pimg,'outvid.avi'),cv2.VideoWriter_fourcc('M','J','P','G'), 10, (2*w,2*w))

crops = np.zeros((w,w,len(d_img))).astype(np.uint8)
xbar = np.array([])
ybar = np.array([])
displ = np.array([])

# Load all images and compute background
for (fname,i) in zip(d_img,range(len(d_img))):

    # Load image
    img = cv2.imread(fname,cv2.IMREAD_GRAYSCALE).astype(np.float)

    # Get a crop
    crop = img[y0:y1,x0:x1]

    # Stack it up
    crops[:,:,i] = crop


running_min = np.mean(crops,axis=2)

# Keep track of up to 10 hypothesized objects
for i in range(len(d_img)):
    
    running_min_start = 30
    if i > running_min_start:
        running_min = np.mean(crops[:,:,i-running_min_start:i],axis=2)

    crop = crops[:,:,i] - running_min
    crop = cv2.GaussianBlur(crop,(3,3),1.0)

    # Compute movement via difference image from 2 frames ago
    if i > 5:

        ## Compute diff image and smooth it
        #diff_img = crops[:,:,i].astype(np.float)- crops[:,:,i-5].astype(np.float)
        #diff_img = cv2.GaussianBlur(diff_img,(3,3),1.0)
        #diff_img = cv2.normalize(diff_img,np.zeros(diff_img.shape),0.0,1.0,cv2.NORM_MINMAX)
        
        MML = cv2.minMaxLoc(crop)
        mean_val = np.mean(crop)
        min_val = MML[0]
        if min_val < mean_val * 0.80:
            worm_x, worm_y  = MML[2]
        else:
            worm_x = -99
            worm_y = -99

    else:
        diff_img = crop
        worm_x = -99
        worm_y = -99

    # Store the grayscale center of gravity
    #M = cv2.moments(crop)
    #xbar = np.append(xbar,M["m10"] / M["m00"]);
    #ybar = np.append(ybar,M["m01"] / M["m00"]);
    xbar = np.append(xbar,worm_x);
    ybar = np.append(ybar,worm_y);

    # Preview image
    #crop = cv2.resize(diff_img,(250,250))
    crop = cv2.normalize(crop.astype(np.float),np.zeros(crop.shape),0,1,cv2.NORM_MINMAX)
    preview = crop #np.concatenate((crop,diff_img),axis=1)

    # Draw circle label
    preview = cv2.cvtColor((preview*255).astype(np.uint8),cv2.COLOR_GRAY2BGR)
    preview = cv2.circle(preview,(worm_x,worm_y),radius,(0,255,0))
    preview = cv2.circle(preview,(worm_x+w,worm_y),radius,(0,255,0))


    # Enlarge image
    preview = cv2.resize(preview,(2*w,2*w))

    cv2.imshow('Preview',preview)
    cv2.waitKey(1)

    # Write frame
    out.write(preview)

out.release()


# Plot results
plt.figure(figsize = (15,3))
plt.subplot(2,1,1)
plt.plot(xbar)
plt.subplot(2,1,2)
plt.plot(ybar)

plt.show()



