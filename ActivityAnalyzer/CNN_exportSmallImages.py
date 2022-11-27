"""
    CNN_exportSmallImages.py
    May 2021
    Anthony Fouad

                     Centralized data exporter for small (eg. 20x20) images to be fed to CNN classifier

    Export small 20x20 images from a larger image, sorting them into an arbitrary number of classes (folders)
    The small images are designed to be fed to a classifier, like a regular CNN, as opposed to an object-
    detector like an R-CNN.

    pparent must contain sub folders and files:
        parent/"DatasetImages" with images and 
        parent/"DatasetMasks" with masks
        parent/"dataset_export_params.json" SAMPLE FILE CONTENTS:
            {
                "classes": ["background","tail_herm","tail_male","head"],
                "mask_encoding": [[0,-1,-1],
                            [0,150,255],
                            [1,150,255],
                            [2,150,255]],
                "dsfactor" : 1.0,
                "N_samples_per_class": 8,
                "mod_denominator": 4,
                "r" : 20,

                "comments":[
                            "classes: must include background and number matches length of values",
                            "values[i]: first number is channel for that class, second is pixel val ranges for that class",
                            "dsfactor: 1.0 to disable downsampling, 0.15-0.9 to downsample",
                            "N_samples_per_class: set low number for initial testing and up to 2000 for huge dataset",
                            "mod_denominator: set 4 for 1/4 of images used to extract test samples",
                            "r: radius of images extracted, for example r=10 makes a 20x20 image"
                            ]
            }

    adapted from CNN_exportWormBackground.py

    Designed for data set C:\Dropbox\FramesData\Mask_RCNN_Datasets\GenderDatasets\Preliminary_Gender_Dataset
    created by PDB May 2021.
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
import json
import math

from CNN_EvaluateManualSegmentations import drawROIs
from CNN_EvaluateManualSegmentations import safeImportBoundingBoxes

# Folders
pparent = r'C:\Dropbox\FramesData\Mask_RCNN_Datasets\GenderDatasets\Combined_ADF-YF-ZL_WholeHeadMasks' #Must have DatasetImages and DatasetMasks
pout = r'C:\CNN_Datasets\Head_Tail_Gender_CNN_08'
ptest = os.path.join(pout,'test')
ptrain = os.path.join(pout,'train')

# Parameters
with open(os.path.join(pparent,"dataset_export_params.json")) as fin:
    params  = json.load(fin)
N_samples_per_class = params["N_samples_per_class"] # number of samples per image per class. However, class will be skipped if not present in image
mod_denominator = params["mod_denominator"] # Fraction of images to set aside for testing, 4 = 1/4 for testing
dsfactor = params["dsfactor"] # 1.0 to disable
classes = params["classes"]
mask_encoding = params["mask_encoding"]
                 # Where each of the classes is encoded in the mask images.
                 # [channel, start_val, end_val]
                 # Order and number of elements much match classes order and number of elements
                 # start/end of -1 are for background and just mean none of the other

# Set image parameters
r = params["r"]          # RADIUS of crop image size, actual crop image size is r*2
z = int(6/20*r)          # Zoom radius variation of about +/- 0.25r
grab_r = [r-z,r+z] # Range of radii to GRAB images from. Actual resized to r=r regardless, but changes FOV/zoom
outsize = r*2

# function for getting the coordinates of all points in a ROTATED crop image,
# i.e. to extract crop images with an arbitrary rotation augment.
#
#   1. Shift centroid of coordinates to the origin
#   2. Use matrix multiplication to rotate about origin
#   3. Shift centroid back to where it was
#   4. Also report the bounding min/max of x and y for use in 



def extractRotatedCropImage(img, r, xrand, yrand, th_rad):

    # Generate a list of the points in the region that need to be rotated
    xx = np.arange(-r, +r)  # Start at origin to rotate using matrix. Have to add xrand/yrand at end
    yy = np.arange(-r, +r)
    XX, YY = np.meshgrid(xx,yy)

    # Matrix multiply each point by the rotation matrix, and populate the image
    crop_img = np.zeros((2*r,2*r),dtype=np.uint8)

    for r in range(crop_img.shape[0]):
        for c in range(crop_img.shape[1]):

            # Rotate the coordinate about the center of the crop_image
            x = xrand + int(XX[r,c] * np.cos(th_rad) - YY[r,c] * np.sin(th_rad))
            y = yrand + int(XX[r,c] * np.sin(th_rad) + YY[r,c] * np.cos(th_rad)) # add xrand/yrand to shift rotated coord back to real potition

            # safety check that x,y is in bounds
            if y > img.shape[0] or x > img.shape[1] or x < 0 or y < 0:
                print('rotated coordinate (x,y) = (%d,%d) is out of bounds. Aborting!'%(x,y))
                exit(-1)

            # Grab data from rotated coordinate and store it in out img
            crop_img[r,c] = img[y,x]

    # DEBUG
    # cv2.imshow("CROP IMG",crop_img)
    # cv2.waitKey(0)

    # Return the rotated image
    return crop_img

    pass


# function for extracting and agumenting the little portion of the image
def getSmallImageWithAugmentations(img,r,xrand,yrand):


    # Augment #0: Select a random point within 10%*r distant from the rand point
    xrand +=int((np.random.rand()-0.5)*0.25*r)
    yrand +=int((np.random.rand()-0.5)*0.25*r)

    # Augment #1: Crop the image using requested zoom, prevent OOB access
    # In preventing OOB it's helpful if extreme edge objects are eliminated from the masks
    # This is done earlier at class mask computation.
    this_r = np.random.randint(grab_r[0],grab_r[1])
    safe_r = math.ceil(np.sqrt(2)*this_r)+1 # This is the safe radius size allowing for rotated images
    xrand = max((safe_r,xrand))
    xrand = min((img.shape[1]-1-safe_r,xrand))
    yrand = max((safe_r,yrand))
    yrand = min((img.shape[0]-1-safe_r,yrand))

    # Augment #2: Rotate the extracted image
    th_rad = np.random.rand()*2*np.pi


    # Instead of simple cropping, extract a rotated and cropped image
    crop_img = extractRotatedCropImage(img, this_r, xrand, yrand, th_rad)
    #crop_img = img[ymin:ymax,xmin:xmax]

    # Resize cropped image to correct size 
    crop_img = cv2.resize(crop_img,(2*r,2*r))

    # Augment #3: Flipping
    if np.random.normal() > 0.5:
        crop_img = cv2.flip(crop_img,flipCode = 0)

    if np.random.normal() > 0.5:
        crop_img = cv2.flip(crop_img,flipCode = 1)        

    # Augment #4: Random noise
    rand_filter = np.random.rand(2*r,2*r) * 10 # Don't use 255, it will overwhelm images with noise
    crop_img = crop_img + rand_filter.astype(np.uint8)

    # Augment #5: Gaussian blur
    sigma = int(np.random.rand(1,1)*3)
    crop_img = cv2.GaussianBlur(crop_img,(3,3),sigmaX=sigma,sigmaY=sigma)

    # Return augmented image
    return crop_img




# function for getting random coordinates that are within a mask of interest
def getRandomCoords(r,mask):

    # Extract image size. 
    width = mask.shape[1]
    height = mask.shape[0]

    # Coordinates selected must be at least r units away from edges
    min_x = r+1
    min_y = r+1
    max_x = width-r-1
    max_y = width-r-1

    # Find all coordinates that are True in the mask and in bounds
    coords = np.where(mask)
    idx_keep = (coords[0] > min_y) & (coords[0] < max_y) & (coords[1] > min_x) & (coords[1] < max_x)

    # Return None if no valid in range object (less than 5 pixels)
    if np.sum(idx_keep) < 5:
        #print("ERROR: did not find any coordinates in-bounds in the image")
        return None, None

    # Select coordinate randomly from within valid coordinates
    coords_r = coords[0][idx_keep]
    coords_c = coords[1][idx_keep]

    idx_keep = np.random.randint(0,len(coords_c)-1,)
    yrand = coords_r[idx_keep]
    xrand = coords_c[idx_keep]

    # Return random coordinates
    return xrand,yrand


# Mini function for adding and creating a directory for outputs
def addOutputDir(dirs_list, pout_train_or_test, this_class):

    # Generate full output path
    this_dir = os.path.join(pout_train_or_test,this_class)

    # Add it to the list of dirs
    dirs_list += [os.path.join(ptrain,this_class)]

    # Check that the path already exists or creat it
    if not os.path.exists(this_dir):
        os.makedirs (this_dir)
        print("made dir ",this_dir)
    else:
        print("dir already exists: ",this_dir)

    # Return updated dirs list
    return dirs_list

train_dirs = list()
test_dirs = list()
for this_class in classes :

    train_dirs = addOutputDir(train_dirs, os.path.join(pout,"train"), this_class)
    test_dirs = addOutputDir(test_dirs, os.path.join(pout,"test"), this_class)

# Add checkpoint folder, don't save in train/test lists
addOutputDir([],pout,"checkpoint")

# Find the dataset folders in this folder
pimg = os.path.join(pparent,"DatasetImages")
pmask = os.path.join(pparent,"DatasetMasks")
d_images = verifyDirlist(pimg,False,".png")[0]
d_masks, d_masks_short = verifyDirlist(pmask,False,".png")
exported_by_type = np.zeros((len(classes),2),dtype=np.int64) # rows = classes, cols = train and test


# Get individual masks from mask folder, AND the corresponding image from image folder
for n, maskname, shortname in zip(range(len(d_masks)),d_masks, d_masks_short):

    # Print status 
    if n % 10 ==0:
        print("Exporting from image %d of %d" % (n,len(d_masks)))

    # Determine whether this image will be used to extract samples for training or testing 
    
    if n % mod_denominator == 0:
        train_or_test = "test"
        exp_idx = 0

    else:
        train_or_test = "train"
        exp_idx = 1

    # Get associated imagename, which should be in a different folder
    imgname = os.path.join(pimg, shortname)
    imgname = imgname.replace("_mask","")

    # Error if not exist
    if not os.path.exists(imgname):
        print('Skipped (no image): ',imgname)
        continue

    # Load the image as grayscale and resize to standard resolution so that the output size has a good resolution
    img = cv2.imread(imgname,cv2.IMREAD_GRAYSCALE)
    img = cv2.resize(img,(0,0), fx = dsfactor, fy = dsfactor)

    # Extract Mask, which can be a color image with different classes in different channels
    mask = cv2.imread(maskname,cv2.IMREAD_COLOR)
    mask = cv2.resize(mask,(0,0),fx = dsfactor, fy = dsfactor)
    
    for c, this_class in enumerate(classes):

        # Generate a mask of where (if anywhere) this class is in the image
        channel=mask_encoding[c][0]
        val0 = mask_encoding[c][1]
        val1 = mask_encoding[c][2]

        if val0 < 0 or val1 < 0: # NEgative values signify background - none of other classes
            class_mask = np.max(mask,axis=2)
            class_mask = ~(class_mask>0)
        
        else:
            class_mask = (mask[:,:,channel] >= val0) & (mask[:,:,channel] <= val1)

        class_mask = class_mask.astype(np.uint8)

        # Exclude image borders from consideration to reduce downstream OOB (another check occurs at point selection)
        brdr = int(np.max(grab_r)*np.sqrt(2))
        R,C = img.shape
        class_mask[:brdr,:] = 0
        class_mask[:,:brdr] = 0
        class_mask[R-brdr:-1,:] = 0
        class_mask[:,C-brdr:-1] = 0

        # Skip class if not present in image
        if np.max(class_mask) == 0:
            continue


        for i in range(N_samples_per_class):

            # Generate random coordinates that include this class
            xrand,yrand = getRandomCoords(r,class_mask)

            if xrand is None: # Skip export if no valid point available
                continue

            # Extract and agument little image
            crop_img = getSmallImageWithAugmentations(img,r,xrand,yrand)

            # DEBUG:
            # print("CLASS = ", this_class)            
            # cv2.imshow("CROP IMG",crop_img)
            # cv2.waitKey(0)

            # Set output file name
            fout = os.path.join(pout,train_or_test,this_class,"crop_img_"+
                                str(exported_by_type[c,exp_idx]).zfill(4) + ".png")
            exported_by_type[c,exp_idx] += 1

            # Save file to disk
            cv2.imwrite(fout,crop_img)

            pass
        

# Print overall numbers of the data size

for i, this_class in enumerate(classes):
    print(this_class,' (test/train)\t\t- ', exported_by_type[i,:])

pass


