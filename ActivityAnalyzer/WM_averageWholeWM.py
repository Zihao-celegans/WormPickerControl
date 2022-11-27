"""
    WM_averageWholeWM.py

    Create an "Average" whole wormotel, could be any size, but must have been carefully cropped first.

"""


import cv2
import os
import matplotlib.pyplot as plt
import numpy as np
import tkinter
from verifyDirlist import *

pcropped = r'C:\Dropbox\WormWatcher (shared data)\Wormotel simulations\SampleImages\SamplesFrom PDM AUXIN\Cropped kernel plates'
fout = r'C:\Dropbox\WormWatcher (shared data)\Wormotel simulations\SampleImages\SamplesFrom PDM AUXIN\ModelWM240.png'
fullfiles = verifyDirlist(pcropped,False,'png')[0]

all_img = np.zeros((0,0,0))

for [fullfile,i] in zip(fullfiles,range(len(fullfiles))):

    # Load current image
    img = cv2.imread(fullfile,cv2.IMREAD_GRAYSCALE)


    # Resize to identical shape
    dsfrac = 4

    # Preallocate stack if first image
    if i==0:
        img = cv2.resize(img,(int(img.shape[1]/dsfrac),int(img.shape[0]/dsfrac)))
        img_shape = img.shape
        all_img = np.zeros(img.shape+(len(fullfiles),)).astype(np.uint8)
    else:
        img = cv2.resize(img,(int(img_shape[1]),int(img_shape[0])))


    # Slightly blur the image
    img = cv2.GaussianBlur(img,(3,3),2)

    # Save to stack
    all_img[:,:,i] = img


# Generate average image
avg_img = np.mean(all_img,axis=2).astype(np.uint8)

cv2.imshow("Preview",avg_img)
cv2.waitKey(0)
cv2.imwrite(fout,avg_img)