"""
    WM_extractWellBg.py

    similar to CNN_ExportWormBackgroundImages.py

    Loads CSV file containing marked wells. Exports all "well (centered)" images to the "well" folder and all
    non-centerend-well images to the "background" folder. 
"""



import cv2
import os
import matplotlib.pyplot as plt
import numpy as np
import tkinter
from CNN_EvaluateManualSegmentations import safeImportBoundingBoxes
from verifyDirlist import *

# Set paths
pimg = r'C:\Dropbox\WormWatcher (shared data)\Wormotel simulations\SampleImages'
ptest = r'C:\WMtest'
ptrain = r'C:\WMtrain'
N_samp_img = 1200 # number of samples per image to export, set to 8 if just want to test CSV validity, 2000+ otherwise


# Create the output directories
pout_worm_train = os.path.join(ptrain,"well")
pout_worm_test = os.path.join(ptest,"well")
pout_bkg_train = os.path.join(ptrain,"background")
pout_bkg_test = os.path.join(ptest,"background")
all_dirs = [pout_worm_train,pout_worm_test,pout_bkg_train,pout_bkg_test]

for this_dir in all_dirs:
    if not os.path.exists(this_dir):
        os.makedirs (this_dir)


# Find training and testing folders
fullfolders,partfolders = verifyDirlist(pimg,True,'samples')

for pimg in fullfolders:

    # Find the CSV files that show which ones have already been analyzed
    [d_csv,d_short] = verifyDirlist(pimg,False,".csv");

    # Find the image associated with each csv and export small figures of worms
    export_size = 20
    exported_by_type = [0,0,0,0]
    exported_by_folder = list()

    for (csvname,shortname) in zip(d_csv,d_short):

        # Get associated image
        imgname = csvname.replace('.csv','.png')
        imgname = imgname.replace('.CSV','.png')

        (trash,imghead) = os.path.split(imgname)

        # Error if not exist
        if not os.path.exists(imgname):
            print('Skipped (no image): ',imgname)

        # Load image as grayscale
        img = cv2.imread(imgname,cv2.IMREAD_GRAYSCALE)

        # Load the csv file and xy data
        data = np.genfromtxt(csvname, delimiter=',',skip_header = 1)
        xbar = data[:,2]
        ybar = data[:,3]

        # Estimate the width of the wells, using median to reduce the influence of column jumps
        well_size = np.median(np.diff(data[:,3]))
        r = int(round(well_size/2))

        is_train = 'TRAIN' in pimg.upper()

        # Continue exporting until we've got 1200 WELL images. 
        i=0
        backgrounds_exported = 0
        worms_exported = 0

        while worms_exported <= N_samp_img:


            # Draw random x and y coordinates
            i+=1
            xrand = np.random.randint(r,img.shape[1]-r-1)
            yrand = np.random.randint(r,img.shape[0]-r-1)

            # Determine whether any well centroids are close (< 0.1*r?) to the center of this image 
            contains_ctr = np.sqrt((xbar-xrand)**2 + (ybar-yrand)**2) < (0.05*r)
            contains_any = np.any(contains_ctr)

            # Preventing imbalance: if this is a background and we already have too many backgrounds, skip
            if not contains_any and backgrounds_exported >= N_samp_img:
                continue
                
            elif not contains_any:
                backgrounds_exported += 1

            else:
                worms_exported += 1

            # Extract crop image
            crop_img = img[yrand-r:yrand+r,xrand-r:xrand+r]

            # Resize crop image
            crop_img = cv2.resize(crop_img,(export_size,export_size))

            # Save it in the appropriate folder

            # Training worm
            if contains_any and is_train:
                fout = imghead.replace(".png","_worm_" + str(i).zfill(4) + '.png')
                fout = os.path.join(pout_worm_train,fout)
                exported_by_type[0]+=1


            # testing worm
            elif contains_any and not is_train:
                fout = imghead.replace(".png","_worm_" + str(i).zfill(4) + '.png')
                fout = os.path.join(pout_worm_test,fout)
                exported_by_type[1]+=1

            # training background
            elif not contains_any and is_train:
                fout = imghead.replace(".png","_bkg_" + str(i).zfill(4) + '.png')
                fout = os.path.join(pout_bkg_train,fout)
                exported_by_type[2]+=1

            # testing background
            elif not contains_any and not is_train:
                fout = imghead.replace(".png","_bkg_" + str(i).zfill(4) + '.png')
                fout = os.path.join(pout_bkg_test,fout)
                exported_by_type[3]+=1
                
            cv2.imwrite(fout,crop_img)


        print('Finished plate ',shortname)
     
# Print overall numbers of the data size
print('Wells (training) = ', exported_by_type[0])
print('Wells (testing) = ', exported_by_type[1])
print('Backg (training) = ', exported_by_type[2])
print('Backg (testing) = ', exported_by_type[3])