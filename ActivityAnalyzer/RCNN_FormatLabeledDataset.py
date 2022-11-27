# The images and masks generated from the labeling script (CNN_Draw_
# LabelsInImages.py) are saved in one folder. This script takes the images and
# the masks from that folder, and will copy them into separate folders
# (DatasetImages, DatasetMasks, EmptyImages, and EmptyMasks) as is necessary
# for the Mask RCNN training.

# Note: there is no need to edit this file before running

import cv2
import numpy as np
from verifyDirlist import *
import tkinter as tk
import tkinter.filedialog
import os
import shutil

# Obtaining the initial and final directories to work on
root = tk.Tk()
root.withdraw()
pinitial = tk.filedialog.askdirectory(parent=root,
                                      title='Please select the folder containing the images and masks to move.')
pfinal = tk.filedialog.askdirectory(parent=root,
                                    title='Please select the base dataset folder to move images to')

pfinal_images = os.path.join(pfinal, 'DatasetImages')
pfinal_masks = os.path.join(pfinal, 'DatasetMasks')

image_paths, img_names = verifyDirlist(pinitial, False, ".png")

# Separate images and masks
# Note: all images and masks without their corresponding counterpart are skipped
print('---------------------------------------------------')
print(f"Images to move: {len(image_paths)}")
images_moved = 0
masks_moved = 0
for idx, image_path in enumerate(image_paths):
    if '_mask' not in image_path:
        # Test if a mask exists for this image
        mask_path = image_path.replace('.png', '_mask.png')
        if not os.path.exists(mask_path):
            print('Skipped (no mask): ', img_names[idx])
            continue
        # Move the image
        new_img_path = os.path.join(pfinal_images, img_names[idx])
        os.makedirs(os.path.dirname(new_img_path), exist_ok=True)
        shutil.copy(image_path, new_img_path)
        images_moved += 1
        # Move the corresponding mask
        new_mask_path = os.path.join(pfinal_masks,
                                     img_names[idx].replace('.png',
                                                            '_mask.png'))
        os.makedirs(os.path.dirname(new_mask_path), exist_ok=True)
        shutil.copy(mask_path, new_mask_path)
        masks_moved += 1
    # Report progress
    if idx > 0 and idx % 10 == 0:
        print(f'Progress report: {idx} images moved. Running...')

print('---------------------------------------------------')
print('All image-mask pairs moved.')
print('Now checking for empty ones...')

# Separate dataset and empty
image_paths, img_names = verifyDirlist(pfinal_images, False, ".png")
mask_paths, mask_names = verifyDirlist(pfinal_masks, False, ".png")
pfinal_images_empty = os.path.join(pfinal, 'EmptyImages')
pfinal_masks_empty = os.path.join(pfinal, 'EmptyMasks')

empty_count = 0

for idx, image_path in enumerate(image_paths):
    mask_path = mask_paths[idx]
    mask = cv2.imread(mask_path, cv2.IMREAD_COLOR)

    m = np.amax(mask)
    if m == 0:
        empty_count += 1
        new_img_filepath = os.path.join(pfinal_images_empty, img_names[idx])
        new_msk_filepath = os.path.join(pfinal_masks_empty, mask_names[idx])
        os.makedirs(os.path.dirname(new_img_filepath), exist_ok=True)
        os.replace(image_path, new_img_filepath)
        os.makedirs(os.path.dirname(new_msk_filepath), exist_ok=True)
        os.replace(mask_path, new_msk_filepath)
    # Report progress
    if idx > 0 and idx % 100 == 0:
        print(f'Progress report: {idx} images checked. Running...')

# Report final stats
print('---------------------------------------------------')
print('Finished.')
print(f'{images_moved - empty_count} images moved to "DatasetImages."')
print(f'{masks_moved - empty_count} masks moved to "DatasetMasks."')
print(f'{empty_count} images moved to "EmptyImages."')
print(f'{empty_count} masks moved to "EmptyMasks."')
