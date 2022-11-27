

import cv2
import cv2.dnn
import os
import matplotlib.pyplot as plt
import matplotlib.patches as patches

import numpy as np
import tkinter
import time
import scipy.interpolate
from scipy.signal import find_peaks

# Load wormotel image
pimg = r'C:\Dropbox\WormWatcher (shared data)\Wormotel simulations\SampleImages'
fimg = '2016-10-12 (12-30-35).png'
fsw = 'WellStandard.png'

img = cv2.imread(os.path.join(pimg,fimg),cv2.IMREAD_GRAYSCALE)
row = np.zeros(shape=(0,0))
STD = row


""" Helper function: Generate grid of wells of known size, and compute mean value of 1 pixels """

def meanValueUnderGrids(img,Nrows,Ncols,well_size,r_off,c_off):

# USAGE CODE:

#scan_range = 100
#scan_jump = 2
#energy = np.zeros(shape=(int(scan_range/scan_jump),int(scan_range/scan_jump)))

#for r_off in range(0,scan_range,scan_jump):
#    for c_off in range(0,scan_range,scan_jump):
#        (gridmean, MSEave, MSEmax)= meanValueUnderGrids(img,Nrows,Ncols,well_size,r_off,c_off)
#        energy[int(r_off/scan_jump),int(c_off/scan_jump)]  = MSEmax



    # "frame" the image in gray pixels so that out-of-bounds wells are allowed but will be penalized.
    R,C = img.shape
    Rf = int(R*1.2)
    Cf = int(C*1.2)
    imgf = np.ones(shape=(Rf,Cf)) - 100

    # Keep track of the additional offset now added to the image
    c_frame = int(C*0.1)
    r_frame = int(R*0.1)

    # Add the image to the frame
    imgf[r_frame:r_frame+R,c_frame:c_frame+C] = img
    

    # Generate a grid of wells (position might be off but size should be right)
    gridimg = np.zeros(imgf.shape)
    colvals = range(c_off+c_frame,Ncols*well_size+c_off+c_frame,well_size)
    rowvals = range(r_off+r_frame,Nrows*well_size+r_off+r_frame,well_size)
    MSE = np.zeros((len(rowvals)*len(colvals),))
    idx=-1

    for c0 in colvals:
        for r0 in rowvals:

            # Get the bottom right corner of this grid
            r1 =r0+well_size
            c1 =c0+well_size

            # Skip if out of bounds, resulting in a bad structure. Penalize heavily
            if r1 >= Rf or c1 >= Cf:
                MSE[idx] = 9999
                continue

            # Drawa box to represent the grid
            gridimg[r0:r1,c0:c1] = 1
            gridimg[r0+1:r1-1,c0+1:c1-1] = 0

            # compute the x-symmetry of this grid - mirror the right half over the left and compute MSE
            cmid = int(well_size/2)
            v = np.mean(imgf[r0:r1,c0:c1],axis=0)
            rhs = v[cmid:]
            mirror = np.flip(rhs)
            lhs = v[:cmid]

            # Add the MSE of symmetry to the list
            idx+=1
            MSE[idx] = np.mean((lhs-mirror)**2)

            if False:
                plt.plot(v,'-k',linewidth=3)
                plt.plot(mirror,'-r')
                plt.show()


    # Get the mean value of the pixels under the 1's
    imgtest = imgf * gridimg

    imgtest[gridimg==0] = np.nan
    gridmean = np.nanmean(imgtest)

    # Get the overall average MSE and max MSE
    MSEave = np.mean(MSE)
    MSEmax = np.max(MSE)


    # Display debug image
    if True:
        imgf = imgf.astype(np.uint8)
        gridimg = gridimg.astype(np.uint8)*255

        imgf = cv2.cvtColor(imgf,cv2.COLOR_GRAY2RGB)
        gridimg = cv2.cvtColor(gridimg,cv2.COLOR_GRAY2RGB)
        gridimg[:,:,0:2] = 0


        imgshow = cv2.addWeighted(imgf,0.5,gridimg,0.5,0.0)
        imgshow = imgshow.astype(np.uint8)

        cv2.putText(imgshow,str(round(gridmean,1)),(50,50),cv2.FONT_HERSHEY_SIMPLEX,2,(0,255,0),thickness=1)
        cv2.namedWindow('Debug',cv2.WINDOW_NORMAL)
        cv2.imshow('Debug',imgshow)
        cv2.waitKey(0)

    return (gridmean, MSEave, MSEmax)



""" STEP 1: Figure out spacing by using arbitrary kernel near center of image """

# Preferences
Nrows = 12
Ncols = 20

# Resize the image (downsample 4x) to improve speed
dsfactor = 0.25
img = cv2.resize(img,(0,0),fx=dsfactor,fy=dsfactor)


# Get a crop of the center of the image, which should have the least distortions
R,C = img.shape
r0 = int(R * 0.45)
r1 = int(R * 0.66)
c0 = int(C * 0.45)
c1 = int(C * 0.65)
crop_img = img[r0:r1,c0:c1]    

# Scan the kernel horizontally to look for periodic correlation
scan_radius_rel = 0.2
scan_radius = int(scan_radius_rel * C)
kernel_size = int(crop_img.shape[1])

rctr = int(R*0.5)
cctr = int(C*0.5)

testcols = np.arange(cctr-scan_radius,cctr+scan_radius)
MSE= np.zeros(shape=testcols.shape)
idx=-1

for c in testcols:

    # Get local crop kernel of the same size
    c0 = c
    c1 = c+kernel_size
    this_crop = img[r0:r1,c0:c1]  

    # Get its MSE
    idx+=1
    MSE[idx] =np.mean( (crop_img - this_crop)**2)


# Find prominent peaks
MSE = -MSE
peaks, _ = find_peaks(MSE,prominence = (4,None))

# Require at least 3 valid peaks to be found
if len(peaks) < 3:
    print('Too few peaks found to correlate wormotel well positions')
    

# Find the median distance between prominent peaks. This is the (downsampled) well size
well_size = int(np.median(np.diff(peaks)))


# Load and resize the standard well to match the known well size

ws = int(0.9*well_size)
swell = cv2.imread(os.path.join(pimg,fsw),cv2.IMREAD_GRAYSCALE)
swell = cv2.resize(swell,(ws,ws))


# Scan it across the entire image and compute MSE
filtered = np.ones(img.shape)*40000
for r in range(well_size,R-well_size):
    for c in range(well_size,C-well_size):

        crop_img = cv2.normalize(img[r:r+ws,c:c+ws],0,255)
        MSE = np.mean(crop_img - swell)**2
        filtered[r,c] = MSE


plt.figure(1)
ax=plt.subplot(1,2,1)
plt.imshow(img,cmap='gray')
rect = patches.Rectangle((cctr,rctr),well_size,well_size,linewidth=1,edgecolor='r',facecolor='none')
ax.add_patch(rect)

plt.subplot(1,2,2,sharex=ax,sharey=ax)
plt.imshow(filtered,cmap = 'jet')


plt.show()



""" Canal tracing, easy to slip up """

## Canal tracing, start in image center and move to the right
#row = int(img.shape[0]/2)+75
#col = int(img.shape[1]/2)
#points = []
#jump = 5

#while col < img.shape[1] - jump - 1:

#    # Add current point to list
#    points.append([row,col])

#    # Select next point with lowest
#    nextcol = col+jump
#    rowopts = range(row-2,min(row+2+1,img.shape[0]-1))
#    rowvals = img[rowopts,nextcol]
#    imin = np.argmin(rowvals)

#    # Assign this as the next point
#    row = rowopts[imin]
#    col = nextcol

## Convert results to numpy array
#points = np.asarray(points)

## Plot results
#plt.figure(1)
#plt.imshow(img,cmap = 'gray')
#plt.plot(points[:,1],points[:,0],'-g')
#plt.show()
    






""" Edge finding, doesn't work so well """

#imgx,imgy = cv2.spatialGradient(img)
##imgx[imgx<0] = 0
##imgy[imgy<0] = 0
#img = imgx + imgy

## Get a crop of the center of the image, which should have the least distortions
#R,C = img.shape
#r0 = int(R * 0.33)
#r1 = int(R * 0.66)
#c0 = int(C * 0.33)
#c1 = int(C * 0.66)
#crop_img = img[r0:r1,c0:c1]

#Xavg = np.mean(crop_img,axis = 1)
#Yavg = np.mean(crop_img,axis=0)

#plt.figure(1)
#plt.subplot(2,1,1)
#plt.plot(Yavg)
#plt.subplot(2,1,2)
#plt.plot(Xavg)


##plt.figure(2)
##plt.subplot(1,2,1)
##plt.imshow(img)
##plt.subplot(1,2,2)
##plt.imshow(imgy)
#plt.show()
