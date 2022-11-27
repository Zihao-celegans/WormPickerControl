

import cv2
import numpy as np
import matplotlib.pyplot as plt
from verifyDirlist import *
from roipoly_adf import *
import tkinter as tk
import tkinter.filedialog

# Script - load all images and work on them
pimg = r'C:\Dropbox\FramesData\2019-11-21_test01_raw'

# Select plate folder
root = tk.Tk()
root.withdraw()
pimg = tk.filedialog.askdirectory(parent=root, initialdir=pimg, title='Please select the PLATE folder')

extension = ".png"
dimg, dshort = verifyDirlist(pimg,False,extension,'contmask')
if len(dimg) == 0:
    extension = ".jpg"
    dimg, dshort = verifyDirlist(pimg,False,extension,'contmask')

# Check if already done, if not do it
for (imgname, short_name) in zip(dimg,dshort):

    # Check if contmask exists
    ftest = short_name.replace(extension,'_contmask.png')
    test = len(verifyDirlist(pimg,False,ftest)[0]) > 0

    if test:
        print('Skipping - already annotated image: ', imgname)
        continue

    # Otherwise do the annotation
    annotateImage(imgname,extension)





