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
pmasks=r'C:\Users\pdb69\Desktop\WormTrainingData\GenderRaw\DataToLabel\DatasetMasks'

mask_paths, mask_names = verifyDirlist(pmasks,False,".png")

print(f"Masks to check: {len(mask_paths)}")
herm_count = 0
male_count = 0
for idx, mask_path in enumerate(mask_paths):
    #print(f'Augmenting {image_path}')
    mask = cv2.imread(mask_path, cv2.IMREAD_COLOR)

    channel = 1
    this_channel = mask[:,:,channel]
    this_obj_ids = np.unique(this_channel) # Get the instance ids for this channel
    this_obj_ids = this_obj_ids[1:] # First id is the background, so remove it
    male_count += len(this_obj_ids)

    channel = 0
    this_channel = mask[:,:,channel]
    this_obj_ids = np.unique(this_channel) # Get the instance ids for this channel
    this_obj_ids = this_obj_ids[1:] # First id is the background, so remove it
    herm_count += len(this_obj_ids)
 
    if(idx % 100 == 0):
        print(f'{idx} images checked')
print(f'Found {herm_count} herm worms. Found {male_count} male worms.')
