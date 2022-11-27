"""
    Instead of classifying the whole well as contaminated or not, export small images based on manually labeled contamination regions
    Just like for CNN_ExportWormBackgroundImages

    REQUIRES THE IMAGES OF WELLS WERE EXPORTED USING CNN_ExportROIContClean to FULL RESOLUTION, SINGLE WELL images (check code)

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

from CNN_EvaluateManualSegmentations import drawROIs
from CNN_EvaluateManualSegmentations import safeImportBoundingBoxes

# Suppress empty CSV warnings, these are okay
import warnings
warnings.filterwarnings("ignore")


# Set folder that contains the ROI images datasets, divided into various people's groups
pparent = r'C:\Dropbox\WormWatcher (shared data)\Well score training\Training_WormCamp_Contamination'
ptest = r'C:\ContaminationTest'
ptrain = r'C:\ContaminationTrain'
N_samp_img = 500 # number of samples per image to export, set to 8 if just want to test CSV validity, 2000+ otherwise

# Set image parameters
r = 10          # RADIUS of crop image size, actual crop image size is r*2
outsize = r*2
circle_filter = True # Set to inf to use the whole image

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
pout_clean_train = os.path.join(ptrain,"Clean")
pout_clean_test = os.path.join(ptest,"Clean")
pout_cont_train = os.path.join(ptrain,"Contaminated")
pout_cont_test = os.path.join(ptest,"Contaminated")
all_dirs = [pout_clean_train,pout_clean_test,pout_cont_train,pout_cont_test]


for this_dir in all_dirs:
    if not os.path.exists(this_dir):
        os.makedirs (this_dir)

    print("made dir ",this_dir)


# Find the dataset folders in this folder
d_image_sets = verifyDirlist(pparent,True,'ImagesToLabel')[0]
exported_by_type = [0,0,0,0]
exported_by_folder = list()
rects = np.zeros(shape = (1,5)) # preallocate dummy old rects for comparing to previous CSV

for pperson in d_image_sets:

    # Find the ROI files that show which ones have already been analyzed
    [d_roi,d_roi_short] = verifyDirlist(pperson,False,'_contmask.png');

    # Find the image associated with each csv and export small figures of worms
    exported_by_folder.append(len(d_roi_short))

    for (roiname,shortroiname) in zip(d_roi,d_roi_short):

        # Get associated image
        imgname = roiname.replace('_contmask.png','.png')
        imgname = imgname.replace('_contmask.png','.png')

        (trash,imghead) = os.path.split(imgname)

        # Error if not exist
        if not os.path.exists(imgname):
            print('Skipped (no image): ',imgname)

        # Load the image as grayscale and resize to standard resolution so that the output size has a good resolution
        img = cv2.imread(imgname,cv2.IMREAD_GRAYSCALE)
        img = cv2.resize(img,(5*outsize,5*outsize))

        # Extract ROI mask of contamination
        cont_img = cv2.imread(roiname,cv2.IMREAD_GRAYSCALE)
        cont_img = cont_img
        cont_img = cv2.resize(cont_img,(5*outsize,5*outsize))
        cont_img = cont_img>0
       

        # Anything from the testing folder is not for training
        is_train = not 'Testing' in roiname

        # Select 1000 random crop images and determine whether the contain a worm centroid to sort
        for i in range(N_samp_img):

            # Draw random x and y coordinates
            xrand = np.random.randint(r,img.shape[1]-r-1)
            yrand = np.random.randint(r,img.shape[0]-r-1)

            # If using circle filter, exclude randoms far from center (e.g. corners)
            xbar = img.shape[1]/2 
            ybar = img.shape[0]/2
            imagerad = xbar # Also the radius...
            dist = np.sqrt((xrand-xbar)**2 + (yrand-ybar)**2)
            
            if circle_filter and (dist > (0.7*imagerad)):
                continue;


            # Determine whether any worm centroids are in this region
            this_roi = np.zeros(img.shape)
            this_roi = cv2.rectangle(this_roi,(xrand-r,yrand-r),(xrand+r,yrand+r),(1,1,1),-1)
            overlap = cont_img * this_roi
            Area = np.sum(this_roi)
            OverlapArea = np.sum(overlap)
            OverlapFrac = OverlapArea/Area

            contains_any = OverlapFrac > 0.25

            #----------------------- DEBUG

            #if 'ROI=3' in roiname:
            #    plt.imshow(cont_img)
            #    plt.show()
            #    plt.imshow(this_roi)
            #    plt.show()
            #    plt.imshow(overlap)
            #    plt.show()
            #    print(OverlapFrac)


            # Extract crop image
            crop_img = img[yrand-r:yrand+r,xrand-r:xrand+r]

            # Save it in the appropriate folder
            if contains_any and is_train:
                fout = imghead.replace(".png","_worm_" + str(i).zfill(4) + '.png')
                fout = os.path.join(pout_cont_train,fout)
                exported_by_type[0]+=1

            elif contains_any and not is_train:
                fout = imghead.replace(".png","_worm_" + str(i).zfill(4) + '.png')
                fout = os.path.join(pout_cont_test,fout)
                exported_by_type[1]+=1

            elif not contains_any and is_train:
                fout = imghead.replace(".png","_bkg_" + str(i).zfill(4) + '.png')
                fout = os.path.join(pout_clean_train,fout)
                exported_by_type[2]+=1

            elif not contains_any and not is_train:
                fout = imghead.replace(".png","_bkg_" + str(i).zfill(4) + '.png')
                fout = os.path.join(pout_clean_test,fout)
                exported_by_type[3]+=1
                
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
