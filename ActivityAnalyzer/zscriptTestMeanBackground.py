"""
    SCRIPT, originally for testing whether worms could be nicely segmented in the wells by subtracting the background averaged
    over the course of the experiment, with image registration.

    Overall, sort of passable, but inferior to taking the adaptive threshold of a small ROI of the raw image.

    This script is therefore primarily configured for doing the RAW IMAGE active thresh but also calculates the mean background.

    The adaptive threshold has been promoted to a funciton and moved to segmentWormsAdaptThresh.py

"""



# Anthony's Includes
from verifyDirlist import verifyDirlist
from breakUpSession import breakUpSession
from getSessionData import getSessionData
from getSessionData import getSessionTimepoint
from getSessionData import validateSessionData
from getSessionData import plotSessionData
from ActivityCurves import ActivityCurves
from validateAnalysisFolder import validateAnalysisFolder
from ErrorNotifier import ErrorNotifier
import csvio as csvio
from ImageProcessing import *

# Standard Includes
import os
import numpy as np
import scipy as sp
import scipy.ndimage
import time
import sys

# OpenCV includes
import cv2
cv2.__version__

# Matplotlib backend
import matplotlib as temp
import matplotlib.pyplot as plt
temp.use("TkAgg")





# PARAMETERS ------------------------------------------------------------------------
pplate = r"H:\RNAi_Project\C5_3C_5"
allowed_shift = 0.05
N_wells = 24
exclude_first_sessions = 4 # Exclude the first x sessions to avoid young worms that are hard to segment and wet wells still settling
# -----------------------------------------------------------------------------------


""" STEP 1: GET PLATE'S OVERALL BACKGROUND IMAGE """

# Find all sessions
d_sessions = verifyDirlist(pplate,True)[0]

i = -1
x1 = 0
y1 = 0
dxlist = list()
dylist = list()
ROIlist = list()

# Load the first image for every session, and the corresponding ROI info
d_sessions = [d_sessions[5],d_sessions[-1]]

for session in d_sessions :
    d_img = verifyDirlist(session,False,".png","Roi")[0]
    d_csv = verifyDirlist(session,False,".csv","")[0]


    if len(d_img) > 0 and len(d_csv) == 1:

        # Increment the counter 
        i += 1

        # Load the first image in the session
        curr_image = cv2.imread(d_img[0],cv2.IMREAD_GRAYSCALE)
        
        # Blur the image a little bit (DONT BECAUSE IMAGES ARE STORED FOR USE LATER)
        curr_image = cv2.GaussianBlur(curr_image,(3,3),1)

        # Load the ROI info from the session
        ROIS = csvio.readArrayFromCsv(d_csv[0],"<ROI Coordinates>")

        # Store the ROIS, we need them later
        ROIlist.append(ROIS)

        # Store roi coord from well 1 (if first image) or else compare and shift the image
        if (x1 == 0) :
            # Get coordinates and store
            x1 = ROIS[0,0]
            y1 = ROIS[1,0]
        else:
            # Get coordinates and compute shift
            x1c = ROIS[0,0]
            y1c = ROIS[1,0]
            dx = x1c-x1
            dy = y1c-y1

            # Store this shift, it will be needed again
            dxlist.append(dx)
            dylist.append(dy)

            # Store the overall ROIS, needed later


            # Skip this image if shift is more than 5% of image width
            rows,cols = curr_image.shape
            if dx > allowed_shift * float(cols) or dy > allowed_shift * float(rows) :
                continue

            # Generate translation matrix
            # https://opencv-python-tutroals.readthedocs.io/en/latest/py_tutorials/py_imgproc/py_geometric_transformations/py_geometric_transformations.html
            M = np.float32([[1,0,-dx],[0,1,-dy]])

            # Apply shift to image
            #cv2.imwrite(r"C:\WormWatcher\Samples\test_delete_me.png",curr_image)
            curr_image = cv2.warpAffine(curr_image,M,(cols,rows))
            #cv2.imwrite(r"C:\WormWatcher\Samples\test_delete_me.png",curr_image)
          
            #if abs(dx) > 0 or abs(dy) > 0:
            #    plt.figure(1)
            #    plt.imshow(curr_image)
            #    plt.show()
            #    debug_here=1

        # Preallocate array if first image
        if i == 0:
            all_img = np.zeros([curr_image.shape[0],curr_image.shape[1], len(d_sessions)],np.float)
            all_img[:] = np.nan

        # Add the shifted (or not) image to the array        
        all_img[:,:,i] = curr_image

    elif len(d_csv) != 1 :
        msg = "Could not incorporate: " + session + " into background image Because " + str(len(d_csv)) + " csv files found in folder"
        print(msg)
        continue

    elif len(d_img) == 0 :
        msg = "Could not incorporate: " + session + " into background image Because " + str(len(d_img)) + " image files found in folder"
        print(msg)
        continue


# Get the minimum and mean images
min_img = np.nanmin(all_img,2).astype(np.float)
mean_img = np.nanmean(all_img,2)

# Show mean, min and diff images
plt.figure(1)
ax1= plt.subplot(2,2,1)
plt.imshow(mean_img)

plt.subplot(2,2,2, sharex = ax1, sharey = ax1 )
plt.imshow(min_img)

plt.subplot(2,2,3, sharex = ax1, sharey = ax1 )
plt.imshow(all_img[:,:,0])

plt.subplot(2,2,4, sharex = ax1, sharey = ax1 )
diff_img = all_img[:,:,-1] - all_img[:,:,0]
diff_img[diff_img<0] = 0
plt.imshow(diff_img)

plt.show();


"""  STEP 2: GET  WORM SEGMENTATION IN EACH SESSION OF THIS PLATE """




# Session 4 (for debug only)
WormsPerSession = np.zeros((len(d_sessions),N_wells))
WormsPerSession[:] = np.nan

for (dx,dy,ROIS,i) in zip(dxlist,dylist,ROIlist,range(len(d_sessions))): #

    # Select image to use for worm segmentation in this session
    img = all_img[:,:,i]


    # Subtract this background to get the background image
    img_sub = img
    img_sub[img_sub<0] = 0

    #plt.figure(1)
    #plt.imshow(img_sub)
    #plt.figure(2)
    #plt.imshow(img)
    #plt.figure(3)
    #plt.imshow(min_img)
    #plt.show()

    # Skip this image if it is all nan (e.g. skipped earlier)
    if np.all(np.isnan(img)) :
        continue;

    # Skip this image if shift is more than 5% of image width
    if dx > allowed_shift * float(curr_image.shape[1]) or dy > allowed_shift * float(curr_image.shape[0]) :
        continue


    for i in range(0,ROIS.shape[1]): #

        # Extract ROI info
        x = ROIS[0,i]
        y = ROIS[1,i]
        ri = ROIS[2,i]
        ro = ROIS[3,i]

        # Extract an image of ONLY the well (and some nans around it)...
        ruse = ri*0.6
        well_img = circularROI(img_sub,x,y,ruse,False) 

        # Crop the well
        x1 = int(x - ruse)
        x2 = int(x + ruse)
        y1 = int(y - ruse)
        y2 = int(y + ruse)
        roi_img = well_img[y1:y2,x1:x2]

        # Segment sharp objects, but exclude parallax areas
        hf_seg = cv2.adaptiveThreshold(roi_img.astype(np.uint8),255,cv2.ADAPTIVE_THRESH_MEAN_C ,cv2.THRESH_BINARY,9,-10)

        # Remove small objects
        min_size = 20
        max_size = 3.14159*(ri/20.0)**2
        hf_seg = removeObjectsBySize(hf_seg,min_size,max_size)

        # Display the image with ROIs on top
        #plt.imshow(imgdisplay)


        #plt.figure(1)
        #plt.subplot(1,2,1)
        #plt.imshow(roi_img, cmap='gray',clim=(0,np.max(roi_img)))
        #plt.subplot(1,2,2)
        #plt.imshow(hf_seg)
        #plt.show(block = True)
        B=1
    


