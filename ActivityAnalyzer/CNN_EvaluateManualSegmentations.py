"""
    Compare manual segmentations to Anthony's to evaluate a manual segmenter's performance.
"""

from verifyDirlist import *
from csvio import *
import os
import numpy as np
from shutil import copyfile
import matplotlib.pyplot as plt
import cv2
from tkinter import messagebox
from tkinter import filedialog
from tkinter import Tk
import time


""" 
    Helper function to safely import bounding boxes using np.genfromtxt 
    - If there's only 1 box, the shape is set to (1,5) instead of (5,)
    - If there are no boxes, then one row of nans is added that can be easily handled downstream

"""

def safeImportBoundingBoxes(csvname, dsfactor, old_rects, max_worms = 100):

    # Load CSV tables
    rects = np.genfromtxt(csvname, delimiter=',',skip_header = 1)

    # Special case: No rows, all background
    if len(rects.shape) == 1 and rects.shape[0] == 0:
        rects = np.zeros(shape=(1,5))
        rects[:] = np.nan

    # Special case: only one row, so forms 1d array
    elif len(rects.shape) == 1 and rects.shape[0] == 5:
        rects = np.reshape(rects,(1,5))

    # Resize the bounding boxes if requesteddsfactor
    rects = rects * dsfactor

    # Compute the centroids of each rectangle
    xbar = np.reshape(rects[:,1] + rects[:,3]/2,(rects.shape[0],1))
    ybar = np.reshape(rects[:,2] + rects[:,4]/2,(rects.shape[0],1))
    ctrs = np.concatenate((xbar,ybar),axis=1)

    # Check whether there are any duplicates with the previous xbar/ybar
    any_matching = False
    for row in rects:
        any_matching = any_matching or np.any(np.min(old_rects==row,axis=1))
        if any_matching:
            print(row)

    # Check whether it has >100 worms, a sign of an incorrect CSV
    if (np.max(np.shape(xbar)) > max_worms) or any_matching:
        msg = csvname + ' has too many worms or duplicate worms with the previous file. Please correct and re-run.\nN worms is: ' + str(len(xbar))
        msg += '\nFor duplicate points see command window.'
        messagebox.showerror('Invalid CSV',msg)
        sys.exit(0)



    return (rects,ctrs)


def drawROIs(all_roi,frect):

    # Load images and csv tables
    rects = safeImportBoundingBoxes(frect)

    for row in rects:

        # Skip if it's nan (e.g. skipped or no roi in the image)
        if np.isnan(row[1]):
            continue

        # Extract this ROI
        x = int(row[1])
        y = int(row[2])
        w = int(row[3])
        h = int(row[4])
    
        this_roi = np.zeros(shape=all_roi.shape[0:3])
        this_roi = cv2.rectangle(this_roi,(x,y),(x+w,y+h),1,-1)

        # Add it to the overall ROI
        all_roi += this_roi

    return all_roi


def showOverlay(pparent):

    # Create a dummy hidden window
    root = Tk()
    root.withdraw()

    # No input arguments, ask for file selection
    ftest = filedialog.askopenfilename(initialdir = pparent,title = "Select file",filetypes = (("Image annotations","*.csv"),("all files","*.*")))
    root.destroy()



    # Exit immediately if no file provided
    if len(ftest) == 0:
        print("No file provided - comparison will exit")
        time.sleep(2)
        return ''
         
    # Determine corresponding image and gold standard file
    (pparent,fname) = os.path.split(ftest)
    pgold = r'C:\Dropbox\WormWatcher (shared data)\Well score training\ImagesToLabel_ADF'
    fimg = os.path.join(pgold, fname.replace('.csv','.png'))
    fgold = os.path.join(pgold, fname)

    # Make sure everything was found
    if not os.path.exists(ftest):
        messagebox.showerror("Error","Could not find test file:\n" + ftest)
        exit(0)

    if not os.path.exists(fgold):
        messagebox.showerror("Error","Could not find matching gold standard file :\n" + fgold + "\nfor test file:\n" + ftest)
        exit(0)

    if not os.path.exists(fimg):
        messagebox.showerror("Error","Could not find image file:\n" + fimg)
        exit(0)

    # Create images of the ROIs drawn by both files
    img = cv2.imread(fimg,cv2.IMREAD_GRAYSCALE)

    bw_gold = np.zeros(shape=img.shape[0:3])
    bw_test = np.zeros(shape=img.shape[0:3])

    bw_gold = drawROIs(bw_gold,fgold)
    bw_test = drawROIs(bw_test,ftest)

    # Determine the amount of overlap
    both        = (bw_gold == bw_test) & (bw_gold>0)
    adf_only    = (bw_gold>bw_test) 
    test_only   = (bw_gold<bw_test)

    overlap_display = np.zeros(shape=(img.shape[0],img.shape[1],3))
    overlap_display[:,:,1] = both       # GREEN: BOTH
    overlap_display[:,:,2] = test_only  # BLUE: TEST ONLY
    overlap_display[:,:,0] = adf_only   # RED: ADF ONLY

    DC = 2*np.sum(both) / (np.sum(bw_gold>0) + np.sum(bw_test>0))
    DC = round(DC,2)

    # Display
    plt.figure(1,figsize=(10,9))
    ax1 = plt.subplot(2,2,1)
    plt.imshow(img, cmap='gray', vmin=0, vmax=np.max(img))
    plt.title("Image")

    plt.subplot(2,2,2,sharex = ax1, sharey = ax1)
    max_val = np.max([np.max(bw_gold),np.max(np.max(bw_gold))])
    plt.imshow(bw_gold, cmap='jet', vmin=0, vmax=max_val)
    plt.title("Anthony")

    plt.subplot(2,2,3,sharex = ax1, sharey = ax1)
    plt.imshow(bw_test, cmap='jet', vmin=0, vmax=max_val)
    plt.title("test")

    plt.subplot(2,2,4,sharex = ax1, sharey = ax1)
    plt.imshow(overlap_display)
    plt.title("overlap - DC = " + str(DC))

    plt.show()
    return pparent

if __name__ == "__main__":

    startdir = r'C:\Dropbox\WormWatcher (shared data)\Well score training'
    while(True):
        newdir=showOverlay(startdir)

        if len(newdir)>0:
            startdir = newdir


