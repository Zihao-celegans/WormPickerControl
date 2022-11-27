import cv2
import numpy as np
from verifyDirlist import *
import random
import os


pbase = r'C:\Users\pdb69\Desktop\WormTrainingData'
#pmasks = r'C:\Users\pdb69\Desktop\WormTrainingData\DatasetMasks'
pmasks = r'C:\Users\pdb69\Desktop\WormTrainingData\DatasetMasks'
poutput = r'C:\Users\pdb69\Desktop\WormTrainingData\ConvertedGrayToRGBMasks'

mask_paths, mask_names = verifyDirlist(pmasks,False,".png")

total_masks = len(mask_names)
herm_converted = 0
male_converted = 0

print(f"Masks to check: {len(mask_paths)}")

#For class parameter reference, see mask_rcnn_utils/utils.py function get_labeling_params

for index, mask_path in enumerate(mask_paths):

    if index % 100 == 0:
        print(f'{index}/{total_masks} checked.')

    mask = cv2.imread(mask_path, cv2.IMREAD_GRAYSCALE)
    mask_channel_r = np.zeros(shape=mask.shape)
    mask_channel_g = np.zeros(shape=mask.shape)
    mask_channel_b = np.zeros(shape=mask.shape)

    if "adult_herm" in mask_names[index]:
        #print("Converting adult herm")
        herm_converted += 1
        mask_channel_g = mask
        mask_rgb = np.dstack((mask_channel_r,mask_channel_g,mask_channel_b))
        #print(f'unique rgb: {np.unique(mask_rgb)}')
        #print(f'unique mask 0: {np.unique(mask_rgb[:,:,0])}')
        #print(f'unique mask 1: {np.unique(mask_rgb[:,:,1])}')
        #print(f'unique mask 2: {np.unique(mask_rgb[:,:,2])}')
        #print(f"rgb shape: {mask_rgb.shape}")
    elif "adult_male" in mask_names[index]:
        #print("Converting adult male")
        male_converted += 1
        mask_channel_g = mask
        for r in range(len(mask_channel_g)):
            for c in range(len(mask_channel_g[r])):
                if mask_channel_g[r][c] > 0:
                    mask_channel_g[r][c] += 51
        mask_rgb = np.dstack((mask_channel_r,mask_channel_g,mask_channel_b))

    # Save image to new folder
    final_output = os.path.join(poutput, mask_names[index])
    #print(final_output)
    cv2.imwrite(final_output, mask_rgb)

print(f'{herm_converted + male_converted}/{total_masks} converted ({herm_converted} herm + {male_converted} male)')
