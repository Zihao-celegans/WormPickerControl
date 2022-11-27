"""
    Ask the user to click on a few wells in the wormotel and then 
    Interpolate the rest
"""

import cv2
import os
import matplotlib.pyplot as plt
import numpy as np
import tkinter
import time
import scipy.interpolate
from verifyDirlist import *

global x,y,cx, cy, dblclick, button, samples_for_x, samples_for_y

def onclick(event,x,y,flags,param):
    
    global cx, cy, dblclick, button
    if event == cv2.EVENT_LBUTTONDOWN:
        print('(x,y) = ','(',x,',',y,')')


        cx = x
        cy = y
        button = 1

    elif event == cv2.EVENT_RBUTTONDOWN:
        button = 3

    return

def displayFig1(img, row, col, x=-1,y=-1):

    global samples_for_x, samples_for_y

    # add image text as black bar at bottom
    imgcolor = cv2.cvtColor(img,cv2.COLOR_GRAY2RGB)
    bar = np.zeros(shape=(120,imgcolor.shape[1],imgcolor.shape[2])).astype(np.uint8)
    imgcolor = np.concatenate((imgcolor,bar),axis=0)

    ytxt = imgcolor.shape[0]-10

    cv2.putText(imgcolor,"Click on (row,col) = (" + str(row) + "," + str(col) + ") - Rt click next",(30,ytxt),cv2.FONT_HERSHEY_PLAIN,5,(0,255,0),3)
    #cv2.putText(imgcolor,"Right click for next",(30,200),cv2.FONT_HERSHEY_PLAIN,10,(0,255,0),3)
    
    # Add all previous well coordinates 
    for  (xx,yy) in zip(samples_for_x,samples_for_y):
        if xx[2] > 0:
            cv2.circle(imgcolor,(int(xx[2]),int(yy[2])),10,(0,0,255),-1)


    # Add well coordinate if any
    if x >= 0:
        cv2.circle(imgcolor,(x,y), 35, (0,255,0),thickness = 2)
        cv2.circle(imgcolor,(x,y), 3, (0,255,0),thickness = -1)
            
    # Show
    ax = cv2.imshow("Figure 1",imgcolor)
    cv2.waitKey(1)

    return


def getCoords(img,row,col):

   
    global x,y, cx, cy, dblclick, button

    # Display the image
    displayFig1(img,row,col)

    # Keep trying to draw the well until right click button 3
    x=-1
    y=-1

    while True:

        # Wait for any valid button, then draw the selected point
        if button == 1:

            # Get coordinate
            x = cx
            y = cy

            # Draw a mark to show the location
            displayFig1(img,row,col,x,y)

            # Reset the click
            cx = -1
            cy = -1
            button = -1

        # Press RIGHT CLICK to finish this well
        if button == 3 and x >= 0:
            break

        elif button == 3:
            print('Error - did not select point before quitting!')
            button = -1

        cv2.waitKey(50)


    # Return the latest coordinates
    return(x,y)



# Load wormotel image
# pimg = r'C:\Dropbox\WormWatcher (shared data)\Wormotel simulations\SampleImages\Training Samples'
pimg = r'C:\Dropbox\WormWatcher (shared data)\Wormotel simulations\SampleImages\Testing samples'

full_names, part_names = verifyDirlist(pimg,False)

for fimg in part_names:

    img = cv2.imread(os.path.join(pimg,fimg),cv2.IMREAD_GRAYSCALE)

    # Connect the mouse to the callback function
    cx = -1
    cy = -1
    dblclick = False
    button = -1

    # Link the image to the event handler
    cv2.namedWindow('Figure 1',cv2.WINDOW_NORMAL)
    cv2.setMouseCallback('Figure 1',onclick)

    # Select the wells that we're going to label
    cols = [1,7,13,20]
    rows = [1,6,12]
    Nrows = 12
    Ncols = 20
    N = len(cols) * len(rows)
    samples_for_x = np.zeros(shape=(N,3))
    samples_for_y = np.zeros(shape=(N,3))
    i=-1

    # Loop through rows and cols and get user to click on coords
    for row in rows:
        for col in cols:
            i+=1
            x, y = getCoords(img,row,col)
            samples_for_x[i,:] = [row,col,x]
            samples_for_y[i,:] = [row,col,y]


    # Interpolate the centers of all remaining wells - Calculate surface fit
    fitx = scipy.interpolate.LinearNDInterpolator(samples_for_x[:,0:2], samples_for_x[:,2], fill_value=np.nan, rescale=False)
    fity = scipy.interpolate.LinearNDInterpolator(samples_for_y[:,0:2], samples_for_y[:,2], fill_value=np.nan, rescale=False)

    # Genrate grid of points and fit them to the surface
    (rgrid,cgrid) = np.meshgrid(np.arange(1,Nrows+1),np.arange(1,Ncols+1))
    xfitpts = fitx(rgrid,cgrid)
    yfitpts = fity(rgrid,cgrid)

    # Show error if any extrapolated points
    if np.any(xfitpts == np.nan,axis=None) :
        tkinter.messagebox.showerror('Not enough data','Your calibration samples do not cover the entire tray. Plate positions cannot be extrapolated. Please add more samples and try again.')

    # Save the coordinates to disk
    fout = os.path.join(pimg,fimg.upper().replace('.PNG','.csv')).lower()
    fout = fout.upper().replace('.BMP','.csv').lower()
    outfile = open(fout, "w")

    with outfile:
        for (r,c,x,y) in zip(rgrid.flatten(),cgrid.flatten(),xfitpts.flatten(),yfitpts.flatten()) : 
            outfile.write(str(r) + ',' + str(c) + ',' + str(int(x)) + ',' + str(int(y)) + '\n')
            

    # Show all wells found
    imgcolor = cv2.cvtColor(img,cv2.COLOR_GRAY2RGB)    

    for (x,y) in zip(xfitpts.flatten(),yfitpts.flatten()):
        cv2.circle(imgcolor,(int(x),int(y)), 30, (0,255,0),thickness=2)
        cv2.circle(imgcolor,(int(x),int(y)),3,(0,255,0),thickness=-1)

    cv2.imshow('Figure 1',imgcolor)
    cv2.waitKey(0)