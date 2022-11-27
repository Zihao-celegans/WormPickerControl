"""
    RCNN_Segment_Image.py
    Python-based server engine for segmenting worms using Mask-RCNN.
    Supply full file names of network and image as command line arguments
    Output masks are saved to disk
    Load an image and segment it using PyTorch Mask R-CNN
    Resource for directly sending /receiving images via socket connection
    https://github.com/TheMarvelousWhale/Python-Socket-For-Image-Transfer/blob/master/OptimizedServer.py
    Modified on 6/19/2021 to also run the gender finder net and return coordinates of heads,
    herm tails, and male tails
"""

# Hardcoded params
print_elapsed_time = False
MAX_LENGTH = 4500 * 3500 * 4  # up to 4 digits per
HOST = '127.0.0.1'

# Softcoded params
import sys

curr_path = sys.path[0].split("\\") # Get the current path
utils_path = curr_path.copy()
curr_path.append('mask_rcnn') # append mask_rcnn folder to the path
utils_path.append('mask_rcnn_utils') #append the utils folder to the path
curr_path = '\\'.join(curr_path) # create the final path string to the mask rcnn folder
utils_path = '\\'.join(utils_path) # create the final path string to the utils folder
sys.path.append(curr_path) # Add the mask_rcnn folder to the system's path
sys.path.append(utils_path) # Add the utils folder to the system's path

if len(sys.argv) >= 4:
    fpth_worm = sys.argv[1]
    PORT = int(sys.argv[2])
    conf_thresh_pixel = float(sys.argv[3])  # not used anymore in this version

else:
    # # Peter/John/Yuying, please don't change these, just put your nets in the NeuralNets folder
    fpth_worm = r"C:\WormPicker\NeuralNets\model_2021-04-16_22-06-40_100epochs_15Done.pth"
    fpth_gender = r'C:\WormPicker\NeuralNets\model_2021-06-20_15-10-50_resnet50_4classes_9epochs_7Done_HTGender.pth'
    # fpth_gender = r'C:\WormPicker\NeuralNets\model_2021-06-30_17-12-01_resnet50_4classes_20epochs_12Done.pth'
    PORT = 10000

# Global variables
buf = ""

# Imports
import time
import socket
from threading import Thread
import torch
import torchvision
from torchvision.models.detection.faster_rcnn import FastRCNNPredictor
from torchvision.models.detection.mask_rcnn import MaskRCNNPredictor
from RCNN_Helper import loadRCNN
import cv2
import numpy as np
import numpy.matlib
import os
from mask_rcnn.Tutorial_Mask_R_CNN_PyTorchOfficial import draw_masks_on_imgs


def printElapsedTime(start, label, print_elapsed_time):
    end = time.time()
    #print("-- server processing - ", label, " took ", round(end - start, 3),"seconds")
    return end


def decodeImageFromSocket(buf):
    # Split comma separated values
    buf = str(buf)
    if buf[0] == 'b':
        # remove b and ' at beginning after conversion  to string
        buf = buf[2:]
    if buf[-1] == "'":  # Remove ' from end
        buf = buf[:-1]

    vals = buf.split(",")

    # Starting index in the list of encoded string integers
    pixel_start_idx = int(vals[0])
    # Preallocate image
    R = int(vals[1])
    C = int(vals[2])
    img = np.zeros((R, C), np.uint8)

    # Enforce that the received image size matches the header
    N_pix_actual = len(vals) - pixel_start_idx
    N_pix_requested = R * C

    # Enforce that values were split
    if len(vals) < 10:
        print("DECODE ERROR: Only received %d values" % (N_pix_actual))
        return None

    if N_pix_actual < N_pix_requested:
        print(
            "DECODE ERROR: Expected %d x %d = %d pixels - received %d pixels" % (
                R, C, N_pix_requested, N_pix_actual))
        return None

    # Populate pixel values
    for r in range(R):
        for c in range(C):
            img[r, c] = int(vals[pixel_start_idx])
            pixel_start_idx += 1

    # Convert the grayscale image to "color" which is required by CNN
    img_color = cv2.cvtColor(img, cv2.COLOR_GRAY2BGR)

    return img_color


# Return the gender info as a dictionary of lists of dictionaries with the
# structure {worm_number: [mask1, mask2]}, mask1 = {cX, cY, type, conf_score}
# where worm_number is an int, cX and cY are int coordinates of the centroid,
# type is an int (1 = head, 2 = male tail, 3 = herm tail), and conf_score is
# the confidence score as a float.
def decodeGender(buf):
    # Split comma separated values
    buf = str(buf)
    if buf[0] == 'b':
        # remove b and ' at beginning after conversion  to string
        buf = buf[2:]
    if buf[-1] == "'":  # Remove ' from end
        buf = buf[:-1]

    vals = buf.split(",")

    # Starting index of the image in the list of encoded string integers
    pixel_start_idx = int(vals[0])
    # Starting index of the gender info
    idx = 3

    # Decode gender info
    gender_info = {}
    while idx < pixel_start_idx:
        worm_number = int(vals[idx])
        mask_info = {'cX': int(vals[idx + 1]),
                     'cY': int(vals[idx + 2]),
                     'type': int(vals[idx + 3]),
                     'conf_score': float(vals[idx + 4])}
        if worm_number not in gender_info:
            gender_info[worm_number] = []
        gender_info[worm_number].append(mask_info)
        idx += 5

    return gender_info


# Return a dictionary containing threshold values to be used in the handle
# function of the server. The last value in the encoded string would be 'T' if
# the thresholds are present. It will be assumed that there are six thresholds
# encoded preceding 'T': worm_overlap, worm_conf_image, worm_conf_pixel,
# gender_overlap, gender_conf_image, gender_conf_pixel. If these aren't
# present, return the default thresholds.
#
# Default thresholds:
# worm_overlap = 0.7
# worm_conf_image = 0.4
# worm_conf_pixel = 0.7
# gender_overlap = 0.05
# gender_conf_image = 0.05
# gender_conf_pixel = 0.7
def decodeThresholds(buf):
    # Split comma separated values
    buf = str(buf)
    if buf[0] == 'b':
        # remove b and ' at beginning after conversion  to string
        buf = buf[2:]
    if buf[-1] == "'":  # Remove ' from end
        buf = buf[:-1]

    vals = buf.split(",")
    if vals[-1] == 'T':
        return {'worm_overlap': float(vals[-7]),
                'worm_conf_image': float(vals[-6]),
                'worm_conf_pixel': float(vals[-5]),
                'gender_overlap': float(vals[-4]),
                'gender_conf_image': float(vals[-3]),
                'gender_conf_pixel': float(vals[-2])
                }
    else:
        return {'worm_overlap': 0.75,
                'worm_conf_image': 0.75, # Was 0.4 but tuned it up because we were getting bad results when using python for worm drop verification
                'worm_conf_pixel': 0.5, # Was 0.425 but changed to 0.5 in order to match the pixel conf of the worm staging data we collected
                                        # Shouldn't change this value because staging and morphology phenotyping depends on it.
                'gender_overlap': 0.05,
                'gender_conf_image': 0.4, # Was 0.05 (calibrated for best overall performance) but changed it for our experiment because we were misclassifying herms as males
                'gender_conf_pixel': 0.7
                }


def encodeImageForSocket(img, worm_gender_results=None):
    gender_results = ""
    if not worm_gender_results:
        first_pixel_idx = 3
    else:
        if len(worm_gender_results) == 0:
            print("Empty worm gender results")
        head_tail_count = 0
        # Encode the gender information as follows:
        #   For each head and tail we have a set of 5 values consisting of
        #   Worm number, cX, cY, head/tail type, conf score.
        for i, worm in worm_gender_results.items():
            if worm['head-index'] != -1:
                head_tail_count += 1
                gender_items = [i, worm['head-centroid'][0],
                                worm['head-centroid'][1], 1,
                                round(worm['head-score'].item(), 4)]
                gender_results += ",".join(map(str, gender_items)) + ','
            if worm['tail-index'] != -1:
                head_tail_count += 1
                gender_items = [i, worm['tail-centroid'][0],
                                worm['tail-centroid'][1],
                                worm['tail-type'].item(),
                                round(worm['tail-score'].item(), 4)]
                gender_results += ",".join(map(str, gender_items)) + ','

        first_pixel_idx = 3 + 5 * head_tail_count

    # Encode rows and columns first
    encoded_image = f'{first_pixel_idx},'
    encoded_image += str(img.shape[0]) + ","
    encoded_image += str(img.shape[1]) + ","
    encoded_image += gender_results

    # print(f'pre pixel encoded string: {encoded_image}')

    # Populate the rest of the pixel values into the str
    for r in range(img.shape[0]):
        for c in range(img.shape[1]):
            encoded_image += str(int(img[r, c])) + ","

    # Add newline print_elapsed_time to trigger end for boost asio
    encoded_image += "\n"

    return encoded_image


# Encodes the thresholds in the input dictionary as a string separated by
# commas, and adds 'T' to the end. All values missing from the input are
# encoded with their default values.
def encodeThresholds(thresholds):
    worm_overlap = thresholds[
        'worm_overlap'] if 'worm_overlap' in thresholds else 0.7
    worm_conf_image = thresholds[
        'worm_conf_image'] if 'worm_conf_image' in thresholds else 0.4
    worm_conf_pixel = thresholds[
        'worm_conf_pixel'] if 'worm_conf_pixel' in thresholds else 0.7
    gender_overlap = thresholds[
        'gender_overlap'] if 'gender_overlap' in thresholds else 0.05
    gender_conf_image = thresholds[
        'gender_conf_image'] if 'gender_conf_image' in thresholds else 0.05
    gender_conf_pixel = thresholds[
        'gender_conf_pixel'] if 'gender_conf_pixel' in thresholds else 0.7
    return f'{worm_overlap},{worm_conf_image},{worm_conf_pixel},' \
           f'{gender_overlap},{gender_conf_image},{gender_conf_pixel},T'


# Main server function
def handle(clientsocket, display_images=False):
    while 1:

        # Get data
        start = time.time()
        buf = clientsocket.recv(MAX_LENGTH)

        if buf == b'':
            return  # client terminated connection
        if "terminate" in str(buf.lower()):
            return  # client asked server to terminate
        print("Buffer received: ", len(buf), "\tmax: ", MAX_LENGTH)

        #print("-------------- Connection received: Decoding image for inference --------------")

        # Decode image
        image = decodeImageFromSocket(buf)

        # Failed to decode: Abort prediction and send back empty masks
        if image is None:
            clientsocket.send("DECODE_ERROR".encode())
            return

        # cv2.imshow("DEBUG IMAGE",image)
        # cv2.waitKey(0)

        # Load image
        start = printElapsedTime(start, "Reading and decoding image",
                                 print_elapsed_time)

        pil2tensor = torchvision.transforms.ToTensor()
        image_tensor = pil2tensor(image)
        if use_cuda:
            image_tensor = image_tensor.cuda()

        start = printElapsedTime(start, "Image Conversion", print_elapsed_time)

        # Decode thresholds
        thresholds = decodeThresholds(buf)

        #####print(f'Thresholds: {thresholds}')

        ## PERFORM WORM IDENTIFICATION
        # Perform inference
        model_worm.eval()  # For inference
        with torch.no_grad():
            predictions_worms = model_worm(
                [image_tensor])  # Returns prediction

        start = printElapsedTime(start, "Worm Inference", print_elapsed_time)

        # Extract masks from predictions_worms
        predictions_worms = predictions_worms[
            0]  # Always 1 element if 1 element list of images is supplied
        #####print(f'prediction worms: {predictions_worms}')
        #print(f'worm predictions: {predictions_worms}')
        masks_worms = predictions_worms['masks'].permute(
            [2, 3, 1, 0]).cpu().numpy()
        N_masks_worms = masks_worms.shape[3]

        # Consolidate masks that are actually for the same worm
        eliminated_idx = []
        for n in range(N_masks_worms):
            if n in eliminated_idx:
                continue

            # Eliminate masks with a low overall confidence
            if predictions_worms['scores'][n] < thresholds['worm_conf_image']:
                eliminated_idx.append(n)
                masks_worms[:, :, :, n] = np.zeros(masks_worms[:, :, :, n].shape, np.uint8)
                continue

            this_mask = np.mean(masks_worms[:, :, :, n],
                                axis=2) > thresholds['worm_conf_pixel']

            #print(f'mask size: {this_mask.size}')
            #print(f'mask shape: {this_mask.shape}')
            #print(f'mask type: {this_mask.dtype}')

            #display_mask = (this_mask.astype(np.uint8)) * 255
            #cv2.imshow(f"Worm mask {n}", display_mask)
            #cv2.waitKey(0)

            # print(f'mean this mask: \n{this_mask}')
            A1 = np.sum(this_mask)
            # print(f'sum this mask: \n{A1}')

            # Compare it with all other masks
            for m in range(N_masks_worms):
                # Skip self comparisons
                if n == m:
                    continue
                # Skip masks already eliminated
                if m in eliminated_idx:
                    continue
                # Check how much the smaller mask overlaps with the larger mask
                test_mask = np.mean(masks_worms[:, :, :, m],
                                    axis=2) > thresholds['worm_conf_pixel']

                ## Display the two masks we are comparing
                #base_mask = this_mask.copy().astype(np.uint8) * 220
                #compare_mask = test_mask.copy().astype(np.uint8) * 220
                #cv2.imshow(f'current image', image)
                #cv2.imshow(f'base mask {n}', base_mask)
                #cv2.imshow(f'comparing mask {m}', compare_mask)
                #cv2.waitKey(0)
                #cv2.destroyAllWindows()

                A2 = np.sum(test_mask)
                A_min = float(min((A1, A2)))
                #print(f'n: {n}, m: {m}, a1: {A1}, a2: {A2}')
                ol_mask = this_mask & test_mask
                A_ol = float(np.sum(ol_mask))

                # Flag indicating whether to merge two overlaping masks
                to_merge_overlap = False
                if A_min == 0:
                    to_merge_overlap = True
                elif A_ol / A_min > thresholds['worm_overlap']:
                    to_merge_overlap = True

                # If overlaps by more than the threshold, merge the two as union
                if to_merge_overlap:
                    new_mask = this_mask | test_mask
                    new_mask = np.stack((new_mask,) * 3, axis=-1).shape
                    masks_worms[:, :, :, n]  # Replace nth mask
                    masks_worms[:, :, :, m] = np.zeros(
                        masks_worms[:, :, :, n].shape, np.uint8)
                    # Set mth mask to empty
                    eliminated_idx.append(m)  # Mark mth mask as eliminated
                    #####print(f'=====================================eliminated a worm mask {m}')
                # print(f'eliminated: {eliminated_idx}')
        
        #if display_images:        
        #    final_worm_info = {'boxes':[], 'labels':[], 'scores':[], 'masks':[]}
        #    for n in range(N_masks_worms):
        #        print(f'{n} of {N_masks_worms - 1}')
        #        if n in eliminated_idx:
        #            print('continue')
        #            continue
        #        final_worm_info['boxes'].append(predictions_worms['boxes'][n].cpu().numpy().tolist())
        #        final_worm_info['labels'].append(predictions_worms['labels'][n].cpu().numpy().tolist())
        #        final_worm_info['scores'].append(predictions_worms['scores'][n].cpu().numpy().tolist())
        #        final_worm_info['masks'].append(predictions_worms['masks'][n].cpu().numpy().tolist())

        #    print(f'shape of predict mask: {predictions_worms["masks"][n].cpu().numpy().shape}')
        #    print(f'display worm mask 0 len: {len(final_worm_info["masks"][0])}')
        #    print(f'display worm mask 0 len: {len(final_worm_info["masks"][0][0])}')
        #    print(f'display worm mask 0 len: {len(final_worm_info["masks"][0][0][0])}')

        #    #print(f'final worm info boxes pre tensor: {final_worm_info}')
        #    final_worm_info['boxes'] = torch.tensor(final_worm_info['boxes'])
        #    #print(f'final worm info boxes: {final_worm_info["boxes"]}')
        #    final_worm_info['labels'] = torch.tensor(final_worm_info['labels'])
        #    final_worm_info['scores'] = torch.tensor(final_worm_info['scores'])
        #    final_worm_info['masks'] = torch.tensor(final_worm_info['masks'])
                
        #    #print(f'final worm info: {final_worm_info}')
        #    #final_worm_info['masks'] = np.array(final_worm_info['masks'])

        #    #final_worm_info = predictions_worms.copy()
        #    #for n in range(N_masks_worms):
        #    #    if n in eliminated_idx:
        #    #        final_worm_info['boxes'].append(predictions_worms['boxes'][n])
        
        #    worm_image = draw_masks_on_imgs([image],[final_worm_info], score_thresh=thresholds['worm_conf_image'], pix_conf_thresh=thresholds['worm_conf_pixel'],label_type='worm', include_orig_img = False)
        #    worm_image = worm_image[0]
        #    cv2.imshow('worm image', worm_image)
        #    cv2.waitKey(0)

        #for mask in range(N_masks_worms):
        #    cv2.imshow("Worm mask", mask)
        #    cv2.waitKey(0)


        ## PERFORM GENDER IDENTIFICATION
        # Perform inference
        model_gender.eval()  # For inference
        with torch.no_grad():
            # Returns prediction
            predictions_gender = model_gender([image_tensor])

        # print(f"gender predictions info: {predictions_gender}")

        start = printElapsedTime(start, "Gender Inference", print_elapsed_time)
        # print(f'gender predictions: {predictions_gender}')
        # Extract masks from predictions_worms
        # Always 1 element if 1 element list of images is supplied
        predictions_gender = predictions_gender[0]
        masks_gender = predictions_gender['masks'].permute(
            [2, 3, 1, 0]).cpu().numpy()

        # print('initial masks gender')
        # for x in masks_gender:
        #        print(np.amax(x))
        N_masks_gender = masks_gender.shape[3]

        # print(f'gender predictions: \n{predictions_gender}')
        # print(f'num gender masks: {N_masks_gender}')

        worm_gender_results = {}

        # Now for each worm mask we must look through the head and tail masks
        # and find the masks that overlap with the worm, and take the head and
        # tail with the highest overlap.
        for n in range(N_masks_worms):
            worm_gender_results[n] = {'head-score': 0, 'head-index': -1,
                                      'tail-score': 0, 'tail-index': -1,
                                      'tail-type': -1}
            if n in eliminated_idx:
                continue

            this_worm = np.mean(masks_worms[:, :, :, n],
                                axis=2) > thresholds['worm_conf_pixel']
            # print(f'mean this mask: \n{this_mask}')
            # A_worm = np.sum(this_worm)

            for m in range(N_masks_gender):
                # Ignore gender masks with very low confidence
                if predictions_gender['scores'][m] < thresholds[
                                                     'gender_conf_image']:
                    continue
                gender_mask = np.mean(masks_gender[:, :, :, m],
                                      axis=2) > thresholds['gender_conf_pixel']
                #print(f'gender mask shape: {gender_mask.shape}')
                A_gender = np.sum(gender_mask)
                ol_mask = this_worm & gender_mask
                A_ol = float(np.sum(ol_mask))

                if A_ol / A_gender >= thresholds['gender_overlap']:
                    if predictions_gender['labels'][m] == 1:  # Head
                        if predictions_gender['scores'][m] > \
                                worm_gender_results[n]['head-score']:
                            worm_gender_results[n]['head-score'] = \
                                predictions_gender['scores'][m]
                            worm_gender_results[n]['head-index'] = m
                    elif predictions_gender['labels'][m] == 2 or \
                            predictions_gender['labels'][m] == 3:  # Tail
                        if predictions_gender['scores'][m] > \
                                worm_gender_results[n]['tail-score']:
                            worm_gender_results[n]['tail-score'] = \
                                predictions_gender['scores'][m]
                            worm_gender_results[n]['tail-index'] = m
                            worm_gender_results[n]['tail-type'] = \
                                predictions_gender['labels'][m]
        
            ################## NEED TO INSPECT WORM GENDER RESULTS TO SEE HOW I CAN GO THROUGH IT AND GRAB ALL THE "WINNING" MASKS FROM THE ORIGINAL PREDICTIONS TO BUILD A SUBSET OF PREDICTIONS TO SEND TO DRAW IMAGES FUNCTION -- THIS IS FOR HAVING THE NICE MASK DISPLAY FOR GENDER THAT IS OVERLAYED UPON THE ORIGINAL IMAGE, CURRENTLY IT IS ONLY SHOWIUNG THE GENDER CENTROIDS
            #if display_images:
            #    # Display the gender masks we found for this worm
            #    this_worm_gender_masks = np.zeros(image.shape,
            #                        np.uint8)  # Preallocate allmasks
            #    #print(f'This worm gender masks shape: {this_worm_gender_masks.shape}')
            #    #####print(f'Investigating worm {n} gender masks')
            #    if worm_gender_results[n]['head-index'] != -1:
            #        #####print('head found')
            #        #display_head = (np.mean(masks_gender[:, :, :, worm_gender_results[n]['head-index']], axis=2) > thresholds['gender_conf_pixel']).astype(np.uint8)
            #        #print(f'display head: {display_head.shape}')
            #        #print(f'display head max: {np.amax(display_head)}')
            #        #display_head = cv2.multiply(display_head, 255)
            #        #cv2.imshow(f"Worm {n} display head", display_head)
            #        #cv2.waitKey(0)

            #        new_channel = cv2.add(this_worm_gender_masks[:,:,2],(np.mean(masks_gender[:, :, :, worm_gender_results[n]['head-index']], axis=2) > thresholds['gender_conf_pixel']).astype(np.uint8))
            #        new_channel = cv2.multiply(new_channel, 255)
            #        #print(f'new channel shape: {new_channel.shape}')
            #        #print(f'gender channel shape: {this_worm_gender_masks[:,:,2].shape}')
            #        #print(f'new channel max: {np.amax(new_channel)}')
            #        this_worm_gender_masks[:,:,2] = new_channel.copy()
            #        #print(f'gender channel max: {np.amax(this_worm_gender_masks[:,:,2])}')

            #        #display_mask = cv2.multiply(this_worm_gender_masks, 255)
            #        #print(f'gender max: {np.amax(this_worm_gender_masks)}')
            #        #print(f'gender 0 max: {np.amax(this_worm_gender_masks[:,:,0])}')
            #        #print(f'gender 1 max: {np.amax(this_worm_gender_masks[:,:,1])}')
            #        #print(f'gender 2 max: {np.amax(this_worm_gender_masks[:,:,2])}')
            #        #cv2.imshow(f"Worm head {n} gender masks (Red=Head, Green=Male Tail, Blue=Herm Tail)", this_worm_gender_masks)
            #        #cv2.waitKey(0)

            #    if worm_gender_results[n]['tail-index'] != -1:
            #        tail_channel = 0 # Herm tails are blue
            #        if worm_gender_results[n]['tail-type'] == 2: # Male, male tails are green
            #            #####print('male tail found')
            #            tail_channel = 1
            #        else:
            #            #####print('herm tail found')
            #            pass

            #        new_channel = cv2.add(this_worm_gender_masks[:,:,tail_channel],(np.mean(masks_gender[:, :, :, worm_gender_results[n]['tail-index']], axis=2) > thresholds['gender_conf_pixel']).astype(np.uint8))
            #        new_channel = cv2.multiply(new_channel, 255)
            #        this_worm_gender_masks[:,:,tail_channel] = new_channel.copy()

            #    display_mask = this_worm_gender_masks.copy()
            #    #display_mask = cv2.multiply(this_worm_gender_masks, 255)
            #    cv2.imshow(f"Worm {n} gender masks (Red=Head, Green=Male Tail, Blue=Herm Tail)", display_mask)
            #    cv2.waitKey(0)

        start = printElapsedTime(start, "Gender to worm mask overlap", print_elapsed_time)


        # print(f'Gender results: \n{worm_gender_results}')
        # Loop over the gender results and calculate the centroids for each head and tail that was found
        for i in worm_gender_results:
            if worm_gender_results[i]['head-index'] != -1:
                curr_mask = masks_gender[:, :, 0,
                            worm_gender_results[i]['head-index']]
                # print(f'pre conv shape: {curr_mask.shape}')
                # curr_mask = cv2.cvtColor(curr_mask,cv2.COLOR_RGB2GRAY)
                # print(f'post conv shape: {curr_mask.shape}')
                curr_mask = curr_mask > thresholds['gender_conf_pixel']

                curr_mask = np.array(curr_mask)
                # print(f'max: {np.amax(curr_mask)}')
                # print(curr_mask)
                y_ind, x_ind = np.where(curr_mask)
                # print(f'mask indices \n{mask_indices}')
                # print(f'mask indices: {len(mask_indices)}')

                cX = int(np.sum(x_ind) / len(x_ind))
                cY = int(np.sum(y_ind) / len(y_ind))

                centroid = (cX, cY)
                # print(f'centroid: {centroid}')

                ## convert image to grayscale image
                # gray_image = cv2.cvtColor(masks_gender[worm_gender_results[i]['head-index']], cv2.COLOR_RGB2GRAY)
                ## convert the grayscale image to binary image
                # ret,thresh = cv2.threshold(gray_image,127,255,0)
                ## calculate moments of binary image
                # M = cv2.moments(thresh)

                ## calculate x,y coordinate of center
                # cX = int(M["m10"] / M["m00"])
                # cY = int(M["m01"] / M["m00"])

                # head_mask = np.mean(masks_gender[worm_gender_results[i]['head-index']],axis=2)>conf_thresh
                # head_mask = np.array(head_mask)
                # head_mask = head_mask.astype(np.float32)
                ##print(head_mask)
                ##print(head_mask.shape)
                ##head_mask = np.array(head_mask)
                ## calculate moments of binary image
                # M = cv2.moments(head_mask)
                ## calculate x,y coordinate of center
                # cX = int(M["m10"] / M["m00"])
                # cY = int(M["m01"] / M["m00"])
                worm_gender_results[i]['head-centroid'] = centroid

                # cv2.circle(image, centroid, 5,(255, 255, 255), -1)
                # cv2.imshow("centroid", image)
                # cv2.waitKey(0)

            if worm_gender_results[i]['tail-index'] != -1:
                curr_mask = masks_gender[:, :, 0,
                            worm_gender_results[i]['tail-index']]
                # print(f'pre conv shape: {curr_mask.shape}')
                # curr_mask = cv2.cvtColor(curr_mask,cv2.COLOR_RGB2GRAY)
                # print(f'post conv shape: {curr_mask.shape}')
                curr_mask = curr_mask > thresholds['gender_conf_pixel']

                curr_mask = np.array(curr_mask)
                # print(f'max: {np.amax(curr_mask)}')
                # print(curr_mask)
                y_ind, x_ind = np.where(curr_mask)
                # print(f'mask indices \n{mask_indices}')
                # print(f'mask indices: {len(mask_indices)}')

                cX = int(np.sum(x_ind) / len(x_ind))
                cY = int(np.sum(y_ind) / len(y_ind))

                centroid = (cX, cY)
                # print(f'centroid: {centroid}')
                worm_gender_results[i]['tail-centroid'] = centroid

                # cv2.circle(image, centroid, 5,(255, 255, 255), -1)
                # cv2.imshow("centroid", image)
                # cv2.waitKey(0)

        # print(f'Gender results: \n{worm_gender_results}')

        start = printElapsedTime(start, "Head/Tail centroid calculation", print_elapsed_time)

        ########

        # Add masks_worms to a single image for returning to client
        #all_masks_worms = np.zeros(image.shape,np.uint8)  # Preallocate allmasks
        #all_masks_worms = np.zeros((image.shape[0],image.shape[1],1),np.uint8)  # Preallocate allmasks
        #all_masks_worms = np.zeros((image.shape[0],image.shape[1]),np.uint8)  # Preallocate allmasks
        all_masks_worms = np.full((image.shape[0],image.shape[1]),255, np.uint8)  # Preallocate allmasks, we create a all white image so we can use np's minumum function and keep the masks, which will have lower pixel values (and the lower the pixel value the higher the mask's overall score)

        #####print(f' all masks shape: {all_masks_worms.shape}')
        image_pix_modifier = 1
        for n in range(N_masks_worms):
            # Extract current mask
            if n not in eliminated_idx:
                this_mask = masks_worms[:, :, :, n]  # Get the n'th prediciton mask
                mask_pix_value = n + 1

                # If not, add it as a new mask with a unique ID. Note that we subtract one from the image in order to make the background white (255). This way the mask is the smaller values in the image.
                this_mask = ((this_mask > thresholds[
                    'worm_conf_pixel']).astype(np.uint8) * mask_pix_value) - image_pix_modifier

                
                # cv2.imshow("this masks", this_mask)
                # cv2.waitKey(0)
                #####print(f'$$$$$$$$$$$$$$$$$ shape of this mask: {this_mask.shape}')
                #####print(f'$$$$$$$$$$$$$$$$$ max of this mask channel 0: {np.amax(this_mask[:,:,0])}')
                #print(f'$$$$$$$$$$$$$$$$$ max of this mask channel 1: {np.amax(this_mask[:,:,1])}')
                #print(f'$$$$$$$$$$$$$$$$$ max of this mask channel 2: {np.amax(this_mask[:,:,2])}')

                # Now we want to add the new mask to the current image containing all the masks thus far. However, if there is overlap between the two masks we will keep the value of the current mask (the masks are entered in order of overall confidence, so the current mask will have a higher confidence than the new mask)
                all_masks_worms = np.minimum(all_masks_worms, this_mask[:,:,0])
                #for row in range(all_masks_worms.shape[0]):
                #    for col in range(all_masks_worms.shape[1]):
                #        if this_mask[row][col][0] != 0 and all_masks_worms[row][col] == 0:
                #            all_masks_worms[row][col] = mask_pix_value

                

                #print('1')
                #lower_mask_idxs = np.where(all_masks_worms!=0)
                #print('2')
                #this_mask_idxs = np.where(this_mask != 0)
                #print('3')
                #lower_mask_idxs = [(lower_mask_idxs[0][i], lower_mask_idxs[1][i]) for i in range(len(lower_mask_idxs[0]))]
                #print('4')
                #this_mask_idxs = [(this_mask_idxs[0][i],this_mask_idxs[1][i]) for i in range(len(this_mask_idxs[0]))]
                #print('5')
                #idxs_to_replace = [i for i in this_mask_idxs if i not in lower_mask_idxs]
                #print('6')
                #idxs_to_replace = ([i[0] for i in idxs_to_replace], [i[1] for i in idxs_to_replace])
                #print('7')

                ## Now replace all the pixels in the final mask that were in the new mask but not already in the final mask
                #all_masks_worms[idxs_to_replace] = n + 1

                ##all_masks_worms += this_mask

                #####print(f'$$$$$$$$$$$$$$$$$ shape of all masks: {all_masks_worms.shape}')
                #####print(f'$$$$$$$$$$$$$$$$$ max of all mask channel 0: {np.amax(all_masks_worms[:,:])}')
                #####print(f'$$$$$$$$$$$$$$$$$ max of all mask channel 0: {np.amax(all_masks_worms)}')
                #print(f'$$$$$$$$$$$$$$$$$ max of all mask channel 1: {np.amax(all_masks_worms[:,:,1])}')
                #print(f'$$$$$$$$$$$$$$$$$ max of all mask channel 2: {np.amax(all_masks_worms[:,:,2])}')

            else:
                #####print(f'skipping eliminated index {n} in all masks worms of {N_masks_worms}')
                pass

        all_masks_worms = all_masks_worms + image_pix_modifier # add back in the modifier to un-invert the image. The white background becomes black (0) again and all the worm masks return to their appropriate pixel values.

        if display_images:        
            final_worm_info = {'boxes':[], 'labels':[], 'scores':[], 'masks':[]}
            for n in range(N_masks_worms):
                if n in eliminated_idx:
                    continue
                #final_worm_info['boxes'].append(predictions_worms['boxes'][n].cpu().numpy().tolist())
                final_worm_info['labels'].append(predictions_worms['labels'][n].cpu().numpy().tolist())
                final_worm_info['boxes'].append([-10,-10,image.shape[1] + 10,image.shape[0] + 10]) # Since its just finding worm we aren't interested in seeing the boxes and labels drawn on the image, so make the bounding box off screen (which will also push the label off screen)
                final_worm_info['scores'].append(predictions_worms['scores'][n].cpu().numpy().tolist())

                this_mask = (all_masks_worms == n + 1).astype(np.uint8)
                print(f'later mask shape: {this_mask.shape}')
                this_mask = this_mask[np.newaxis, ...]
                print(f'later mask shape: {this_mask.shape}')
                this_mask = this_mask.tolist()
                final_worm_info['masks'].append(this_mask)

            #print(f'final worm info boxes pre tensor: {final_worm_info}')
            final_worm_info['boxes'] = torch.tensor(final_worm_info['boxes'])
            #print(f'final worm info boxes: {final_worm_info["boxes"]}')
            final_worm_info['labels'] = torch.tensor(final_worm_info['labels'])
            final_worm_info['scores'] = torch.tensor(final_worm_info['scores'])
            final_worm_info['masks'] = torch.tensor(final_worm_info['masks'])
                
            #print(f'final worm info: {final_worm_info}')
            #final_worm_info['masks'] = np.array(final_worm_info['masks'])

            #final_worm_info = predictions_worms.copy()
            #for n in range(N_masks_worms):
            #    if n in eliminated_idx:
            #        final_worm_info['boxes'].append(predictions_worms['boxes'][n])
        
            worm_image = draw_masks_on_imgs([image],[final_worm_info], score_thresh=thresholds['worm_conf_image'], pix_conf_thresh=0,label_type='worm', include_orig_img = False)
            worm_image = worm_image[0]

            #print(f'worm gender results: {worm_gender_results}')

            for worm in worm_gender_results.values():
                if worm['head-index'] != -1:
                    loc = worm['head-centroid']
                    loc_text = (worm['head-centroid'][0] + 10, worm['head-centroid'][1] + 5)
                    color = (0,0,255)
                    worm_image = cv2.circle(worm_image,loc,radius=8,color=color,thickness=-1)
                    cv2.putText(worm_image, "Head" ,loc_text, cv2.FONT_HERSHEY_SIMPLEX, 0.65, color, 2)
                if worm['tail-index'] != -1:
                    loc = worm['tail-centroid']
                    loc_text = (worm['tail-centroid'][0] + 10, worm['tail-centroid'][1] + 5)

                    if worm['tail-type'] == 2:
                        color = (0,255,0)
                        worm_image = cv2.circle(worm_image,loc,radius=8,color=color,thickness=-1)
                        cv2.putText(worm_image, "Tail (M)" ,loc_text, cv2.FONT_HERSHEY_SIMPLEX, 0.65, color, 2)
                    elif worm['tail-type'] == 3:
                        color = (255,0,0)
                        worm_image = cv2.circle(worm_image,loc,radius=8,color=color,thickness=-1)
                        cv2.putText(worm_image, "Tail [H]" ,loc_text, cv2.FONT_HERSHEY_SIMPLEX, 0.65, color, 2)


            cv2.imshow('worm image', worm_image)
            cv2.waitKey(0)

        if display_images: 
            # Display the final image of all worm masks
            #####print('Displaying final worm masks to be sent back to C++') 
            display_mask = all_masks_worms[:,:].copy()
            display_mask = cv2.multiply(display_mask, 50)
            cv2.imshow("Worm Masks", display_mask)
            cv2.waitKey(1000)

        start = printElapsedTime(start, "Mask extraction", print_elapsed_time)


        # Send grayscale mask image back to C++

        # cv2.imshow("worm masks", all_masks_worms[:, :, 0])
        # cv2.waitKey(0)
        #encoded_image = encodeImageForSocket(all_masks_worms[:, :, 0], worm_gender_results)
        encoded_image = encodeImageForSocket(all_masks_worms[:, :], worm_gender_results)
        clientsocket.send(encoded_image.encode())
        start = printElapsedTime(start, "Sending image back to C++",
                                 print_elapsed_time)
        #print(f'worm gender results: {worm_gender_results}')


if __name__ == "__main__":
    # Auto-select GPU or CPU
    use_cuda = torch.cuda.is_available()  # Automatically decide whether to use CUDA
    model_worm = loadRCNN(fpth_worm, 2, use_cuda)
    model_gender = loadRCNN(fpth_gender, 4, use_cuda)

    # Setup the server socket to listen
    serversocket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    serversocket.bind((HOST, PORT))
    serversocket.listen(10)

    print("Server set up and listening: CUDA = ", use_cuda)
    # MAIN LOOP: SERVER LISTENS FOR IMAGE INPUT TO ANALYZE
    while 1:

        # It "terminate" was sent to the buffer at any point, terminate
        if "terminate" in buf.lower():
            clientsocket.send('closing server'.encode())
            time.sleep(1.5)
            exit(0)

        # accept connections from outside
        (clientsocket, address) = serversocket.accept()
        time.sleep(
            0.15)  # This little pause seems to prevent partial-buffer transfer errors.

        ct = Thread(target=handle, args=(clientsocket, False))
        ct.start()

pass
