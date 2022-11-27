"""

exportWormMotionTestImage.py

Complementary with CNN_exportWormMotionBackground.py

Export synthesized "color" images, containing the motion information, from WormPicker.
frame i-t, i will be embedded into two channels of the "color" image

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

# Set folder that contains the raw images
pparent = r'C:\WormPicker\Samples\Images for Worm Motion DNN'
# Create the output directories
praw = os.path.join(pparent,'raw')
pout = os.path.join(pparent,'output')

all_dirs = [pout]

for this_dir in all_dirs:
    if not os.path.exists(this_dir):
        os.makedirs (this_dir)

    print("made dir ",this_dir)


frame_interval = 1; # The number of frame seperated apart
                    # This is not the exact frame number difference, 
                    # but the difference in the ranking within the directory

exported_by_type = [0]

# Find the raw images in the file
[raw, raw_short] = verifyDirlist(praw,False,'.png');

for (img_name,short_img_name) in zip(raw, raw_short):

    # Make sure the this image has precedent frame
        idx_img = raw_short.index(short_img_name)

        if idx_img < frame_interval:
            print('Skipped (no precedent image for this image): ',img_name)
            continue

        if (idx_img + frame_interval) >= len(raw_short):
            print('Skipped (no successive image for this image): ',img_name)
            continue

        # Load the image as grayscale and resize to standard resolution so that the output size has a good resolution
        img = cv2.imread(img_name,cv2.IMREAD_GRAYSCALE)

        # Load the precedent frame
        img_pre = cv2.imread(raw[idx_img-frame_interval],cv2.IMREAD_GRAYSCALE)

        # Load the successor frame
        img_suc = cv2.imread(raw[idx_img+frame_interval],cv2.IMREAD_GRAYSCALE)

        # Synthesize the "color" image
        img_syn = np.zeros((img.shape[0], img.shape[1], 3))
        img_syn [:,:,0] = img_pre
        img_syn [:,:,1] = img
        img_syn [:,:,2] = img_suc

        # save the Synthesize "color" image
        exported_by_type[0]+=1
        fout = os.path.join(pout, "test_" + str(exported_by_type[0]).zfill(4) + '.png')
        cv2.imwrite(fout, img_syn)

# Print overall numbers of the data size
print('Images (testing) = ', exported_by_type[0])
