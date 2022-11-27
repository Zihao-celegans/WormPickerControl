


from verifyDirlist import verifyDirlist
import csvio
import cv2
import numpy as np
import numpy.matlib
import os
from tkinter import filedialog
import tkinter as tk
from scipy.signal import medfilt2d, correlate2d
import matplotlib.pyplot as plt

# Set file and params
fcsv = r'C:\Dropbox\Anthony Alice (shared)\WW Swim Assay\Session_SwimTest_2020-04-19 (14-45-58).csv'
fps = 10


#Load xbar ybar and angle from file
xbar = csvio.readArrayFromCsv(fcsv,"<xbar>").astype(np.float);
ybar = csvio.readArrayFromCsv(fcsv,"<ybar>").astype(np.float);
angle = csvio.readArrayFromCsv(fcsv,"<angle>").astype(np.float);
ratio = csvio.readArrayFromCsv(fcsv,"<Aspect ratio>").astype(np.float);
t = csvio.readArrayFromCsv(fcsv,"<Stopwatch>").astype(np.float)

# Generate filtered / smoothened xbar and ybar for comparison, with 3 second median filter
ksize = int(fps * 3)
if (ksize % 2 == 0):
    ksize += 1

xbar_filt = medfilt2d(xbar,(ksize,1))
ybar_filt = medfilt2d(ybar,(ksize,1))

# Compute distance between live point and filtered point
bardist = np.sqrt((xbar- xbar_filt)**2 + (ybar - ybar_filt)**2)

# Xbar for 3

istart = 13 # WELL counting, starting from 1. 
f0 = 0
f1 = 120
plt.figure(1,figsize =(10,8))


for i in range(6):

    # Xbar
    plt.subplot(6,2,2*i+1)
    plt.plot(t[f0:f1],xbar[f0:f1,istart + i + 1])

    # bardist
    plt.subplot(6,2,2*i+2)
    plt.plot(t[f0:f1],ratio[f0:f1,istart + i + 1])



plt.show()

b=1