"""
    RCNN_Mask_Testing.py

    Test the mask network on a set of non-training images to evaluate the
    accuracy

    If testing with file, the script reads a .txt file where each line contains
    gender info for one image in the format of img_name,gender_info_encoded_str

    If testing with server, the script must run while the server
    (RCNN_Segment_Image.py) is running.

    ASSUMPTION: the testing images have human-drawn masks that contain
    exactly a head and a tail for each worm (no missing heads/tails); none of
    the contours should be touching each other

    Reports:
    (1) % correct heads
    (2) wrong head type: % missing, % diff
    (3) % correct tails
    (4) wrong tail type: % missing, % diff
    (5) % correct gender
    (6) % correct overall worm

    All stats are based on the total number of worms in human masks.

    In the case of extra AI mask/worms, a warning will be printed and the
    extra AI masks may amplify certain statistics.

"""
import tkinter as tk
import tkinter.filedialog
from verifyDirlist import *
import os
import matplotlib.pyplot as plt
import seaborn as sns
import numpy as np
import socket
import cv2
import time
from RCNN_Segment_Image import encodeImageForSocket, decodeGender, \
    encodeThresholds, MAX_LENGTH

# Parameters for thresholds for optimization / scanning
worm_conf_image_levels = np.round(np.arange(0.05, 1.0, 0.125), 3)
gender_conf_pixel_levels = np.round(np.arange(0.05, 1.0, 0.125), 3)
# Flag for whether to display the manual mask image with AI centroids
show_debug_images = False
# Flag for whether to print info about mistakes
show_error_info = True
# Flag for testing with file or server
with_file = False


# Check if the square area centered at the given x and y with the given radius
# has a pixel of the matching color in the manual mask specified. The radius
# are in unit of pixels. The color follows BGR, so 0 = B, 1 = G, 2 = R. The
# manual mask is an np array representing the manual mask image, and all other
# inputs are ints. Returns True if there is a matching color pixel in that area
# and False if not.
def check_area(manual_mask, center_x, center_y, radius, color):
    for row in range(center_y - radius, center_y + radius + 1):
        for col in range(center_x - radius, center_x + radius + 1):
            try:
                if manual_mask[row, col][color] >= 150:
                    return True
            except IndexError:
                pass
    return False


# Run the evaluation and outputs the statistics on accuracy for H/T
# classification and gender identification given in the .txt file specified by
# the filename parameter. Return the stats as a dictionary with the following
# keys: total_worms_human, total_worms_ai, gender_denominator, head_correct,
# head_missing, head_diff, tail_correct, tail_missing, tail_diff, gender
# correct (-1 if no gender data), overall (the first two are int denominators,
# and the rest are float percentages).
def test_with_file(filename):
    # Initialize the variables for storing the statistics
    # the number of head centroids that fall into human-drawn heads
    num_head_correct = 0
    # a dict of length 2 indicating the type of head errors; the
    # 1st element is the number of missing heads, and the 2nd
    # element is the number of heads in different locations from
    # the manual heads
    head_error_type_num = {'missing': 0, 'diff': 0}
    # the number of tail centroids that fall into human-drawn tails
    num_tail_correct = 0
    # a dict of length 2 indicating the type of tail errors; the
    # 1st element is the number of missing tails, and the 2nd
    # element is the number of tails in different locations from
    # the manual tails
    tail_error_type_num = {'missing': 0, 'diff': 0}
    # the number of worms with predicted gender tails that match
    # the gender from their human-drawn tail mask
    num_gender_correct = 0
    # the number of worms that are correct overall (head & tail & gender all
    # correct)
    num_worm_correct = 0
    # the total number of human-found worms (by the assumption of one head and
    # one tail per worm, this is also the total number of heads/tails)
    total_worms_human = 0
    # the total number of AI worms
    total_worms_ai = 0
    # the total number of images
    total_images = 0

    # Read the file of encoded strings for all images
    with open(filename, 'r') as gender_file:
        gender_info_lines = gender_file.readlines()

    # Test on the testing image dataset
    for line in gender_info_lines:
        # Get the image name and the encoded string from the current line
        if len(line) > 2:
            img_name, gender_encoded = line.split(',', 1)
        else:
            continue

        # Check if a manual mask exists and obtain its path
        manual_mask_name = img_name.replace('.png', '_mask.png')
        manual_mask_path = os.path.join(p_mask, manual_mask_name)
        if not os.path.exists(manual_mask_path):
            print('Skipped (no manual mask): ', img_name)
            continue

        total_images += 1

        # Read the manual mask
        manual_mask = cv2.imread(manual_mask_path,
                                 cv2.IMREAD_COLOR)

        # Count the total number of worms in the manual mask image
        manual_mask_gray = cv2.cvtColor(manual_mask,
                                        cv2.COLOR_BGR2GRAY)
        ret, thresh = cv2.threshold(manual_mask_gray, 1, 255,
                                    cv2.THRESH_BINARY)
        contours, hierarchy = cv2.findContours(thresh, cv2.RETR_LIST,
                                               cv2.CHAIN_APPROX_NONE)
        # len(contours) should always be a multiple of 2 if the assumption of
        # one head and one tail for each worm holds
        num_worms = np.ceil(len(contours) / 2).astype(int)
        total_worms_human += num_worms

        # Decode the AI gender info for this image
        gender_info = decodeGender(gender_encoded)

        # The number of missing worms (for which the AI misses both the head
        # and the tail)
        num_missing_worms = num_worms - len(gender_info)
        # If AI finds more worms than the human:
        if num_missing_worms < 0:
            print(f'Warning: {img_name} has {(-1) * num_missing_worms} extra '
                  f'AI worms')
            num_missing_worms = 0
        elif num_missing_worms > 0 and show_error_info:
            print(img_name, 'full worm missing')

        # Variables for single misses in AI worm masks
        num_missing_heads = 0
        num_missing_tails = 0

        # Make a copy of the manual mask image for displaying
        manual_mask_copy = np.copy(manual_mask)

        # Test each worm from the AI mask image for mask correctness.
        # It is guaranteed from the decodeGender method that the masks list
        # associated with a worm is always either 1 (just H/T) or 2 (one H and
        # one T).
        for worm, masks in gender_info.items():
            # print('------------------------------------------------')
            total_worms_ai += 1
            has_head = False
            has_tail = False
            head_correct = False
            tail_and_gen_correct = False
            for predicted_mask in masks:
                # the pixel from the manual mask at the location of the
                # centroid of the predicted mask
                cX = predicted_mask['cX']
                cY = predicted_mask['cY']
                centroid_pixel = manual_mask[cY, cX]
                # # Print out the info about the centroid (for debugging)
                # print(f'AI mask type: {predicted_mask["type"]}')
                # print(f'centroid cX: {cX}  cY: {cY}')
                # Draw the AI centroid on manual mask image
                cv2.circle(manual_mask_copy, (cX, cY), 3, (255, 255, 255), -1)

                # The radius of the area around the centroid to check
                radius = 20

                # if AI mask is head (red)
                if predicted_mask['type'] == 1:
                    has_head = True
                    cv2.putText(manual_mask_copy, "AI head",
                                (cX - 25, cY - 25),
                                cv2.FONT_HERSHEY_SIMPLEX,
                                0.5, (255, 255, 255), 2)
                    # if the area around the centroid is red
                    if check_area(manual_mask, cX, cY, radius, color=2):
                        head_correct = True
                        num_head_correct += 1
                    else:
                        head_error_type_num['diff'] += 1
                        if show_error_info:
                            if predicted_mask['conf_score'] == -1:
                                print(img_name, 'c++ head incorrect')
                            else:
                                print(img_name, 'python head incorrect')
                # if AI mask is male tail (green)
                elif predicted_mask['type'] == 2:
                    has_tail = True
                    cv2.putText(manual_mask_copy, "AI tail (male)",
                                (cX - 25, cY - 25),
                                cv2.FONT_HERSHEY_SIMPLEX,
                                0.5, (255, 255, 255), 2)
                    # if the area around the centroid is green
                    if check_area(manual_mask, cX, cY, radius, color=1):
                        tail_and_gen_correct = True
                        num_tail_correct += 1
                        num_gender_correct += 1
                    # if the area around the centroid is blue
                    elif check_area(manual_mask, cX, cY, radius, color=0):
                        num_tail_correct += 1
                        if show_error_info:
                            print(img_name, 'gender incorrect')
                    # if the area around the centroid is red or black
                    # (data point discarded for gender)
                    else:
                        tail_error_type_num['diff'] += 1
                        if show_error_info:
                            print(img_name, 'python tail incorrect')
                # if AI mask is herm tail (blue)
                else:
                    has_tail = True
                    # Tail masks generated with C++ geometry constrains -
                    # has no gender
                    if predicted_mask['conf_score'] == -1:
                        cv2.putText(manual_mask_copy, "AI tail (no gender)",
                                    (cX - 25, cY - 25),
                                    cv2.FONT_HERSHEY_SIMPLEX,
                                    0.5, (255, 255, 255), 2)
                        # if the area around the centroid is blue or green
                        if check_area(manual_mask, cX, cY, radius, color=0) or \
                           check_area(manual_mask, cX, cY, radius, color=1):
                            num_tail_correct += 1
                            if show_error_info:
                                print(img_name, 'c++ tail correct, no gender')
                        else:
                            tail_error_type_num['diff'] += 1
                            if show_error_info:
                                print(img_name, 'c++ tail incorrect')
                    # Real herm tail masks
                    else:
                        cv2.putText(manual_mask_copy, "AI tail (herm)",
                                    (cX - 25, cY - 25),
                                    cv2.FONT_HERSHEY_SIMPLEX,
                                    0.5, (255, 255, 255), 2)
                        # if the area around the centroid is blue
                        if check_area(manual_mask, cX, cY, radius, color=0):
                            tail_and_gen_correct = True
                            num_tail_correct += 1
                            num_gender_correct += 1
                        # if the area around the centroid is green
                        elif check_area(manual_mask, cX, cY, radius, color=1):
                            num_tail_correct += 1
                            if show_error_info:
                                print(img_name, 'gender incorrect')
                        # if the area around the centroid is red or black
                        # (data point discarded for gender)
                        else:
                            tail_error_type_num['diff'] += 1
                            if show_error_info:
                                print(img_name, 'python tail incorrect')
            if not has_head:
                num_missing_heads += 1
                if show_error_info:
                    print(img_name, 'head missing')
            if not has_tail:
                num_missing_tails += 1
                if show_error_info:
                    print(img_name, 'tail missing')
            if head_correct and tail_and_gen_correct:
                num_worm_correct += 1

        head_error_type_num['missing'] += num_missing_heads + num_missing_worms
        tail_error_type_num['missing'] += num_missing_tails + num_missing_worms

        # print(f'{img_name} processed.')

        # Display the manual mask with AI centroids drawn
        if show_debug_images:
            cv2.imshow("Manual mask with AI centroids", manual_mask_copy)
            cv2.waitKey(0)

    # Calculate and report the percentages
    if total_worms_human == 0:
        print('No worms found in the human-drawn masks. '
              'Please check the dataset.')
        return -1.0
    else:
        # (1) % correct heads
        percent_head_correct = num_head_correct / total_worms_human
        # (2) wrong head type: % missing, % diff
        head_error_type_percent = {
            'missing': head_error_type_num[
                           'missing'] / total_worms_human,
            'diff': head_error_type_num['diff'] / total_worms_human}
        # (3) % correct tails
        percent_tail_correct = num_tail_correct / total_worms_human
        # (4) wrong tail type: % missing, % diff
        tail_error_type_percent = {
            'missing': tail_error_type_num[
                           'missing'] / total_worms_human,
            'diff': tail_error_type_num['diff'] / total_worms_human}
        # (5) % correct gender
        percent_gender_correct = num_gender_correct / total_worms_human
        # (6) % correct overall worm
        percent_overall_correct = num_worm_correct / total_worms_human

        # Store and print the percentages
        results = {}
        print('Evaluation finished.')
        print(f'Total number of images: {total_images}')
        results['total_worms_human'] = total_worms_human
        print(f'Total number of worms found by human: {total_worms_human}')
        results['total_worms_ai'] = total_worms_ai
        print(f'Total number of worms found by AI: {total_worms_ai}')
        print('------------------------------------------------')
        results['head_correct'] = percent_head_correct
        print(f'Percent head correct: {round(percent_head_correct * 100, 2)}%')
        results['head_missing'] = head_error_type_percent["missing"]
        print(f'Percent head missing: '
              f'{round(head_error_type_percent["missing"] * 100, 2)}%')
        results['head_diff'] = head_error_type_percent["diff"]
        print(f'Percent head diff: '
              f'{round(head_error_type_percent["diff"] * 100, 2)}%')
        print('------------------------------------------------')
        results['tail_correct'] = percent_tail_correct
        print(f'Percent tail correct: {round(percent_tail_correct * 100, 2)}%')
        results['tail_missing'] = tail_error_type_percent["missing"]
        print(f'Percent tail missing: '
              f'{round(tail_error_type_percent["missing"] * 100, 2)}%')
        results['tail_diff'] = tail_error_type_percent["diff"]
        print(f'Percent tail diff: '
              f'{round(tail_error_type_percent["diff"] * 100, 2)}%')
        print('------------------------------------------------')
        results['gender_correct'] = percent_gender_correct
        print(f'Percent gender correct: '
              f'{round(percent_gender_correct * 100, 2)}%')
        print('------------------------------------------------')
        results['overall'] = percent_overall_correct
        print(f'Percent overall correct: '
              f'{round(percent_overall_correct * 100, 2)}%')
        print('------------------------------------------------')

        # Return the stats as a dictionary
        return results


# Run the evaluation and outputs the statistics on accuracy for H/T
# classification and gender identification. Take in a dictionary of thresholds.
# Return the stats as a dictionary with the following keys: total_worms,
# gender_denominator, head_correct, head_missing, head_diff, tail_correct,
# tail_missing, tail_diff, gender_correct (-1 if no gender data), overall
# (the first two are int denominators, and the rest are float percentages).
def test_with_thresh(thresholds):
    # Initialize the variables for storing the statistics
    # the number of head centroids that fall into human-drawn heads
    num_head_correct = 0
    # a dict of length 2 indicating the type of head errors; the
    # 1st element is the number of missing heads, and the 2nd
    # element is the number of heads in different locations from
    # the manual heads
    head_error_type_num = {'missing': 0, 'diff': 0}
    # the number of tail centroids that fall into human-drawn tails
    num_tail_correct = 0
    # a dict of length 2 indicating the type of tail errors; the
    # 1st element is the number of missing tails, and the 2nd
    # element is the number of tails in different locations from
    # the manual tails
    tail_error_type_num = {'missing': 0, 'diff': 0}
    # the number of worms with predicted gender tails that match
    # the gender from their human-drawn tail mask
    num_gender_correct = 0
    # the number of worms that are correct overall (head & tail & gender all
    # correct)
    num_worm_correct = 0
    # the total number of human-found worms (by the assumption of one head and
    # one tail per worm, this is also the total number of heads/tails)
    total_worms_human = 0
    # the total number of AI worms
    total_worms_ai = 0
    # the total number of images
    total_images = 0

    # Test on the testing image dataset
    for i, (img_path, img_name) in enumerate(zip(img_paths, img_names)):
        # Report progress
        if i != 0 and i % 10 == 0:
            # print('------------------------------------------------')
            print(f'PROGRESS REPORT: {i} out of {len(img_paths)} images '
                  f'processed.')

        # Check if a manual mask exists and obtain its path
        manual_mask_name = img_name.replace('.png', '_mask.png')
        manual_mask_path = os.path.join(p_mask, manual_mask_name)
        if not os.path.exists(manual_mask_path):
            print('Skipped (no manual mask): ', img_name)
            continue

        total_images += 1

        # Read the manual mask
        manual_mask = cv2.imread(manual_mask_path,
                                 cv2.IMREAD_COLOR)

        # Count the total number of worms in the manual mask image
        manual_mask_gray = cv2.cvtColor(manual_mask,
                                        cv2.COLOR_BGR2GRAY)
        ret, thresh = cv2.threshold(manual_mask_gray, 1, 255,
                                    cv2.THRESH_BINARY)
        contours, hierarchy = cv2.findContours(thresh, cv2.RETR_LIST,
                                               cv2.CHAIN_APPROX_NONE)
        # len(contours) should always be a multiple of 2 if the assumption of
        # one head and one tail for each worm holds
        num_worms = np.ceil(len(contours) / 2).astype(int)
        total_worms_human += num_worms

        # Make a copy of the manual mask image for displaying
        manual_mask_copy = np.copy(manual_mask)

        # Encode image
        test_img = cv2.imread(img_path, cv2.IMREAD_GRAYSCALE)
        encoded_img = encodeImageForSocket(test_img)

        # Encode thresholds
        encoded_thresh = encodeThresholds(thresholds)

        # Send encoded image and thresholds to the server
        s.send((encoded_img + ',' + encoded_thresh).encode())

        # Receive and decode the masks returned from the server
        result = s.recv(MAX_LENGTH)
        gender_info = decodeGender(result)

        # The number of missing worms (for which the AI misses both the head
        # and the tail)
        num_missing_worms = num_worms - len(gender_info)
        # If AI finds more worms than the human:
        if num_missing_worms < 0:
            print(f'Warning: {img_name} has {(-1) * num_missing_worms} extra '
                  f'AI worms')
            num_missing_worms = 0

        # Variables for single misses in AI worm masks
        num_missing_heads = 0
        num_missing_tails = 0

        # Test each worm from the AI mask image for mask correctness.
        # It is guaranteed from the decodeGender method that the masks list
        # associated with a worm is always either 1 (just H/T) or 2 (one H and
        # one T).
        for worm, masks in gender_info.items():
            # print('------------------------------------------------')
            total_worms_ai += 1
            has_head = False
            has_tail = False
            head_correct = False
            tail_and_gen_correct = False
            for predicted_mask in masks:
                # the pixel from the manual mask at the location of the
                # centroid of the predicted mask
                cX = predicted_mask['cX']
                cY = predicted_mask['cY']
                centroid_pixel = manual_mask[cY, cX]
                # # Print out the info about the centroid (for debugging)
                # print(f'AI mask type: {predicted_mask["type"]}')
                # print(f'centroid cX: {cX}  cY: {cY}')
                # Draw the AI centroid on manual mask image
                cv2.circle(manual_mask_copy, (cX, cY), 3, (255, 255, 255), -1)

                # The radius of the area around the centroid to check
                radius = 20

                # if AI mask is head (red)
                if predicted_mask['type'] == 1:
                    has_head = True
                    cv2.putText(manual_mask_copy, "AI head",
                                (cX - 25, cY - 25),
                                cv2.FONT_HERSHEY_SIMPLEX,
                                0.5, (255, 255, 255), 2)
                    # if the area around the centroid is red
                    if check_area(manual_mask, cX, cY, radius, color=2):
                        head_correct = True
                        num_head_correct += 1
                    else:
                        head_error_type_num['diff'] += 1
                # if AI mask is male tail (green)
                elif predicted_mask['type'] == 2:
                    has_tail = True
                    cv2.putText(manual_mask_copy, "AI tail (male)",
                                (cX - 25, cY - 25),
                                cv2.FONT_HERSHEY_SIMPLEX,
                                0.5, (255, 255, 255), 2)
                    # if the area around the centroid is green
                    if check_area(manual_mask, cX, cY, radius, color=1):
                        tail_and_gen_correct = True
                        num_tail_correct += 1
                        num_gender_correct += 1
                    # if the area around the centroid is blue
                    elif check_area(manual_mask, cX, cY, radius, color=0):
                        num_tail_correct += 1
                    # if the area around the centroid is red or black
                    # (data point discarded for gender)
                    else:
                        tail_error_type_num['diff'] += 1
                # if AI mask is herm tail (blue)
                else:
                    has_tail = True
                    cv2.putText(manual_mask_copy, "AI tail (herm)",
                                (cX - 25, cY - 25),
                                cv2.FONT_HERSHEY_SIMPLEX,
                                0.5, (255, 255, 255), 2)
                    # if the area around the centroid is blue
                    if check_area(manual_mask, cX, cY, radius, color=0):
                        tail_and_gen_correct = True
                        num_tail_correct += 1
                        num_gender_correct += 1
                    # if the area around the centroid is green
                    elif check_area(manual_mask, cX, cY, radius, color=1):
                        num_tail_correct += 1
                    # if the area around the centroid is red or black
                    # (data point discarded for gender)
                    else:
                        tail_error_type_num['diff'] += 1
            if not has_head:
                num_missing_heads += 1
            if not has_tail:
                num_missing_tails += 1
            if head_correct and tail_and_gen_correct:
                num_worm_correct += 1

        head_error_type_num[
            'missing'] += num_missing_heads + num_missing_worms
        tail_error_type_num[
            'missing'] += num_missing_tails + num_missing_worms

        # print(f'{img_name} processed.')

        # Display the manual mask with AI centroids drawn
        if show_debug_images:
            cv2.imshow("Manual mask with AI centroids", manual_mask_copy)
            cv2.waitKey(0)

    # Calculate and report the percentages
    if total_worms_human == 0:
        print('No worms found in the human-drawn masks. '
              'Please check the dataset.')
        return -1.0
    else:
        # (1) % correct heads
        percent_head_correct = num_head_correct / total_worms_human
        # (2) wrong head type: % missing, % diff
        head_error_type_percent = {
            'missing': head_error_type_num[
                           'missing'] / total_worms_human,
            'diff': head_error_type_num['diff'] / total_worms_human}
        # (3) % correct tails
        percent_tail_correct = num_tail_correct / total_worms_human
        # (4) wrong tail type: % missing, % diff
        tail_error_type_percent = {
            'missing': tail_error_type_num[
                           'missing'] / total_worms_human,
            'diff': tail_error_type_num['diff'] / total_worms_human}
        # (5) % correct gender
        percent_gender_correct = num_gender_correct / total_worms_human
        # (6) % correct overall worm
        percent_overall_correct = num_worm_correct / total_worms_human

        # Store and print the percentages
        results = {}
        print('Evaluation finished.')
        print(f'Total number of images: {total_images}')
        results['total_worms_human'] = total_worms_human
        print(f'Total number of worms found by human: {total_worms_human}')
        results['total_worms_ai'] = total_worms_ai
        print(f'Total number of worms found by AI: {total_worms_ai}')
        print('------------------------------------------------')
        results['head_correct'] = percent_head_correct
        print(f'Percent head correct: {round(percent_head_correct * 100, 2)}%')
        results['head_missing'] = head_error_type_percent["missing"]
        print(f'Percent head missing: '
              f'{round(head_error_type_percent["missing"] * 100, 2)}%')
        results['head_diff'] = head_error_type_percent["diff"]
        print(f'Percent head diff: '
              f'{round(head_error_type_percent["diff"] * 100, 2)}%')
        print('------------------------------------------------')
        results['tail_correct'] = percent_tail_correct
        print(f'Percent tail correct: {round(percent_tail_correct * 100, 2)}%')
        results['tail_missing'] = tail_error_type_percent["missing"]
        print(f'Percent tail missing: '
              f'{round(tail_error_type_percent["missing"] * 100, 2)}%')
        results['tail_diff'] = tail_error_type_percent["diff"]
        print(f'Percent tail diff: '
              f'{round(tail_error_type_percent["diff"] * 100, 2)}%')
        print('------------------------------------------------')
        results['gender_correct'] = percent_gender_correct
        print(f'Percent gender correct: '
              f'{round(percent_gender_correct * 100, 2)}%')
        print('------------------------------------------------')
        results['overall'] = percent_overall_correct
        print(f'Percent overall correct: '
              f'{round(percent_overall_correct * 100, 2)}%')
        print('------------------------------------------------')

        # Return the stats as a dictionary
        return results


# Run evaluation on a set of combinations of gender thresholds to find and
# return the one that creates the maximum percentage for overall correctness
# as a dictionary.
def find_best_thresh():
    # Best threshold combo
    thresholds_best = {}
    # The maximum overall correct percentage
    max_overall_correct = -1.0

    # The np array storing threshold to correctness
    correctness = np.zeros(
        (len(gender_conf_pixel_levels), len(worm_conf_image_levels)),
        np.float64)

    start_time = time.time()
    row = 0
    for gender_conf_pixel_thresh in gender_conf_pixel_levels:
        col = 0
        for worm_conf_image_thresh in worm_conf_image_levels:
            # Estimate the time remaining to run
            if row != 0 or col != 0:
                time_elapsed = time.time() - start_time
                time_remaining = time_elapsed * (len(gender_conf_pixel_levels) - row - 1) * (
                            len(worm_conf_image_levels) - col - 1)
                min_remaining = int(time_remaining / 60)
                sec_remaining = int(time_remaining % 60)
                print(f'Estimated time remaining: {min_remaining} '
                      f'minutes and {sec_remaining} seconds.')
                print('------------------------------------------------')
                start_time = time.time()

            # for gender_conf_pixel_thresh in np.arange(start, stop, step):
            # Run the evaluation
            print('Testing with the following threshold values:')
            print(f'gender_conf_pixel_thresh: {gender_conf_pixel_thresh}')
            print(f'worm_conf_image_thresh: {worm_conf_image_thresh}')
            # print(f'gender_conf_pixel_thresh: {gender_conf_pixel_thresh}')
            eval_results = test_with_thresh({
                'gender_conf_pixel': gender_conf_pixel_thresh,
                'worm_conf_image': worm_conf_image_thresh,
            })
            # Update the current best threshold values
            percent_correct = eval_results['overall']
            if percent_correct > max_overall_correct:
                max_overall_correct = percent_correct
                thresholds_best['gender_conf_pixel'] = gender_conf_pixel_thresh
                thresholds_best['worm_conf_image'] = worm_conf_image_thresh
            # Store the data point in the correctness array for plotting
            correctness[row, col] = round(percent_correct, 2)

            col += 1
        row += 1

    # Plot the percent overall correct against the threshold values
    ax = sns.heatmap(correctness, linewidth=0.5, vmin=0.0, vmax=1.0,
                     cmap='coolwarm', annot=True,
                     yticklabels=gender_conf_pixel_levels,
                     xticklabels=worm_conf_image_levels,
                     cbar_kws={'label': 'Correctness',
                               'orientation': 'horizontal'})
    plt.title('Overall Correctness vs. Threshold Values')
    plt.xlabel('Worm Conf Image Thresh')
    plt.ylabel('Gender Conf Pixel Thresh')
    plt.show()

    return thresholds_best


if __name__ == "__main__":
    # Select folder containing the images to test on
    root = tk.Tk()
    root.withdraw()
    p_parent = tk.filedialog.askdirectory(parent=root,
                                          title='Please select the PARENT folder DatasetImage AND DatasetMasks subfolders')

    p_img = os.path.join(p_parent, "DatasetImages")
    p_mask = os.path.join(p_parent, "DatasetMasks")

    img_paths, img_names = verifyDirlist(p_img, False, ".png")
    manual_mask_paths, manual_mask_names = verifyDirlist(p_mask, False, ".png")

    if with_file:
        p_ai_gender = tk.filedialog.askopenfilename(parent=root,
                                                    title='Please select the .txt file containing AI gender info')
        print('Testing with file:', p_ai_gender)
        print('------------------------------------------------')
        test_with_file(p_ai_gender)
    else:
        # Create a socket connection with the server
        print('Connecting to server...')
        HOST = '127.0.0.1'
        PORT = 10000
        s = socket.socket()
        s.connect((HOST, PORT))
        print("Connected to server.")
        print('------------------------------------------------')

        # print(f'Best combination of thresholds: {find_best_thresh()}')
        eval_results = test_with_thresh({})
