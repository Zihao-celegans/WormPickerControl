"""
    * Adapted from CNN_ExportWormBackgroundIMages.py - all requirements apply.

    * Additional requirement: One folder of labels to use as the gold standard. 


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


# Helper function for contains any worm centroid
def containsAnyCentroid(xbar,ybar,xrand,yrand,r):

    contains_xbar = (xbar < xrand+r) & (xbar > xrand-r)
    contains_ybar = (ybar < yrand+r) & (ybar > yrand-r)
    contains_ctr = contains_xbar & contains_ybar
    contains_any = np.any(contains_ctr)
    return contains_any


# Suppress empty CSV warnings, these are okay
import warnings
warnings.filterwarnings("ignore")


# Set folder that contains the ROI images datasets, divided into various people's groups
pparent = r'C:\Dropbox\WormWatcher (shared data)\Well score training'
pgold = os.path.join(pparent,'JDH_Relabels')
N_samp_img = 2000 # number of samples per image to export, set to 8 if just want to test CSV validity, 2000+ otherwise

# Set image parameters
r = 10          # RADIUS of crop image size, actual crop image size is r*2
dsfactor = 0.5  # Downsmaple to half resolution

# Ask user to confirm
#root = Tk()
#root.withdraw()
#pparent = filedialog.askdirectory(initialdir=pparent, title='Please select the PARENT directory')
#root.destroy()

# Exit immediately if no folder provided
if len(pparent) == 0:
    print("No file provided - image generator will exit")
    time.sleep(2)
    exit(0)


# Find the CSV files in the gold standard directory
[d_gold,d_gold_short] = verifyDirlist(pgold,False,".csv");


# Find the dataset folders in this folder
d_image_sets = verifyDirlist(pparent,True,'ImagesToLabel')[0]
correct = 0
total = 0
exported_by_folder = list()
rects = np.zeros(shape = (1,5)) # preallocate dummy old rects for comparing to previous CSV
rectsg= rects;

for proiimgs in d_image_sets:

    # Find the CSV files that show which ones have already been analyzed
    [d_csv,d_short] = verifyDirlist(proiimgs,False,".csv");

    # Find the image associated with each csv and export small figures of worms
    export_size = 40
    exported_by_folder.append(len(d_short))

    for (csvname,shortname) in zip(d_csv,d_short):

        # Get associated gold standard file, if any
        goldname = os.path.join(pgold, shortname)
        if goldname in d_gold:
            print('Found matching gold standard file ', shortname, '...')
        else:
            continue


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

        # Extract a list of worm centers, making sure no duplicates with old rects, from both csvname and gold name
        rects, ctrs = safeImportBoundingBoxes(csvname,dsfactor,rects)
        xbar = ctrs[:,0]
        ybar = ctrs[:,1]

        rectsg, ctrsg = safeImportBoundingBoxes(goldname,dsfactor,rectsg)
        xbarg = ctrsg[:,0]
        ybarg = ctrsg[:,1]       


        # Select 1000 random crop images and determine whether the contain a worm centroid to sort
        for i in range(N_samp_img):

            # Draw random x and y coordinates
            xrand = np.random.randint(r,img.shape[1]-r-1)
            yrand = np.random.randint(r,img.shape[0]-r-1)

            # Determine whether any worm centroids are in this region on BOTH the gold and main CSV files
            contains_any = containsAnyCentroid(xbar,ybar,xrand,yrand,r)
            contains_any_gold = containsAnyCentroid(xbarg,ybarg,xrand,yrand,r)

            # Keep track of results
            total += 1
            correct += int(contains_any==contains_any_gold)


# Print the counts for each folder
print(correct, ' out of ', total, ' are correct: ',float(correct)/float(total) )

# Print overall numbers of the data size
#print('Worms (training) = ', exported_by_type[0])
#print('Worms (testing) = ', exported_by_type[1])
#print('Backg (training) = ', exported_by_type[2])
#print('Backg (testing) = ', exported_by_type[3])
#print('Downsample factor:',dsfactor)

