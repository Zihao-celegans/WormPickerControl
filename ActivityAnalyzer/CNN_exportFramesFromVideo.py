"""
    Export frames from video files, e.g. for R-CNN bounding box labeling.
    Assumes videos of different class objects are in separate folders named classes (below)

"""


# Params
skip = 5
N_frames_per_class = 1000
vid_filt = "high_mag"
vid_not_filt = "color"
classes = ["male","herm"]

# Set working directory (must be done manually for Jupyter)
import sys
sys.path.append(r"C:\Users\antho\source\repos\adfouad\Autogans\ActivityAnalyzer\ActivityAnalyzer")


# Find male and herm video folders
import numpy as np
import matplotlib.pyplot as plt
import cv2
import os
from verifyDirlist import *

pin = r"C:\Dropbox\FramesData\2021-03-25 Male_and_Herm new Optics" # Where the videos are
pout = r"C:\CNN_Datasets\Head_Tail_Gender_Faster_R_CNN_01" # Where the image files go

folders = verifyDirlist(pin,True,"","")[0]

for i,folder in enumerate(folders):

    num_frames_from_folder = -1

    # Is this male or herm?
    _,head = os.path.split(folder)
    if classes[0] in head.lower():
        prefix = classes[0] + "_"
    elif classes[1] in head.lower():
        prefix = classes[1] + "herm_"

    # find video files in this folder
    vidnames = verifyDirlist(folder,False,vid_filt,"color")[0]

    for j,vidname in enumerate(vidnames):

        # Load video
        cap = cv2.VideoCapture(vidname)

        # Get frames, every 5 frames
        k=-1
        while(cap.isOpened()):
            k+=1
            num_frames_from_folder+=1

            # End if hit the debug limit of n frames
            if num_frames_from_folder > N_frames_per_class:
                break

            # Get frame until end
            ret, frame = cap.read()
            if not ret:
                break

            # Skip to every 5th frame
            if k%skip != 0:
                continue

            # Normalize the frame
            frame = cv2.normalize(frame,None,alpha=0,beta=255, norm_type=cv2.NORM_MINMAX)

            # Write frame in grayscale
            #frame = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
            fout = os.path.join(pout,prefix + "vid=" + str(j) + "_frame=" + str(num_frames_from_folder) + ".png")
            cv2.imwrite(fout,frame)


        # Release vidcap
        cap.release()

        # FINALIZE the End if hit the debug limit of n frames
        if num_frames_from_folder >= N_frames_per_class:
            break
