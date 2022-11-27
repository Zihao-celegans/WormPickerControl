'''
This script loads a set of images and their corresponding masks that are to be used in training the Mask RCNN network and augments them with
various image manipulation techniques. Any augmentations on an image are similarly applied to that images mask to ensure the image and masks
continue to accurately represent one another.

The images and their masks are to be in two seperate folders, and the augmented images and masks will be saved in the same folders, but with
modified filenames representing the applied augmentations.

Each image must have a corresponding mask.
'''

import cv2
import numpy as np
from verifyDirlist import *
import tkinter as tk
import tkinter.filedialog
import random


def flipImage(image, mask, augmentations = [], v_flip = False):
    if v_flip:
        flip_direction = 0
        augmentations.append("vflip")
    else:
        flip_direction = 1
        augmentations.append("hflip")
    
    image_flip = cv2.flip(image, flip_direction)
    mask_flip = cv2.flip(mask, flip_direction)

    return image_flip, mask_flip, augmentations

def blurImage(image, mask, augmentations = []):
    blur_kernel = random.randrange(7, 20, 2)
    b_img = cv2.GaussianBlur(image, (blur_kernel,blur_kernel), 0)
    augmentations.append(f'gblur{blur_kernel}')
    return b_img, mask, augmentations

def addRandomNoise(image, mask, augmentations = []):
    # Generate noise
    noise = []
    amount_of_noise = random.randint(3,6)
    for i in range(image.size):
        if random.randint(1,100) <= 3:
            noise.append(random.randint(0,255))
        else:
            noise.append(0)

    #gauss = np.random.normal(0,1,image.size)
    #gauss = gauss.reshape(image.shape[0],image.shape[1],image.shape[2]).astype('uint8')
    noise = np.array(noise)
    noise = noise.reshape(image.shape[0],image.shape[1],image.shape[2]).astype('uint8')
    #print(f'noise data: {noise}')
    # Add the Gaussian noise to the image
    img_noise = cv2.add(image,noise)
    #print(image.size)
    ## Display the image
    #cv2.imshow(f'noise level {amount_of_noise}',img_noise)
    #cv2.waitKey(0)

    augmentations.append(f'rnoise{amount_of_noise}')
    return img_noise, mask, augmentations

def changeBrightness(image, mask, augmentations = []):
    #print(f'image shape: {image.shape}')
    #print(f'image size: {image.size}')
    #print(f'img/3: {int(image.size/3)}')
    change = random.randint(30,70)
    lighting = np.full(image.shape, change).astype('uint8')
    if random.randint(0,1) == 1:
        # Brighten
        img_lighting = cv2.add(image, lighting)
        augmentations.append(f'brighten{change}')
    else:
        # Darken
        img_lighting = cv2.subtract(image, lighting)
        augmentations.append(f'darken{change}')
    
    ## Display the image
    #cv2.imshow(f'light amt {change}',img_lighting)
    #cv2.waitKey(0)

    return img_lighting, mask, augmentations


        



# Add Gaussian noise to the image. Adapted from stackoverflow:
# https://stackoverflow.com/questions/22937589/how-to-add-noise-gaussian-salt-and-pepper-etc-to-image-in-python-with-opencv
# Note that we don't need to change the mask at all because the position of the worm is not changing with this augmentation
def addNoise(image):
    # TODO: Make it work properly. THIS IS CURRENTLY NOT WORKING.
    print(f"image: {image}")
    row,col,ch= image.shape
    mean = 0
    sigma = 5
    gauss = np.random.normal(mean,sigma,(row,col,ch))
    gauss = gauss.reshape(row,col,ch) 
    print(f"gauss: {gauss}")
    gauss = (gauss).astype(int)
    print(f"gauss int: {gauss}")
    noisy = image + gauss
    return noisy

def saveAugmentations(path_image, image, path_mask, mask, augmentations = []):
    if ".png" in path_image:
        path_image = path_image[:-4]
        path_mask = path_mask[:-4]
    else:
        print("Only supports png filetypes.")
        return
    
    for aug in augmentations:
        path_image = path_image + "_" + aug
        path_mask = path_mask + "_" + aug

    path_image = path_image + ".png"
    path_mask = path_mask + ".png"

    #print(f"Saving image: {path_image}")

    cv2.imwrite(path_image, image)
    cv2.imwrite(path_mask, mask)


#if __name__ == "main":

pbase = r'C:\Users\pdb69\Desktop\WormTrainingData\GenderRaw\DataToLabel\AugmentationsTest'

root = tk.Tk()
root.withdraw()
pimgs = tk.filedialog.askdirectory(parent=root, initialdir=pbase, title='Please select the images folder')
pmasks = tk.filedialog.askdirectory(parent=root, initialdir=pbase, title='Please select the masks folder')

#pimgs = r'C:\Users\pdb69\Desktop\WormTrainingDataTEST\Images'
#pmasks = r'C:\Users\pdb69\Desktop\WormTrainingDataTEST\Masks'

image_paths, _ = verifyDirlist(pimgs,False,".png")
mask_paths, _ = verifyDirlist(pmasks,False,".png")

print(f'{len(image_paths)} images found.')

for idx, image_path in enumerate(image_paths):
    
    if (idx != 0 and idx % 50 == 0):
        print(f'{idx}/{len(image_paths)} images augmented.')

    #print(f'Augmenting {image_path}')
    mask_path = mask_paths[idx]
    image = cv2.imread(image_path,cv2.IMREAD_COLOR)
    mask = cv2.imread(mask_path, cv2.IMREAD_COLOR)

    augmented_images = [(image, mask, [])]

    #cv2.imshow("Original", image)
    #cv2.waitKey(1)

    #Flip the image horizontally
    h_flip, mask_h_flip, h_augmentations = flipImage(image, mask, augmentations = [], v_flip=False)
    augmented_images.append((h_flip, mask_h_flip, h_augmentations.copy()))

    # Flip the image vertically
    v_flip, mask_v_flip, v_augmentations = flipImage(image, mask, augmentations = [], v_flip=True)
    augmented_images.append((v_flip, mask_v_flip, v_augmentations.copy()))

    # Flip the image horizontally and vertically
    vh_flip, mask_vh_flip, vh_augmentations = flipImage(h_flip, mask_h_flip, augmentations = h_augmentations, v_flip=True)
    augmented_images.append((vh_flip, mask_vh_flip, vh_augmentations.copy()))

    # Blur the image
    b_img, b_mask, b_augmentations = blurImage(image, mask, augmentations = [])
    augmented_images.append((b_img, b_mask, b_augmentations.copy()))

    # Add random noise to the image
    noise_img, noise_mask, noise_augmentations = addRandomNoise(image, mask, augmentations = [])
    augmented_images.append((noise_img, noise_mask, noise_augmentations.copy()))

    # Change brightness of image
    light_img, light_mask, light_augmentations = changeBrightness(image, mask, augmentations = [])
    augmented_images.append((light_img, light_mask, light_augmentations.copy()))

    ### A small subset of the images (5%) will also be blurred.
    #for (img, msk, augs) in augmented_images.copy():
    #    blur = 1 == random.randint(1,20)
    #    if blur:
    #        print("Blurring image!")
    #        b_img = cv2.GaussianBlur(img, (15,15), 0)
    #        b_augs = augs.copy()
    #        b_augs.append("gblur")
    #        augmented_images.append((b_img.copy(), msk.copy(), b_augs))

    for (img, msk, augs) in augmented_images[1:]:
        saveAugmentations(image_path, img, mask_path, msk, augs)

    if idx == len(image_paths) - 1:
        print(f'{idx + 1}/{len(image_paths)} images augmented.')
    

    #image, mask, augmentations = flipImage(image, mask, augmentations, True)

    #image = cv2.GaussianBlur(image, (15,15), 0)
    #augmentations.append("gblur")

    #cv2.imshow("Augmented", h_flip)
    #cv2.waitKey(0)



    #saveAugmentations(image_path, image, mask_path, mask, augmentations)
