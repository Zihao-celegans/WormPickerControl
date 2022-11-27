import cv2
import numpy as np
from verifyDirlist import *
import tkinter as tk
import tkinter.filedialog
import random
import os



pbase = r'C:\Users\pdb69\Desktop\WormTrainingData\GenderRaw'

root = tk.Tk()
root.withdraw()
#pimgs = tk.filedialog.askdirectory(parent=root, initialdir=pbase, title='Please select the images folder')
#pmasks = tk.filedialog.askdirectory(parent=root, initialdir=pbase, title='Please select the masks folder')

#pimgs = r'C:\Users\pdb69\Desktop\WormTrainingData\DatasetImages'
#pmasks = r'C:\Users\pdb69\Desktop\WormTrainingData\DatasetMasks'
pimgs=r'C:\Users\pdb69\Dropbox\FangYenLab\FramesData\Mask_RCNN_Datasets\GenderDatasets\Combined_ADF-YF-ZL_WholeHeadMasks\DatasetImages'
pmasks=r'C:\Users\pdb69\Dropbox\FangYenLab\FramesData\Mask_RCNN_Datasets\GenderDatasets\Combined_ADF-YF-ZL_WholeHeadMasks\DatasetMasks'

image_paths, img_names = verifyDirlist(pimgs,False,".png")
mask_paths, mask_names = verifyDirlist(pmasks,False,".png")
print(img_names[0][:-4])
print(mask_names[0])
print(mask_names[0][:-9])
print(f"Images to check: {len(image_paths)}")
print(f"Masks to check: {len(mask_paths)}")
empty_count = 0

bad_img = 0
bad_mask= 0
for iidx, img_name in enumerate(img_names):
    curr_name = img_name[:-4]
    for idx, mask_name in enumerate(mask_names):
        curr_mask = mask_name[:-9]
        if curr_mask == curr_name:
            break
        if idx == len(mask_names)-1:
            print(f"No corresponding mask found for image: {curr_name}")
            print(f"No corresponding mask found for image: {img_name}")
            print()
            bad_img += 1
    if(iidx % 100 == 0):
        print(f'{iidx} images checked')


for iidx, mask_name in enumerate(mask_names):
    curr_mask = mask_name[:-9]
    for idx, img_name in enumerate(img_names):
        curr_name = img_name[:-4]
        if curr_mask == curr_name:
            break
        if idx == len(img_names)-1:
            print(f"No corresponding image found for mask: {curr_mask}")
            print(f"No corresponding image found for mask: {mask_name}")
            print()
            bad_mask += 1
    if(iidx % 100 == 0):
        print(f'{iidx} images checked')


print(f'Found {bad_img} bad images. Found {bad_mask} bad masks.')
    #print()
