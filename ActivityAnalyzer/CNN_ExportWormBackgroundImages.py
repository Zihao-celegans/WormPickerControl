"""
    * Requires that ExportROIImages was run first on the target folder *    
    * Requires that users have used the ImageJ protocol to label some of the ROI images for worms *



    Export cropped images of a single worm or non-work background for use in basic NN training
        REWRITTEN ON 11/25/2019 TO EXPORT RANDOMIZED LOCATION IMAGES INSTEAD OF CENTERED-WORM IMAGES 


    *** TIP ***

    Check if there is anything wrong with the CSVs first by running with N_samp_img = 8
    Then scale it up to about 1200 samples per image for the real set generation



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
pparent = r'C:\Dropbox\WormWatcher (shared data)\Well score training'
ptest = r'C:\test'
ptrain = r'C:\train'
N_samp_img = 900 # number of samples per image to export, set to 8 if just want to test CSV validity, 2000+ otherwise

# Set image parameters
r = 10          # RADIUS of crop image size, actual crop image size is r*2
dsfactor = 0.5  # Downsmaple to half resolution



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
pout_worm_train = os.path.join(ptrain,"worm")
pout_worm_test = os.path.join(ptest,"worm")
pout_bkg_train = os.path.join(ptrain,"background")
pout_bkg_test = os.path.join(ptest,"background")
all_dirs = [pout_worm_train,pout_worm_test,pout_bkg_train,pout_bkg_test]


for this_dir in all_dirs:
    if not os.path.exists(this_dir):
        os.makedirs (this_dir)

    break

# Find the dataset folders in this folder
d_image_sets = verifyDirlist(pparent,True,'ImagesToLabel')[0]
exported_by_type = [0,0,0,0]
exported_by_folder = list()
rects = np.zeros(shape = (1,5)) # preallocate dummy old rects for comparing to previous CSV

for proiimgs in d_image_sets:

    # Find the CSV files that show which ones have already been analyzed
    [d_csv,d_short] = verifyDirlist(proiimgs,False,".csv");

    # Find the image associated with each csv and export small figures of worms
    export_size = 40
    exported_by_folder.append(len(d_short))

    for (csvname,shortname) in zip(d_csv,d_short):

        # Get associated image
        imgname = csvname.replace('.csv','.png')
        imgname = imgname.replace('.CSV','.png')

        (trash,imghead) = os.path.split(imgname)

        # Error if not exist
        if not os.path.exists(imgname):
            print('Skipped (no image): ',imgname)

        # Load the image as grayscale and reduce resolution
        img = cv2.imread(imgname,cv2.IMREAD_GRAYSCALE)
        img = cv2.resize(img,(0,0),None,dsfactor,dsfactor)

        # Extract a list of worm centers, making sure no duplicates with old rects
        rects, ctrs = safeImportBoundingBoxes(csvname,dsfactor,rects)
        xbar = ctrs[:,0]
        ybar = ctrs[:,1]

        # Anything from the testing folder is not for training
        is_train = not 'Testing' in csvname

        # Select 1000 random crop images and determine whether the contain a worm centroid to sort
        for i in range(N_samp_img):

            # Draw random x and y coordinates
            xrand = np.random.randint(r,img.shape[1]-r-1)
            yrand = np.random.randint(r,img.shape[0]-r-1)

            # Determine whether any worm centroids are in this region
            contains_xbar = (xbar < xrand+r) & (xbar > xrand-r)
            contains_ybar = (ybar < yrand+r) & (ybar > yrand-r)
            contains_ctr = contains_xbar & contains_ybar
            contains_any = np.any(contains_ctr)

            # Extract crop image
            crop_img = img[yrand-r:yrand+r,xrand-r:xrand+r]

            # Save it in the appropriate folder
            if contains_any and is_train:
                fout = imghead.replace(".png","_worm_" + str(i).zfill(4) + '.png')
                fout = os.path.join(pout_worm_train,fout)
                exported_by_type[0]+=1

            elif contains_any and not is_train:
                fout = imghead.replace(".png","_worm_" + str(i).zfill(4) + '.png')
                fout = os.path.join(pout_worm_test,fout)
                exported_by_type[1]+=1

            elif not contains_any and is_train:
                fout = imghead.replace(".png","_bkg_" + str(i).zfill(4) + '.png')
                fout = os.path.join(pout_bkg_train,fout)
                exported_by_type[2]+=1

            elif not contains_any and not is_train:
                fout = imghead.replace(".png","_bkg_" + str(i).zfill(4) + '.png')
                fout = os.path.join(pout_bkg_test,fout)
                exported_by_type[3]+=1
                
            cv2.imwrite(fout,crop_img)


        print('Finished plate ',shortname)

# Print the counts for each folder
for (proiimgs,counted) in zip(d_image_sets,exported_by_folder):
    (trash,name) = os.path.split(proiimgs)
    print(name,' = ',counted, ' Labeled wells' )

# Print overall numbers of the data size
print('Worms (training) = ', exported_by_type[0])
print('Worms (testing) = ', exported_by_type[1])
print('Backg (training) = ', exported_by_type[2])
print('Backg (testing) = ', exported_by_type[3])
print('Downsample factor:',dsfactor)
