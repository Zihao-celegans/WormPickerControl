"""
    Export Cropped ROI IMages
    Save images of each individual ROIs to use for user marking, training or testing.
    
    Adapted from zscriptSampleImages.py

"""



from verifyDirlist import *
from csvio import *
import os
import numpy as np
from shutil import copyfile
import matplotlib.pyplot as plt
import cv2


# Parameters
pparent = r'F:\RNAi_Project'
pout = r'F:\RNAi_project_contam_train_images'
sessions_per_day = 2
days_to_sample = [4,10,20]
days_to_sample = np.asarray(days_to_sample)

# Find the plate folders files
d_plates, plate_names = verifyDirlist(pparent,True,"","_Analysis")

# Going to keep a list of all fout images
foutlist = list()

# For each CSV file load in the total activity at timepoint 0.75 days
for (pplate,name) in zip(d_plates,plate_names):

    # Find the Sessions for this plate
    d_sessions_full,d_sessions = verifyDirlist(pplate,True,"")

    # Skip plate if not enough sessions
    sessions_to_sample = np.round(days_to_sample * sessions_per_day)

    if len(d_sessions) < max(sessions_to_sample+1):
        print("-----------------------------> Skipping (not enough sessions): ",name)
        continue

    # Extract one image from requested sessions 
    d_sessions_full = [d_sessions_full[i] for i in sessions_to_sample]
    d_sessions = [d_sessions[i] for i in sessions_to_sample]

    # Loop over sessions
    for (session_full,session) in zip(d_sessions_full,d_sessions):

        # Get the ROI coordinates
        d_roi = verifyDirlist(session_full,False,'ROI_Coord')[0]

        if len(d_roi) != 1:
            print("-----------------------------> Skipping (no ROI file!!): ",session_full)
            continue

        rois = readArrayFromCsv(d_roi[0],"<ROI Coordinates>")
        x = rois[0,:].astype(np.int)
        y = rois[1,:].astype(np.int)
        ri = rois[2,:].astype(np.int)
        ro = rois[3,:].astype(np.int)

        # Get the images for this image series
        d_img_full,d_img = verifyDirlist(session_full,False,'png')

        if len(d_img_full) < 1:
            print("-----------------------------> Skipping (no images!!): ",session_full)
            continue

        fimg = d_img_full[0]
        img = cv2.imread(fimg)

        # Extract the ROI images and mark the worms
        for (thisx,thisy,thisri,thisro,roinum) in zip(x,y,ri,ro,range(1,len(x)+1)):

            # Crop to ROI
            crop_img = img[thisy-thisro:thisy+thisro, thisx-thisro: thisx+thisro]

            # Display the crop image
            #plt.figure(2)
            #plt.imshow(crop_img)
            #plt.show()

            # Save the crop image to disk
            fout = os.path.join(pout,name + "_" + session + "_ROI=" + str(roinum) + ".png")
            cv2.imwrite(fout,crop_img)
