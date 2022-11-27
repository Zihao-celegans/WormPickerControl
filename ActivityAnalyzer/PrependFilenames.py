'''
Prepends all the files in a folder with a given prefix
'''



import tkinter as tk
import tkinter.filedialog
from verifyDirlist import *
import os



# Script - load all images and work on them
pimg = r'C:\Dropbox\FramesData\Mask_RCNN_Normalized_Frames'

# Select folder containing the images you want to change their filename
root = tk.Tk()
root.withdraw()
pimg = tk.filedialog.askdirectory(parent=root, initialdir=pimg, title='Please select the images folder')

dimg, _ = verifyDirlist(pimg, False)

#prepend = 'adult_males_test02_'
prepend = input("What would you like to prepend the filenames with?")

for idx, filepath in enumerate(dimg):
    split_path = filepath.split('\\')
    prepended = prepend + split_path[1]
    os.rename(filepath, split_path[0] + "\\" + prepended)



