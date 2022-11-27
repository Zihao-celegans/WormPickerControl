"""
    Export Cropped ROI IMages, sorted according to whether they are contaminated (group 6) or not.
    Save images of each individual ROIs to use for user marking, training or testing.
    
    Adapted from zscriptSampleImages.py

    Adapted from CNN_ExportROIContClean, but uses difference images for contamination growth 

"""



from verifyDirlist import *
from csvio import *
import os
import numpy as np
from shutil import copyfile
import matplotlib.pyplot as plt
import cv2
import pandas as pd
from getInterSessionDiffImg import  getInterSessionDiffImg

# Parameters
pparent = r'H:\RNAi_Project'
pout = r'H:\RNAi_project_contam_train_images'
fxlsx = r'C:\Dropbox\WormWatcher (shared data)\Well score training\WellScores.xlsx'
sessions_per_day = 2
days_to_sample = [2, 20] #[4,10,20]    # NOT ACTUALLY USED FOR DIFF IMAGE, ONLY USED TO SEE IF ENOUGH SESSIONS
days_to_sample = np.asarray(days_to_sample)
outsize = 120




# Load the excel file
xls = pd.ExcelFile(fxlsx)
data67 = pd.read_excel(xls, 'DataInclGrp6-7_IMAGEBASED').as_matrix()
dataorig = pd.read_excel(xls, 'Data').as_matrix()

# Organize excel data
labels67 = data67[:,1].astype(np.str)
data67 = data67[:,2:].astype(np.float)

labelsorig = dataorig[:,1].astype(np.str)
dataorig = dataorig[:,2:].astype(np.float)

# Find the plate folders files
d_plates, plate_names = verifyDirlist(pparent,True,"","_Analysis")

# Going to keep a list of all fout images
foutlist = list()

# For each CSV file load in the total activity at timepoint 0.75 days
for (pplate,name,i) in zip(d_plates,plate_names,range(len(plate_names))):

    # Find the Sessions for this plate
    d_sessions_full,d_sessions = verifyDirlist(pplate,True,"")

    # Skip plate if not enough sessions
    sessions_to_sample = np.round(days_to_sample * sessions_per_day)

    if len(d_sessions) < max(sessions_to_sample+1):
        print("-----------------------------> Skipping (not enough sessions): ",name)
        continue

    # Find the corresponding entry in the excel data
    has_name = np.char.find(labels67,name)
    idx = np.argwhere(has_name>=0)

    if len(idx) == 0:
        print('Could not find matching XLSX entry for: ', name)
        continue

    elif len(idx) > 1:
        print('Found too many match XLSX entries for: ', name)
        continue

    else:
        idx = idx[0]
        xlsxdata = data67[idx,:].flatten()



    # Extract one image from requested sessions 
    d_sessions_full = [d_sessions_full[i] for i in sessions_to_sample]
    d_sessions = [d_sessions[i] for i in sessions_to_sample]


    # Determine whether this plate will be training or testing
    if i%4 == 0:
        subfolder = 'test'
    else:
        subfolder = 'train'


    # Get the ROI coordinates of the first session
    d_roi = verifyDirlist(d_sessions_full[0],False,'ROI_Coord')[0]

    if len(d_roi) != 1:
        print("-----------------------------> Skipping (no ROI file in first session!!): ",session_full)
        continue

    rois = readArrayFromCsv(d_roi[0],"<ROI Coordinates>")
    x = rois[0,:].astype(np.int)
    y = rois[1,:].astype(np.int)
    ri = rois[2,:].astype(np.int)
    ro = rois[3,:].astype(np.int)

    # Get the difference image for this plate
    img, _ = getInterSessionDiffImg(pplate)
   

    # Extract the ROI images and mark the worms
    for (thisx,thisy,thisri,thisro,roinum) in zip(x,y,ri,ro,range(1,len(x)+1)):

        # Determine whether this well is marked contaminated or not
            

        if "4" in str(xlsxdata[roinum-1]):  # xlsxdata[roinum-1] > 0: #"6" in str(xlsxdata[roinum-1]):
            this_pout = os.path.join(pout,subfolder,'Contaminated')
            #dummy_add = (np.random.rand(outsize,outsize)*100).astype(np.uint8)
            #dummy_add = cv2.rectangle(dummy_add,(25,25),(75,75),(50,50,50),-1)
                
        else:
            this_pout = os.path.join(pout,subfolder,'Clean')
            #dummy_add = np.zeros(shape = (outsize,outsize)).astype(np.uint8)

        #this_pout = pout

        # Create the output path if it doesn't already exist
        if not os.path.exists(this_pout):
            os.makedirs (this_pout)

        # Crop to ROI
        crop_img = img[thisy-thisro:thisy+thisro, thisx-thisro: thisx+thisro]

        # Resize to standard small size
        crop_img = cv2.resize(crop_img,(outsize,outsize))

        # Crop to only inside 50% of image?
        cr = int(outsize/4)
        crop_img = crop_img[cr:cr+2*cr,cr:cr+2*cr]

        # Display the crop image
        #plt.figure(2)
        #plt.imshow(crop_img)
        #plt.show()

        # Save the crop image to disk
        session = "INTERSESSIONDIFF"
        fout = os.path.join(this_pout,name + "_" + session + "_ROI=" + str(roinum) + ".png")
        cv2.imwrite(fout,crop_img)
