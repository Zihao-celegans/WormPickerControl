import cv2
import numpy as np
import matplotlib.pyplot as plt



# Create white images in the correct aspect ratio
m = 5
W = 135*m
H = 180*m
img = np.ones(shape=(H,W))*255


# Draw in black borders
edge_rad = 0.15*W
thickness = 2*edge_rad


# Blur to get gradient
ksize = 99
sigma = 55
img = cv2.rectangle(img,(0,0),(W-1,H-1),(0,0,0),int(thickness))
img = cv2.GaussianBlur(img,(ksize,ksize),sigma)
img = cv2.normalize(img,img,0,255,cv2.NORM_MINMAX)

cv2.imwrite("C:\Dropbox\Tau Scientific Instruments\Designs\Static Rig\Red Gradient files\outimg.png",img)

plt.imshow(img)
plt.show()
