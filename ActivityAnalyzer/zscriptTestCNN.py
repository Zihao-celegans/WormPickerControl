
"""
    Python-OpenCV test of the ONNX exported Matlab network
    Could not get it to make correct predictions
"""
# Standard Includes
import os
import numpy as np
import time
import sys

# Matplotlib+
import matplotlib.pyplot as plt
import cv2
import cv2.dnn
import onnx


# Load the ONNX
fonnx = "C:\\Dropbox\\WormWatcher (shared data)\\Well score training\\CNN_worm_bg_v9a_norm28.onnx";
net = cv2.dnn.readNetFromONNX(fonnx);

# Check the ONNX https://answers.opencv.org/question/205166/how-can-i-load-a-onnx-with-opencv/
onnx_model = onnx.load( fonnx)
result = onnx.checker.check_model(onnx_model)

# Load 2 worm images and 2 background images
images = list()
images.append(cv2.imread("C:\\train\\worm\\C2_2D_1_2019-03-27 (10-15-05)_ROI=1_worm_0029.png", cv2.IMREAD_COLOR));
images.append(cv2.imread("C:\\train\\worm\\C2_2D_1_2019-03-27 (10-15-05)_ROI=1_worm_0070.png", cv2.IMREAD_COLOR));
images.append(cv2.imread("C:\\train\\background\\C2_2D_1_2019-03-27 (10-15-05)_ROI=1_bkg_0000.png", cv2.IMREAD_COLOR));
images.append(cv2.imread("C:\\train\\background\\C2_2D_1_2019-03-27 (10-15-05)_ROI=1_bkg_0008.png", cv2.IMREAD_COLOR));

im4d = np.zeros(shape=(20,20,3,4)).astype(np.uint8)

for (img,i) in zip(images,range(len(images))):

    # Normalize the image
    #img = img - np.min(img)
    #img = img.astype(np.float)
    #img = img/np.max(img)
    #img = img*255
    #img = img.astype(np.uint8)

    im4d[:,:,:,i] = img

blob = cv2.dnn.blobFromImages(images,1,size=(20,20))
net.setInput(blob)
result = net.forward()
print(result)


