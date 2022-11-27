"""
    WM_ExportWellOff.m

    Adapted from WM_ExtractWellBG

    Instead of exporting "Centered wells" vs "Everything else", instead export:
    Well = Anything in the well area (the repeating gridlike part)
    Background = Edges of the chip and off of the chip

    Requires annotation does by WM_AnnotateWellVsOff

"""



import cv2
import os
import matplotlib.pyplot as plt
import numpy as np
import tkinter
from CNN_EvaluateManualSegmentations import safeImportBoundingBoxes
from verifyDirlist import *
from matplotlib import path
import imutils


# Set paths
pimg = r'C:\Dropbox\WormWatcher (shared data)\Wormotel simulations\SampleImages\SamplesFrom PDM AUXIN'
ptest = r'C:\WMtest'
ptrain = r'C:\WMtrain'
N_samp_img = 1200 # number of samples per image to export, set to 8 if just want to test CSV validity, 2000+ otherwise
export_size = 20

# Create the output directories
pout_well_train = os.path.join(ptrain,"well")
pout_well_test = os.path.join(ptest,"well")
pout_bkg_train = os.path.join(ptrain,"background")
pout_bkg_test = os.path.join(ptest,"background")
all_dirs = [pout_well_train,pout_well_test,pout_bkg_train,pout_bkg_test]

for this_dir in all_dirs:
    if not os.path.exists(this_dir):
        os.makedirs (this_dir)


# Find training and testing folders
fullfolders,partfolders = verifyDirlist(pimg,True,'plates')

for pimg in fullfolders:

    # Find the CSV files that show which ones have already been analyzed
    [d_mask,d_short] = verifyDirlist(pimg,False,"_contmask.png");

    # Find the image associated with each csv and export small figures of worms
    exported_by_type = [0,0,0,0]
    exported_by_folder = list()

    for (maskname,shortname) in zip(d_mask,d_short):

        # Get associated image
        imgname = maskname.replace('_contmask.png','.png')
        (trash,imghead) = os.path.split(imgname)

        # Error if not exist
        if not os.path.exists(imgname):
            print('Skipped (no image): ',imgname)

        # Load image as grayscale
        img_orig = cv2.imread(imgname,cv2.IMREAD_GRAYSCALE)

        # Load mask as grayscale
        mask_orig = cv2.imread(maskname,cv2.IMREAD_GRAYSCALE)

        # Estimate the width of the wells, using median to reduce the influence of column jumps

        contours = cv2.findContours(mask_orig,cv2.RETR_LIST,cv2.CHAIN_APPROX_SIMPLE)[1][0]
        xmax = np.max(contours[:,0,0],axis=0)
        xmin = np.min(contours[:,0,0],axis=0)
        well_size = (xmax-xmin)/20
        rws = int(round(well_size/2))

        is_train = 'TEST' not in pimg.upper()

        # Continue exporting until we've got 1200 WELL images. 
        i=0
        backgrounds_exported = 0
        wells_exported = 0

        while (wells_exported+backgrounds_exported) < (2*N_samp_img):

            i+=1

            # Export random size, but later it will get resize to 20x20. Helps avoid learning size-dependent features
            r = np.random.randint(rws*0.4,rws*1.2)

            # Draw random x and y coordinates
            xrand = np.random.randint(r,img_orig.shape[1]-r-1)
            yrand = np.random.randint(r,img_orig.shape[0]-r-1)

            # Randomly rotate the image between +/- 2 degrees
            angle = 6*(np.random.rand() - 0.5)
            img = imutils.rotate(img_orig,angle)
            mask =imutils.rotate(mask_orig,angle)

            # Determine whether this region is mostly wells, or mostly not wells
            this_mask = np.zeros(img.shape).astype(np.uint8)
            this_mask[yrand-r:yrand+r,xrand-r:xrand+r] = 1
            test = this_mask * mask
            
            #cv2.namedWindow('Preview',cv2.WINDOW_NORMAL)
            #cv2.imshow('Preview',test);
            #cv2.waitKey(0);

            test[test>0] = 1
            mostly_wells = np.sum(test) >= (0.75*np.sum(this_mask))

            # Preventing imbalance: if this is a background and we already have too many backgrounds, skip
            if not mostly_wells and backgrounds_exported >= N_samp_img:
                continue

            # Preventing imbalance: if this is a well and we already have too many wells, skip
            elif mostly_wells and wells_exported >= N_samp_img:
                continue
                
            elif not mostly_wells:
                backgrounds_exported += 1

            else:
                wells_exported += 1

            # Extract crop image
            crop_img = img[yrand-r:yrand+r,xrand-r:xrand+r]

            # Resize crop image
            crop_img = cv2.resize(crop_img,(export_size,export_size))

            # Save it in the appropriate folder

            # Training worm
            if mostly_wells and is_train:
                fout = imghead.replace(".png","_worm_" + str(i).zfill(4) + '.png')
                fout = os.path.join(pout_well_train,fout)
                exported_by_type[0]+=1


            # testing worm
            elif mostly_wells and not is_train:
                fout = imghead.replace(".png","_worm_" + str(i).zfill(4) + '.png')
                fout = os.path.join(pout_well_test,fout)
                exported_by_type[1]+=1

            # training background
            elif not mostly_wells and is_train:
                fout = imghead.replace(".png","_bkg_" + str(i).zfill(4) + '.png')
                fout = os.path.join(pout_bkg_train,fout)
                exported_by_type[2]+=1

            # testing background
            elif not mostly_wells and not is_train:
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
