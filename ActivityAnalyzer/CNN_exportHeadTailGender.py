"""
    CNN_exportSmallImages.py

    adapted from CNN_exportWormBackground.py

    Designed for data set C:\Dropbox\FramesData\Mask_RCNN_Datasets\GenderDatasets\Preliminary_Gender_Dataset
    created by PDB May 2021. 

    Export small 20x20 images from a larger image, sorting them into an arbitrary number of classes


    Exports 4 classes: 
        head        (from 0 channel of mask images, vals 150+)
        tail_mail   (from channel 1 of mask images, vals 150+)
        tail_herm   (from channel 2 of mask)
        background


"""

from verifyDirlist import *
from csvio import *
import os
import numpy as np
from shutil import copyfile
import matplotlib.pyplot as plt
import cv2
from tkinter import filedialog
from tkinter import Tk
import imutils

from CNN_EvaluateManualSegmentations import drawROIs
from CNN_EvaluateManualSegmentations import safeImportBoundingBoxes

# Suppress empty CSV warnings, these are okay
import warnings
warnings.filterwarnings("ignore")

def getRandomCoords(r,width,height,xt,yt,does_not_contain_tip_mode):

    contains_tip = does_not_contain_tip_mode

    # Draw random x and y coordinates, but make sure not near tip
    while(contains_tip == does_not_contain_tip_mode):

        # Get random
        if does_not_contain_tip_mode:
            xrand = np.random.randint(r+2,width-r-3)
            yrand = np.random.randint(r+2,height-r-3)
        else:

            xmin = max((0,xt-2))
            xmax = min((width-1,xt + 2))
            ymin = max((0,yt-2))
            ymax = min((height-1,yt+2))

            xrand = np.random.randint(xmin,xmax)
            yrand = np.random.randint(ymin,ymax)

        # Stop immediately if None tip
        if xt == None or yt == None:
            break

        # Otherwise check if near tip
        contains_tip = np.sqrt((xt-xrand)**2 + (yt-yrand)**2) <= 5


    return xrand,yrand



# Set folder that contains the ROI images datasets, divided into various people's groups
pparent = r'C:\Dropbox\FramesData\Mask_RCNN_Datasets\GenderDatasets\Preliminary_Gender_Dataset'
pout = r'G:\CNN_Datasets\Head_Tail_Gender_CNN_01'
ptest = os.path.join(pout,'test')
ptrain = os.path.join(pout,'train')

# Parameters
N_samp_img = 8 # number of samples per image to export, set to 8 if just want to validity, 2000+ otherwise
dsfactor = 0.5 #1.0 to disable
seg_type = ["background","head","tail_male","tail_herm"]
do_vert_flipping = False

# Set image parameters
grab_r = [8,15] # Range of radii to GRAB images from. Actual resized to r=10 regardless, but changes FOV
r = 10          # RADIUS of crop image size, actual crop image size is r*2
outsize = r*2

# Create the output directories
pout_back_train = os.path.join(ptrain,"background")
pout_back_test = os.path.join(ptest,"background")
pout_pick_train = os.path.join(ptrain,seg_type)
pout_pick_test = os.path.join(ptest,seg_type)
pout_checkpoint = os.path.join(pout,"checkpoint")
all_dirs = [pout_back_train,pout_back_test,pout_pick_train,pout_pick_test,pout_checkpoint]

for this_dir in all_dirs:
    if not os.path.exists(this_dir):
        os.makedirs (this_dir)

    print("made dir ",this_dir)


# Find the dataset folders in this folder
d_image_sets = verifyDirlist(pparent,True,"DatasetImages")[0]
exported_by_type = [0,0,0,0]


for (pperson,i) in zip(d_image_sets,range(len(d_image_sets))):

    # Find the ROI files that show which ones have already been analyzed
    [d_roi,d_roi_short] = verifyDirlist(pperson,False,'_contmask.png');



    print('Starting folder ', i, ' of ', len(d_image_sets))

    # Get individual images from the segmented pick / worm folder, AND the corresponding RAW folder
    for (roiname,shortroiname) in zip(d_roi,d_roi_short):

        # Get associated image, which should be in a different folder
        imgname = roiname.replace('_contmask.png','.png')
        imgname = imgname.replace('_' + seg_type,'')
        imgname = imgname.replace('tip','') # some folders are labeled picktip, get rid of that if it's there

        (trash,imghead) = os.path.split(imgname)
        (trash,foldername) = os.path.split(trash)

        # Error if not exist
        if not os.path.exists(imgname):
            print('Skipped (no image): ',imgname)
            continue

        # Load the image as grayscale and resize to standard resolution so that the output size has a good resolution
        img = cv2.imread(imgname,cv2.IMREAD_GRAYSCALE)
        img = cv2.resize(img,(0,0), fx = dsfactor, fy = dsfactor)

        # Extract ROI mask of contamination (actually, it's the pick outline)
        cont_img = cv2.imread(roiname,cv2.IMREAD_GRAYSCALE)
        cont_img = cv2.resize(cont_img,(0,0),fx = dsfactor, fy = dsfactor)
        cont_img = (cont_img>0).astype(np.uint8)

        # Get the pick tip (rightmost point that equals 1)
        xt = None
        yt = None
        if np.max(cont_img) > 0:

            # Get contours of pick
            image, contours, hierarchy = cv2.findContours(cont_img,cv2.RETR_LIST ,cv2.CHAIN_APPROX_NONE)
            contours =contours[0]
            x = contours[:,0,0]
            y = contours[:,0,1]

            # Find right most point, but look a LITTLE bit to the left to capture U shape
            idx = int(np.mean(np.argwhere(x == np.max(x))))
            xt = x[idx] - 10
            yt = y[idx]



            # For worms - look for the CENTROID point (of each worm individually, not all worms put together)
           

        # Anything from the testing folder is not for training
        is_train = not 'TEST' in roiname
       

        # Step 1: Get backgrounds, no augmentation because lots of background samples available
        for i in range(N_samp_img):

            xrand,yrand = getRandomCoords(r,img.shape[1],img.shape[0],xt,yt,True)

            # Extract crop image
            crop_img = img[yrand-r:yrand+r,xrand-r:xrand+r]

            # SAve in train or test background folder
            if is_train:
                exported_by_type[2]+=1
                fout = foldername + "_bkg_" + str(exported_by_type[2]).zfill(4) + '.png'
                fout = os.path.join(pout_back_train,fout)
                


            elif not is_train:
                exported_by_type[3]+=1
                fout = foldername + "_bkg_" + str(exported_by_type[3]).zfill(4) + '.png'
                fout = os.path.join(pout_back_test,fout)
                

            cv2.imwrite(fout,crop_img)

        # Continue if no tip
        if xt == None or yt == None:
            continue

        # STEP 2: Get augmented images of each class type
        for i in range(2*N_samp_img):

            # Select grab radius (zoom level)
            this_r = np.random.randint(grab_r[0],grab_r[1])

            # Get random coordinate NEAR the tip
            xrand,yrand = getRandomCoords(this_r,img.shape[1],img.shape[0],xt,yt,False)

            # Augment #1: Crop the image using requested zoom, prevent OOB access

            xmin = max((0,xrand-this_r))
            xmax = min((img.shape[1]-1,xrand+this_r))
            ymin = max((0,yrand-this_r))
            ymax = min((img.shape[0]-1,yrand+this_r))

            crop_img = img[ymin:ymax,xmin:xmax]

            # Resize cropped image to correct size 
            crop_img = cv2.resize(crop_img,(2*r,2*r))

            # Augment #2: Flipping
            if np.random.normal() > 0:
                crop_img = cv2.flip(crop_img,flipCode = 0)

            # Augment #3: Random noise
            rand_filter = np.random.rand(2*r,2*r) * 15 # Don't use 255, it will overwhelm images with noise
            crop_img = crop_img + rand_filter.astype(np.uint8)

            # Save it in the appropriate folder
            if is_train:
                exported_by_type[0]+=1
                fout = foldername + "_worm_" + str(exported_by_type[0]).zfill(4) + '.png'
                fout = os.path.join(pout_pick_train,fout)
                
            elif not is_train:
                exported_by_type[1]+=1
                fout = foldername + "_worm_" + str(exported_by_type[1]).zfill(4) + '.png'
                fout = os.path.join(pout_pick_test,fout)


            cv2.imwrite(fout,crop_img)

# Print overall numbers of the data size
print('Worms (training) = ', exported_by_type[0])
print('Worms (testing) = ', exported_by_type[1])
print('Backg (training) = ', exported_by_type[2])
print('Backg (testing) = ', exported_by_type[3])


