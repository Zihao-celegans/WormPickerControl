"""

    CNN_LabelWormMotion.py

    Requires a folder of images exported by CNN_exportRandomLoc3frames

    Adapted from WM_ls_annotate_bywell.py

"""


from verifyDirlist import verifyDirlist
import csvio
import cv2
import numpy as np
import os
from tkinter import filedialog
import tkinter as tk
from scipy.signal import medfilt2d, correlate2d



""" HELPER FUNCTIONS """


def getWormScore(crops):

    # Flash image back and forth, get user input. 
    cv2.namedWindow("Preview",cv2.WINDOW_NORMAL)
    decision = np.nan
    iter = 0
    decrement_session_flag = False

    while(True):

        # Select one of the images
        if iter >= len(crops):
            iter=0
        display = crops[iter]
        display = cv2.cvtColor(display,cv2.COLOR_GRAY2BGR)
       
        # Display decision results
        if decision == 0:
            display = cv2.rectangle(display,(0,0),(display.shape[0]-1,display.shape[1]-1),(0,255,0),3)

        elif decision == 3:
            display = cv2.rectangle(display,(0,0),(display.shape[0]-1,display.shape[1]-1),(0,255,255),3)

        elif not np.isnan(decision):
            display = cv2.rectangle(display,(0,0),(display.shape[0]-1,display.shape[1]-1),(0,0,255),3)


        # Add targeting crosshair
        cv2.circle(display,(int(display.shape[0]*0.5),int(display.shape[1]*0.5)),12,(0,128,128),1)

        cv2.imshow("Preview",display)
        key = cv2.waitKey(250)

        # If a decision was already made, then show the image color and return
        if not np.isnan(decision):
            cv2.waitKey(250)
            break


        if key == 97:
            print('Alive')
            decision = 0

        elif key == 100:
            print('Dead')
            decision = 1

        elif key == 109:
            print("Missing/Background")
            decision = 2

        elif key == 27:
            exit(0)

        elif key == 8:
            decision = 3 # Go back

        else:
            iter+=1


    return decision



# Set parameters
pdata = r'Z:\UnsortedColor01'
classes = ["alive","dead","missing"] # order must match classes in user decision funciton

# confirm exported data folder to sort
root = tk.Tk()
root.withdraw()
pdata = filedialog.askdirectory(parent=root, initialdir=pdata, title='Please select folder with images')

# Find output folders and create
trash, setname = os.path.split(pdata)
root = pdata[0:3]
setname_out = setname.replace("Unsorted","Sorted")
pout = os.path.join(root,setname_out)

for this_class in classes:
    if not os.path.exists(os.path.join(pout,this_class)):
        os.makedirs (os.path.join(pout,this_class))



# Find input images
d_full,d_short = verifyDirlist(pdata,False,'.png')
outname_list = [];
idx = -1

while idx < len(d_full):

    # Extract iteration variables
    idx += 1
    fullname = d_full[idx]
    shortname = d_short[idx]

    # Load the image and convert to list
    img = cv2.imread(fullname,cv2.IMREAD_COLOR)
    images = [];
    for i in range(3):
        images.append(img[:,:,i])

    # Show list of images to user to get decision
    decision = getWormScore(images)

    # Go back to last frame if that was the decision
    if decision == 3:

        # Decrement index
        idx -= 1

        # Return the image to the unsorted folder
        os.rename(outname_list[idx],d_full[idx])

        # Clear entry from the outname list
        outname_list.pop()

        # Decrement again to redo this frame next
        idx -= 1

    # Move image to corresponding folder
    else:
        outname = os.path.join(pout,classes[decision],shortname)
        os.rename(fullname,outname)
        #print("Moved: ", outname)
        outname_list.append(outname)

