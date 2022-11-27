"""

CNN_exportRandomLoc3frames.py

Export unsorted images form WormCamps analyzed with WWAnalyzer. Export 3 images 

Adapted from CNN_exportROIContClean


"""


from verifyDirlist import *
from csvio import *
import os
import numpy as np
from shutil import copyfile
import matplotlib.pyplot as plt
import cv2
import pandas as pd


# Parameters
pparent = r'E:\RNAi_Project'
sessions_per_day = 2
days_to_sample = [4,20]    
days_to_sample = np.asarray(days_to_sample)
outsize = 40 # Final size of the output images for classification and training
fovsize = 50 # Size of the FOV to export. 2*outsize is like dsfactor 0.5 
N_samples_per_well = 8
inner_radius = True # False to use outer radius

# Set output path nearby
root, trash = os.path.split(pparent)
pout = os.path.join(root,'UnsortedColor02')

if not os.path.exists(pout):
    os.mkdir(pout)

# Find the plate folders files
d_plates, plate_names = verifyDirlist(pparent,True,"","_Analysis")

# For each plate folder loop over sessions
for (pplate,name,i) in zip(d_plates,plate_names,range(len(plate_names))):

    # Find the Sessions for this plate
    d_sessions_full,d_sessions = verifyDirlist(pplate,True,"")

    # Skip plate if not enough sessions
    sessions_to_sample = np.round(days_to_sample * sessions_per_day)

    if len(d_sessions) < max(sessions_to_sample+1):
        print("-----------------------------> Skipping (not enough sessions): ",name)
        continue
    
    # Extract Session names to analyze
    d_sessions_full = [d_sessions_full[i] for i in sessions_to_sample]
    d_sessions = [d_sessions[i] for i in sessions_to_sample]

    print("Starting plate ", i, " of ", len(plate_names))


    # Loop over sessions
    for (session_full,session) in zip(d_sessions_full,d_sessions):

        # Get the ROI coordinates
        d_roi = verifyDirlist(session_full,False,'ROI_Coord')[0]

        # If there are multiple ROI files, take the longer named one (Duplicate on WW3 didn't delete old style ROI files)
        if len(d_roi) > 1:
            longest_idx = 0
            longest_len = 0;

            for k in range(len(d_roi)):
                if len(d_roi[k]) > longest_len:
                    longest_len = len(d_roi[k])
                    longest_idx = k

            d_roi = [d_roi[longest_idx]]


        # If no ROI skip
        if len(d_roi) != 1:
            print("-----------------------------> Skipping (", len(d_roi), " ROI files!!): ",session_full)
            continue

        # Read ROI
        rois = readArrayFromCsv(d_roi[0],"<ROI Coordinates>")
        xw = rois[0,:].astype(np.int)
        yw = rois[1,:].astype(np.int)
        ri = rois[2,:].astype(np.int)
        ro = rois[3,:].astype(np.int)


        # Get the images for this image series, 1 min apart, 3x
        d_img_full,d_img = verifyDirlist(session_full,False,'png')
        N_images = len(d_img_full)

        if N_images < 50:
            print("-----------------------------> Skipping (no images!!): ",session_full)
            continue

        # Construct a color image from the set of 3
        time_samples = [int(N_images/2 + 4), int(N_images/2 + 10), int(N_images/2 + 16)];
        imgnum = -1;
        for idx in time_samples:
            img = cv2.imread(d_img_full[idx],cv2.IMREAD_GRAYSCALE)

            if idx == time_samples[0]:
                all_img = np.zeros((img.shape[0],img.shape[1],len(time_samples)))

            imgnum += 1;
            all_img[:,:,imgnum] = img


        # Loop over wells in this image 
        N_wells = len(ro)
        H,W = img.shape

        for (thisx,thisy,thisri,thisro,roinum) in zip(xw,yw,ri,ro,range(1,len(xw)+1)):
           
           # get well range based on outer limits of well (not allowed to be OOB)
           xw0 = np.max([thisx - thisro,0])
           xw1 = np.min([thisx + thisro,W-1])
           yw0 = np.max([thisy - thisro,0])
           yw1 = np.min([thisy + thisro,H-1])

           # Extract some random images from this well
           for i in range(0,N_samples_per_well):

               # ZOOM augment: Select a random sample image size based on the requested 20x20 (for example) size 
               this_size = fovsize + np.random.randint(-5,5)
               r = int(this_size/2)

               # POSITION: Select random coordinates in range, but require it is within inner radius if requested
               dist = 9999
               while (inner_radius and dist > thisri) or dist == 9999:
                   x = np.random.randint(xw0+r+1,xw1-r-1)
                   y = np.random.randint(yw0+r+1,yw1-r-1)
                   dist = np.sqrt((x-thisx)**2 + (y-thisy)**2)

               # Extract the sample image
               x0 = x - r;
               x1 = x + r;
               y0 = y - r;
               y1 = y + r;
               crop_img = all_img[y0:y1,x0:x1,:]

               # Resize the sample image to the standard outsize
               crop_img = cv2.resize(crop_img,(outsize,outsize))

               # No classification at this stage... save it directly to folder
               fout = name + '_session=' + session + '_ROI=' + str(roinum) + '_sample=' + str(i) + '.png'
               fout = os.path.join(pout,fout)

               cv2.imwrite(fout,crop_img)

