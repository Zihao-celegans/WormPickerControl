"""
    CNN_exportWormBackground.py

    adapted from CNN_exportPickBackgroundV2.py

    contmask files should be in a different folder with the same name and "pick" or "worm" depending on which you want to do

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


def getRandomCoords(r,width,height,List_centroid,does_not_contain_worm_mode):

    contains_worms = does_not_contain_worm_mode

    # Draw random x and y coordinates, but make sure not near worms
    while(contains_worms == does_not_contain_worm_mode):

        # Get random
        if does_not_contain_worm_mode:
            xrand = np.random.randint(r+2,width-r-3)
            yrand = np.random.randint(r+2,height-r-3)
        else:
            rand_idx = np.random.choice(len(List_centroid),1)
            [xt, yt] = List_centroid[rand_idx[0]]
            xmin = max((0,xt-2))
            xmax = min((width-1,xt + 2))
            ymin = max((0,yt-2))
            ymax = min((height-1,yt+2))

            xrand = np.random.randint(xmin,xmax)
            yrand = np.random.randint(ymin,ymax)

        # Stop immediately if None tip
        if List_centroid == [None, None]:
            break

        # Otherwise check if near any worms
        contains_worms = False
        for ct in List_centroid:
            if ct != [None, None]:
                contains_worms = contains_worms or (np.sqrt((ct[0]-xrand)**2 + (ct[1]-yrand)**2) <= 5)


    return xrand,yrand



# Set folder that contains the ROI images datasets, divided into various people's groups
pparent = r'C:\Users\lizih\WormPicker Dropbox\FramesData'
pout = r'E:\CNN_Datasets\Worm_gray_debug'
ptest = os.path.join(pout,'test')
ptrain = os.path.join(pout,'train')

# Parameters
N_samp_img = 8 # number of samples per image to export, set to 8 if just want to test CSV validity, 2000+ otherwise
dsfactor = 0.4 #1.0 to disable
seg_type = "worm"
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
d_image_sets = verifyDirlist(pparent,True,'_' + seg_type)[0]
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

        (trash,imghead) = os.path.split(imgname)

        # Error if not exist
        if not os.path.exists(imgname):
            print('Skipped (no image): ',imgname)
            continue

        # Load the image as grayscale and resize to standard resolution so that the output size has a good resolution
        img = cv2.imread(imgname,cv2.IMREAD_GRAYSCALE)
        img = cv2.resize(img,(0,0), fx = dsfactor, fy = dsfactor)

        # Extract ROI mask of contamination (actually, it's the worm outline)
        cont_img = cv2.imread(roiname,cv2.IMREAD_GRAYSCALE)
        cont_img = cv2.resize(cont_img,(0,0),fx = dsfactor, fy = dsfactor)
        cont_img = (cont_img>0).astype(np.uint8)



        # Get the list of centroid of worms
        List_centroid = [None, None]
        if np.max(cont_img) > 0:

            # Get contours of worms
            image, contours, hierarchy = cv2.findContours(cont_img,cv2.RETR_LIST ,cv2.CHAIN_APPROX_NONE)

            # For worms - look for the CENTROID point (of each worm individually, not all worms put together)
            List_centroid = [[None, None]]*len(contours)

            idx_centroid = 0
            for c in contours:
                # calculate moments for each contour
                M = cv2.moments(c)
                # calculate x,y coordinate of center
                if M["m00"] !=0:
                    cX = int(M["m10"] / M["m00"])
                    cY = int(M["m01"] / M["m00"])
                else:
                    cX = None
                    CY = None
                List_centroid[idx_centroid] = [cX, cY]
                idx_centroid = idx_centroid + 1



        # Anything from the testing folder is not for training
        is_train = not 'TEST' in roiname
       

        # Step 1: Get backgrounds, no augmentation because lots of background samples available
        for i in range(N_samp_img):

            xrand,yrand = getRandomCoords(r,img.shape[1],img.shape[0],List_centroid,True)

            # Extract crop image
            crop_img = img[yrand-r:yrand+r,xrand-r:xrand+r]

            # SAve in train or test background folder
            if is_train:
                exported_by_type[2]+=1
                fout = imghead.replace(".png","_bkg_" + str(exported_by_type[2]).zfill(4) + '.png')
                fout = os.path.join(pout_back_train,fout)
                


            elif not is_train:
                exported_by_type[3]+=1
                fout = imghead.replace(".png","_bkg_" + str(exported_by_type[3]).zfill(4) + '.png')
                fout = os.path.join(pout_back_test,fout)
                

            cv2.imwrite(fout,crop_img)

        # Continue if no worm
        if List_centroid == [None, None]:
            continue

        # STEP 2: Get augmented image of the tip
        for i in range(2*N_samp_img):

            # Select grab radius (zoom level)
            this_r = np.random.randint(grab_r[0],grab_r[1])

            # Get random coordinate NEAR the tip
            xrand,yrand = getRandomCoords(this_r,img.shape[1],img.shape[0],List_centroid,False)

            # Augment #1: Crop the image using requested zoom, prevent OOB access

            xmin = max((0,xrand-this_r))
            xmax = min((img.shape[1]-1,xrand+this_r))
            ymin = max((0,yrand-this_r))
            ymax = min((img.shape[0]-1,yrand+this_r))

            crop_img = img[ymin:ymax,xmin:xmax]

            # Resize cropped image to correct size 
            crop_img = cv2.resize(crop_img,(2*r,2*r))

            # Augment #2: Flipping
            if np.random.normal() > 0.5:
                crop_img = cv2.flip(crop_img,flipCode = 0)

            # Augment #3: Random noise
            rand_filter = np.random.rand(2*r,2*r) * 10 # Don't use 255, it will overwhelm images with noise
            crop_img = crop_img + rand_filter.astype(np.uint8)

            # Save it in the appropriate folder
            if is_train:
                exported_by_type[0]+=1
                fout = imghead.replace(".png","_worm_" + str(exported_by_type[0]).zfill(4) + '.png')
                fout = os.path.join(pout_pick_train,fout)
                
            elif not is_train:
                exported_by_type[1]+=1
                fout = imghead.replace(".png","_worm_" + str(exported_by_type[1]).zfill(4) + '.png')
                fout = os.path.join(pout_pick_test,fout)


            cv2.imwrite(fout,crop_img)

# Print overall numbers of the data size
print('Worms (training) = ', exported_by_type[0])
print('Worms (testing) = ', exported_by_type[1])
print('Backg (training) = ', exported_by_type[2])
print('Backg (testing) = ', exported_by_type[3])
