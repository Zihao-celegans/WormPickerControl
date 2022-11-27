"""

    CNN_AugmentEqualizeClasses

    Force the classes in a deep learning dataset folder to have the same number of elements by augmenting

    Designed for use with color images like for CNN_LabelWormMotion

    Designed for folders NOT split into train/test - this does it

    Cannot apply zoom augments, these must be applied before exporting the dataset

"""
from verifyDirlist import verifyDirlist
import csvio
import cv2
import numpy as np
import os
from tkinter import filedialog
import tkinter as tk
from scipy.signal import medfilt2d, correlate2d
from math import ceil

# Parameters
pdata = r'C:\CNN_Datasets\SortedColor01c'
N_per_class = 20000 # must be larger than any of the classes
noise_coeff = 5 #uint8 units
test_frac = 0.25; # Fraction to use as testing, 1-f used for training. E.g. if NPC is 1000, 750 train 250 test

# Augment and export loop

def augmentAndExport(fullimages,shortimages,shortclass,subfolder,N_per_class):
    
    # Loop over each image in this class
    N_samples = len(fullimages)
    N_exported_this_class = 0
    N_per_image = ceil(N_per_class / N_samples)

    # Verify N per image
    if N_per_image < 1:
        print("ERROR: N_per_class must have more than any original folders image count")
        exit(-1)


    # Export augments
    for (fullimage,shortimage) in zip(fullimages,shortimages):

        # Load the image
        img = cv2.imread(fullimage,cv2.IMREAD_COLOR);

        # Apply N_per_image augmentations and save
        for i in range(N_per_image):

            # EXACT class numbers - quit if reach exact number
            N_exported_this_class += 1
            if N_exported_this_class >= N_per_class:
                print("Reached N_per_class: ", N_per_class, " in ", subfolder)
                return

            # Augment 1: Swap channels
            swap = np.random.permutation(3)
            img_use = img[:,:,swap]

            # Augment 2: Add random noise
            noise = 2*(np.random.rand(img.shape[0],img.shape[1],img.shape[2]) - 0.5)*noise_coeff # noise from -noise_coeff to +noise_coeff
            img_use = img_use + noise

            # Augment 3: Rotate in intervals of 90 degrees
            rotate_method = np.random.randint(0,4)
            if rotate_method == 0:
                None

            elif rotate_method == 1:
                img_use = cv2.rotate(img_use,cv2.ROTATE_180)

            elif rotate_method == 2:
                img_use = cv2.rotate(img_use,cv2.ROTATE_90_CLOCKWISE)

            elif rotate_method == 3:
                img_use = cv2.rotate(img_use,cv2.ROTATE_90_COUNTERCLOCKWISE)

            # Augment 4: Flip horizontal
            if np.random.rand() > 0.5:
                img_use = cv2.flip(img_use,1)

            # Augment 5: Flip vertical
            if np.random.rand() > 0.5:
                img_use = cv2.flip(img_use,0)

            # Save output image
            fout = shortimage.replace('.png','_aug' + str(i) + '.png')
            fout = os.path.join(pout,subfolder,shortclass,fout)
            cv2.imwrite(fout,img_use)


# Find classes
fullclasses,shortclasses = verifyDirlist(pdata,True)

# Set output folder
root,name = os.path.split(pdata)
pout = os.path.join(r"C:\CNN_Datasets",name + "_AugEq")

# Create all output folders
for this_class in shortclasses:
    for this_sub in ['test','train']:
        if not os.path.exists(os.path.join(pout,this_sub,this_class)):
            os.makedirs (os.path.join(pout,this_sub,this_class))
    

# Cycle through class folders
for (fullclass,shortclass) in zip(fullclasses,shortclasses):


    print("starting class folder " , shortclass)

    # output folder
    fullout = os.path.join(pout,shortclass)

    # Calculate how many augmentations need to be applied to each image to reach the designed number out
    fullimages,shortimages = verifyDirlist(fullclass,False,'png')

    
    # Export TRAINING data for this class
    cutoff = int(0.25*len(fullimages))
    augmentAndExport(fullimages[cutoff:],shortimages[cutoff:],shortclass,'train',0.75*N_per_class)

    # Export TESTING data for this class
    augmentAndExport(fullimages[:cutoff],shortimages[:cutoff],shortclass,'test',0.25*N_per_class)


