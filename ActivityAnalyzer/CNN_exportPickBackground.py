"""
    CNN_exportPickBackground.py

    adapted from CNN_ExportROIContCleanSMALL.py

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


# Set folder that contains the ROI images datasets, divided into various people's groups
pparent = r'H:\Worm picker training\FramesData'
pout = r'C:\Users\lizih\OneDrive\Documents\CNN_Datasets'
# r'C:\Users\antho\Documents\CNN_Datasets\WPworm05'
ptest = os.path.join(pout,'test')
ptrain = os.path.join(pout,'train')

N_picks_max = 500
N_backs_max = 220

N_samp_img = 9*(N_backs_max + N_picks_max) # number of samples per image to export, set to 8 if just want to test CSV validity, 2000+ otherwise
dsfactor = 0.25 #1.0 to disable
olfactor = 0.025 # minimum fraction of ROI that must be in mini image to count images as Worm or Pick, otherwise background
fillfactor = 0.05 # % of the image that must be filled with worm pixels, doesn't work well
seg_type = "pick"
exclude_type = "tip"
#tip_mode = False # Tip mode code eliminated, if you want tip mode use V2
do_vert_flipping = True

# Set image parameters
grab_r = [15,25] # Range of radii to GRAB images from. Actual resized to r=10 regardless, but changes FOV
r = 10          # RADIUS of crop image size, actual crop image size is r*2
outsize = r*2
circle_filter = False # Set to inf to use the whole image

# Ask user to confirm
root = Tk()
root.withdraw()
pparent = filedialog.askdirectory(initialdir=pparent, title='Please select the PARENT directory')
root.destroy()

# Exit immediately if no folder provided
if len(pparent) == 0:
    print("No file provided - image generator will exit")
    time.sleep(2)
    exit(0)


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
d_image_sets = verifyDirlist(pparent,True,'_' + seg_type,exclude_type)[0]
exported_by_type = [0,0,0,0]
exported_by_folder = list()
rects = np.zeros(shape = (1,5)) # preallocate dummy old rects for comparing to previous CSV

for (pperson,i) in zip(d_image_sets,range(len(d_image_sets))):

    # Find the ROI files that show which ones have already been analyzed
    [d_roi,d_roi_short] = verifyDirlist(pperson,False,'_contmask.png');

    # Find the image associated with each csv and export small figures of worms
    exported_by_folder.append(len(d_roi_short))

    print('Starting folder ', i, ' of ', len(d_image_sets))

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

        # Extract ROI mask of contamination (actually, it's the pick outline)
        cont_img = cv2.imread(roiname,cv2.IMREAD_GRAYSCALE)
        cont_img = cv2.resize(cont_img,(0,0),fx = dsfactor, fy = dsfactor)
        cont_img = (cont_img>0).astype(np.uint8)

        # Skip image if no pick / worm / etc visible - NO, need these examples
        #if np.max(cont_img) == 0:
        #    print('Skipped (no object): ',imgname)
        #    continue
      


        # Anything from the testing folder is not for training
        is_train = not 'TEST' in roiname
        
        # Data augmentation angle:
        angle = 0
        flipvert = 0
        this_exported_pick = 0
        this_exported_back = 0

        # Select 1000 random crop images and determine whether the contain a worm centroid to sort
        for i in range(N_samp_img):

            # Rotate the image randomly within +/- 3 degrees every 10 frames
            if i%10==0:
                angle = 6*(np.random.rand() - 0.5)
                img_use = imutils.rotate(img,angle)
                cont_img_use = imutils.rotate(cont_img,angle)
                flipvert = np.random.rand() > 0.5

            # Select grab radius
            this_r = np.random.randint(grab_r[0],grab_r[1])

            # Draw random x and y coordinates
            xrand = np.random.randint(this_r+2,img_use.shape[1]-this_r-3)
            yrand = np.random.randint(this_r+2,img_use.shape[0]-this_r-3)

            # If using circle filter, exclude randoms far from center (e.g. corners)
            xbar = img_use.shape[1]/2 
            ybar = img_use.shape[0]/2
            imagerad = xbar # Also the radius...
            dist = np.sqrt((xrand-xbar)**2 + (yrand-ybar)**2)
            
            if circle_filter and (dist > (0.7*imagerad)):
                continue;


            # Determine whether any worm centroids are in this region - OR, if using tip mode, just check if tip inside
            this_roi = np.zeros(img_use.shape)
            this_roi = cv2.rectangle(this_roi,(xrand-r,yrand-r),(xrand+r,yrand+r),(1,1,1),-1)
            overlap = cont_img_use * this_roi
            Area = np.sum(this_roi)
            OverlapArea = np.sum(overlap)
            OverlapFrac = OverlapArea/Area
            FillFrac = OverlapArea / (2*this_r)**2

            contains_any = OverlapFrac > olfactor and FillFrac > fillfactor


            # Extract crop image
            crop_img = img_use[yrand-r:yrand+r,xrand-r:xrand+r]

            # Flip the image if requested
            if flipvert and do_vert_flipping:
                crop_img = cv2.flip(crop_img,flipCode = 0)

            # If the image grab radius != the real radius, resize
            if this_r != r:
                crop_img = cv2.resize(crop_img,(2*r,2*r))

            # Restrict imbalance between pick and background images
            if contains_any and this_exported_pick > N_picks_max:
                continue

            elif not contains_any and this_exported_back > N_backs_max:
                continue

            # Save it in the appropriate folder
            if contains_any and is_train:
                exported_by_type[0]+=1
                fout = imghead.replace(".png","_worm_" + str(exported_by_type[0]).zfill(4) + '.png')
                fout = os.path.join(pout_pick_train,fout)
                this_exported_pick += 1

            elif contains_any and not is_train:
                exported_by_type[1]+=1
                fout = imghead.replace(".png","_worm_" + str(exported_by_type[1]).zfill(4) + '.png')
                fout = os.path.join(pout_pick_test,fout)
                this_exported_pick += 1

            elif not contains_any and is_train:
                exported_by_type[2]+=1
                fout = imghead.replace(".png","_bkg_" + str(exported_by_type[2]).zfill(4) + '.png')
                fout = os.path.join(pout_back_train,fout)
                this_exported_back += 1

            elif not contains_any and not is_train:
                exported_by_type[3]+=1
                fout = imghead.replace(".png","_bkg_" + str(exported_by_type[3]).zfill(4) + '.png')
                fout = os.path.join(pout_back_test,fout)
                this_exported_back += 1
                
            cv2.imwrite(fout,crop_img)


        print('Finished plate ',shortroiname)

# Print the counts for each folder
for (proiimgs,counted) in zip(d_image_sets,exported_by_folder):
    (trash,name) = os.path.split(proiimgs)
    print(name,' = ',counted, ' Labeled wells' )

# Print overall numbers of the data size
print('Worms (training) = ', exported_by_type[0])
print('Worms (testing) = ', exported_by_type[1])
print('Backg (training) = ', exported_by_type[2])
print('Backg (testing) = ', exported_by_type[3])

