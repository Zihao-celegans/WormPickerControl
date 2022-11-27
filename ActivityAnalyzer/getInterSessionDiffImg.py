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
#pplate = r"H:\RNAi_Project\C5_3C_5"
#allowed_shift = 0.05
#N_wells = 24
#exclude_first_sessions = 4 # Exclude the first x sessions to avoid young worms that are hard to segment and wet wells still settling
# -----------------------------------------------------------------------------------

def getInterSessionDiffImg(pplate,allowed_shift = 0.05,N_wells = 24,exclude_first_sessions = 4):

    # Find all sessions for this plate
    d_sessions = verifyDirlist(pplate,True)[0]

    i = -1
    x1 = 0
    y1 = 0
    dxlist = list()
    dylist = list()
    ROIlist = list()

    # Load the first image for every session, and the corresponding ROI info
    d_sessions = [d_sessions[exclude_first_sessions],d_sessions[-1]]

    for session in d_sessions :
        d_img = verifyDirlist(session,False,".png","Roi")[0]
        d_csv = verifyDirlist(session,False,".csv","")[0]


        if len(d_img) > 0 and len(d_csv) == 1:

            # Increment the counter 
            i += 1

            # Load the first image in the session
            curr_image = cv2.imread(d_img[0],cv2.IMREAD_GRAYSCALE)
        
            # Preallocate array if first image
            if i == 0:
                all_img = np.zeros([curr_image.shape[0],curr_image.shape[1], len(d_sessions)],np.float)
                all_img[:] = np.nan

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

            # Add the shifted (or not) image to the array        
            all_img[:,:,i] = curr_image

        elif len(d_csv) != 1 :
            msg = "ERROR: Could not incorporate: " + session + " into background image Because " + str(len(d_csv)) + " was not analyzed by STEP 1 yet"
            print(msg)
            continue

        elif len(d_img) == 0 :
            msg = "ERROR: Could not incorporate: " + session + " into background image Because " + str(len(d_img)) + " was not analyzed by STEP 1 yet"
            print(msg)
            continue


    # Get the minimum and mean images
    min_img = np.nanmin(all_img,2).astype(np.float)
    mean_img = np.nanmean(all_img,2)
    diff_img = all_img[:,:,-1] - all_img[:,:,0]

    # Get diff images. Note that if frames failed to be found, the diff image should be all 0s
    diff_img[diff_img<0] = 0

    return diff_img, ROIlist
