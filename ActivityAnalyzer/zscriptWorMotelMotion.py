



from verifyDirlist import verifyDirlist
import csvio
import cv2
import numpy as np
import os
from tkinter import filedialog
import tkinter as tk
from scipy.signal import medfilt2d, correlate2d


fimg0 = r'F:\ADF_AUX\AUX_E1_T1\2020-01-10 (21-56-40)\2020-01-10 (21-56-40).png'
fimg1 = r'F:\ADF_AUX\AUX_E1_T1\2020-01-10 (21-56-40)\2020-01-10 (22-01-40).png'

img0 = cv2.imread(fimg0,cv2.IMREAD_GRAYSCALE)
img1 = cv2.imread(fimg1,cv2.IMREAD_GRAYSCALE)

# Resize to scale up
scalefactor = 9.0
img0 = cv2.resize(img0,(0,0),fx=scalefactor,fy=scalefactor)
img1 = cv2.resize(img1,(0,0),fx=scalefactor,fy=scalefactor)


# Resize scale up coords
x0 = int((781)*scalefactor)
x1 = int((781+132)*scalefactor)
y0 = int((1682)*scalefactor)
y1 = int((1682+132)*scalefactor)


dx = -1
dy = 0

crop0 = img0[y0:y1 , x0:x1 ]
crop1 = img1[y0+dy : y1+dy , x0+dx : x1+dx]

i = 0
while True:

    i += 1
    if i == 2:
        i=0
        



    cv2.namedWindow('Test',cv2.WINDOW_NORMAL)
    if i == 0:
        cv2.imshow('Test',crop0)
    else:
        cv2.imshow('Test',crop1)
    cv2.waitKey(100)

    if i == 0:
        cv2.waitKey(500)

772,182