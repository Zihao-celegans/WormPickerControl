'''
This script will use the mask rcnn network that was trained on the WormPicker's high mag images to segment worms in a worm motel.
The script extracts each well from the provided image and sends them each to the mask rcnn network for inference, rather than sending
the entire image at once. This is to avoid confusing the network due to the large dark lines present around the wells in the
worm motel images. 
The script uses hard coded parameters for the size and position of each well, as it is assumed the well positions will be more or less
static from image to image.
'''

import sys

# Add the mask rcnn folders to the path so we can import from thir files
curr_path = sys.path[0].split("\\") # Get the current path
utils_path = curr_path.copy()
curr_path.append('mask_rcnn') # append mask_rcnn folder to the path
utils_path.append('mask_rcnn_utils') #append the utils folder to the path
curr_path = '\\'.join(curr_path) # create the final path string to the mask rcnn folder
utils_path = '\\'.join(utils_path) # create the final path string to the utils folder
sys.path.append(curr_path) # Add the mask_rcnn folder to the system's path
sys.path.append(utils_path) # Add the utils folder to the system's path

# Imports
import torch
import cv2
import numpy as np
from mask_rcnn.Tutorial_Mask_R_CNN_PyTorchOfficial import get_resnet50_model_instance_segmentation, predict_with_model, draw_masks_on_imgs


if __name__ == "__main__":
    # Auto-select GPU or CPU
    use_cuda = torch.cuda.is_available()  # Automatically decide whether to use CUDA

    # Load model and image
    fpth_worm = r"C:\WormPicker\NeuralNets\model_2021-04-16_22-06-40_100epochs_15Done.pth"
    model_worm = get_resnet50_model_instance_segmentation(1, model_load_dir=fpth_worm)
    image_dir = r'C:\Users\Yuchi\Desktop\WM2_inverted.png'
    worm_img = cv2.imread(image_dir)

    # Hard coded well parameters
    well_width = 132
    well_height = 122
    resize_multiplier = 4 # We enlarge the extracted well images so that the size of worms is similar to the size of a worm in the Wormpicker high mag camera (which is what the network was trained on)

    # Create the ROIs for each well. The position of each well is predictable inside the image, so the values are hard coded.
    wells = []
    row_vals = [26, 185, 344]
    col_vals =[24, 196, 374]
     
    # Extract the individual wells from the image
    for row_y in row_vals:
        for col_x in col_vals:
            wells.append(worm_img[row_y:row_y + well_height, col_x:col_x + well_width, :])

    # Enlarge each well image
    wells_large = []
    for well in wells:
        wells_large.append(cv2.resize(well, (well_width * resize_multiplier, well_height * resize_multiplier)))

    # Send each image to the network
    predictions = []
    for well in wells_large:
        predictions.append(predict_with_model([well], model_worm))

    # Overlay the segmentation results
    masked_images=[]
    for i in range(len(predictions)):
        masked_images.append(draw_masks_on_imgs([wells_large[i]], predictions[i], score_thresh=0.7, label_type="worm", display_labels = False, include_orig_img = False))

    # Reduce size back down to original size
    for i in range(len(wells_large)):
        wells[i] = cv2.resize(masked_images[i][0], (well_width, well_height))

    # Stitch the image back together
    worm_img_overlay = cv2.imread(image_dir)
    well_num = 0
    for row_y in row_vals:
        for col_x in col_vals:
            worm_img_overlay[row_y:row_y + well_height, col_x:col_x + well_width, :] = wells[well_num]
            well_num += 1
    
    # Display results
    worm_img_overlay = cv2.resize(worm_img_overlay, (int(worm_img_overlay.shape[1] * 1.75), int(worm_img_overlay.shape[0] * 1.75)))
    worm_img = cv2.resize(worm_img, (worm_img_overlay.shape[1], worm_img_overlay.shape[0]))
    cv2.imshow("stitched", worm_img_overlay)
    cv2.imshow("original", worm_img)
    cv2.waitKey(0)


