"""
    Derived from zscriptTestMeanBackground, stripped all mean background computations and registration but instead load raw images,
    slightly blur, and then take the active threshold of a small region.

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
from ErrorNotifier import *

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



def segmentWormsAdaptThresh(pplate, N_wells = None, skip_already_done = True) :

    #if N_wells is not supplied it's probably 24
    if N_wells == None:
        N_wells = 24

    # If N_wells != 24 invalid
    Err = ErrorNotifier()
    if N_wells != 24 :
        msg = "Worm segmentation requires 24-well plates.\n" + str(N_wells) + "-well plates are not supported"
        Err.notify("Return",msg)
        return False

    # Set out file name
    (pparent,head) = os.path.split(pplate)
    outname = os.path.join(pparent,"_Analysis",head,"WormNumEstimates.csv")
    outpath = os.path.join(pparent,"_Analysis",head)

    # Check if already done
    if skip_already_done and os.path.exists(outname):
        Err.notify("Print", head + "  skipped by segmentWorms (already analyzed)")
        return True
    

    """ STEP 1: GET PLATE'S OVERALL BACKGROUND IMAGE """


    # Find all sessions
    d_sessions, d_sessions_part = verifyDirlist(pplate,True)

    i = -1
    x1 = 0
    y1 = 0
    dxlist = list()
    dylist = list()
    ROIlist = list()

    # Load the first image for every session, and the corresponding ROI info


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

            # Preallocate array if first image
            if i == 0:
                all_img = np.zeros([curr_image.shape[0],curr_image.shape[1], len(d_sessions)],np.uint8)
                all_img[:] = np.nan

            # Add the shifted (or not) image to the array        
            all_img[:,:,i] = curr_image

        elif len(d_csv) != 1 :
            msg = "Could not load ROIs from: " + session + " because " + str(len(d_csv)) + " csv files found in folder"
            print(msg)
            continue

        elif len(d_img) == 0 :
            msg = "Could not load images from: " + session + " because " + str(len(d_img)) + " image files found in folder"
            print(msg)
            continue



    """  STEP 2: GET  WORM SEGMENTATION IN EACH SESSION OF THIS PLATE """

    WormsPerSession = np.zeros((len(d_sessions),N_wells))
    WormsPerSession[:] = np.nan

    for (ROIS,i) in zip(ROIlist,range(len(d_sessions))): 

        # Select image to use for worm segmentation in this session
        img = all_img[:,:,i]

        #plt.figure(1)
        #plt.imshow(img)
        #plt.figure(2)
        #plt.imshow(img)
        #plt.figure(3)
        #plt.imshow(min_img)
        #plt.show()

        # Skip this image if it is all nan (e.g. skipped earlier)
        if np.all(np.isnan(img)) :
            continue;


        for j in range(0,ROIS.shape[1]): #

            # Extract ROI info
            x = ROIS[0,j]
            y = ROIS[1,j]
            ri = ROIS[2,j]
            ro = ROIS[3,j]

            # Extract an image of ONLY the well (and some nans around it)...
            ruse = ro*0.48
            well_img = circularROI(img,x,y,ruse,False) 

            # Crop the well
            x1 = int(x - ruse)
            x2 = int(x + ruse)
            y1 = int(y - ruse)
            y2 = int(y + ruse)
            roi_img = well_img[y1:y2,x1:x2]

            # Segment sharp objects, but exclude parallax areas
            hf_seg = cv2.adaptiveThreshold(roi_img.astype(np.uint8),255,cv2.ADAPTIVE_THRESH_MEAN_C ,cv2.THRESH_BINARY,9,-10)

            # Remove small or large objects
            min_size = 20
            max_size = 3.14159*(ri/20.0)**2
            hf_seg = removeObjectsBySize(hf_seg,min_size,max_size)

            # Display the image with ROIs on top (DEBUGGING ONLY)
            if __name__ == "__main__":
                plt.figure(1)
                plt.subplot(1,2,1)
                plt.imshow(roi_img, cmap='gray',clim=(0,np.max(roi_img)))
                plt.subplot(1,2,2)
                plt.imshow(hf_seg)
                plt.show(block = True)

            # Count up the worms visible on this well
            nb_components, output, stats, centroids = cv2.connectedComponentsWithStats(hf_seg.astype(np.uint8), connectivity=8)
            WormsPerSession[i,j] = nb_components


    # make sure the sessions list is the same size as the array
    if len(d_sessions_part) != WormsPerSession.shape[0]:
        Err.notify("Return","segmentWorms internal error: Worm Count array size does not match session list length")
        return False

    # Make sure the analysis folder has been generated
    if not os.path.exists(outpath):
        Err.notify("Return","The output path:\n" + outpath + "\nDoes not exist. Please run STEP 1 on the parent folder:\n" + pparent)
        return False

    # Try to open the CSV file
    try:
        outfile = open(outname, "w") # or "a+", whatever you need
    except IOError:
        Err.notify("Abort","Could not open file:\n" + outname + "\nPlease close Excel and try again!")

    # Save the Worm counts to file
    with outfile :

        # Print ROI nums 
        roinums = np.arange(1,N_wells+1)
        roinums = np.reshape(roinums,(1,N_wells))
        csvio.printArrayToCsv(outfile,"<ROI Number>",roinums)

        # Print actual worm count data
        csvio.printArrayToCsv(outfile,"<N worms est>",WormsPerSession)

        # TODO ----------------------------------------------------------------------------------------------------
        # need to print out data to a separate file per session just like SessionData, maybe even append to session data
        # OR
        # At least need to handshake between activitycurves file and this file to make sure points line up

        # Print session names to handshake with activity curves
        outfile.write("<Session names>\n")
        for session in d_sessions_part:
            outfile.write(session + "\n")

        # End the file
        csvio.printEndToCsv(outfile)

    # Return results
    return True #WormsPerSession

    
if __name__ == "__main__":
    #pplate = r"E:\old_RNAi_Project_Delete_Me\CX_2D_2"
    pplate = "F:\RNAi_project\C4_3C_4"
    segmentWormsAdaptThresh(pplate,None,False)
