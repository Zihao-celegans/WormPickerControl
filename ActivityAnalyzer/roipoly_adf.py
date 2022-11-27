
import cv2
import numpy as np
import matplotlib.pyplot as plt
from mask_rcnn_utils.utils import get_labeling_params, get_gender_labeling_params


# Mouse callback
global xclick, yclick

def onMouse(event, x, y, flags, params):


    global xclick, yclick

    if event == cv2.EVENT_LBUTTONDOWN:
        xclick = x
        yclick = y


    return



def drawUserContour(img_color, class_params={}):


    global xclick, yclick
    pts = []

    cv2.namedWindow("Preview",cv2.WINDOW_NORMAL)
    cv2.setMouseCallback("Preview", onMouse) 
    xclick = -1
    yclick = -1
    done_flag = False

    while True:
        # Must multiply by scalar/mask/etc to force DEEP COPY
        img = img_color*1

        # Copy the points list
        pts_closed = list()
        for pt in pts:
            pts_closed.append(pt)

        # Draw all points on image. MUST DEEP COPY LIST!!
        if len(pts) >= 3:
            pts_closed.append(pts[0])

        if len(pts) == 1:
            img = cv2.circle(img,tuple(pts[0]),3,(0,0,255))
        for i in range(len(pts_closed) - 1):
            img = cv2.line(img,tuple(pts_closed[i]),tuple(pts_closed[i+1]),(0,0,255),2)

        # Check if any points were clicked
        if (xclick>=0) and (yclick >=0):
            pts.append([xclick,yclick])
            xclick = -1
            yclick = -1

        cv2.imshow("Preview",img)
        k = cv2.waitKey(50)

        user_label = -1
        remove_contour = False

        # Remove last point - backspace
        if (k==8) and (len(pts) > 0):
            pts.pop()

        # Add another contour - space
        elif (k == 32) and ((len(pts) >= 4) or (len(pts) == 0)):
            user_label = getLabelFromUser(class_params)
            break

        elif (k==32) and (len(pts) > 0):
            print('Minimum of 4 points required before continuing. Either remove all or add more')


        # Exit - esc
        #elif (k==27) and ((len(pts) >= 4) or (len(pts) == 0)):
        #    done_flag = True
        #    break
        elif (k==27) and len(pts) >= 4:
            print("Can't finalize image with an unfinished mask present:\nPress SPACE to finalize the contour first or press BACKSPACE to delete the points of the unfinished mask.")
        elif (k==27) and len(pts) == 0:
            done_flag = True
            break

        elif (k==27 and (len(pts) > 0)):
            print('Minimum of 4 points required before continuing. Either remove all or add more')

        elif (k==120):
            if len(pts) > 0:
                print("Please use BACKSPACE to remove any points from the unfinished mask before removing the most recent contour.")
            else:
                remove_contour = True
                break

    return pts, done_flag, user_label, remove_contour


def annotateImage(fimg, extension, desired_extension = None, mask_rcnn_format = False, gender_labels = False):
    # Load image
    img_color = cv2.imread(fimg,cv2.IMREAD_COLOR)
    img_mask = np.zeros(shape=img_color.shape)

    #class_params = {0 : ["L1-Larva",1,51,0], 1: ["L2-Larva",52,102,0], 2 : ["L3-Larva",103,153,0], 3 : ["L4-Herm",154,204,0], 4 : ["L4-Male",205,255,0], 5 : ["Adult-Herm",1,51,1], 6 : ["Adult-Male",52,102,1], 7: ["Unkown-Stage", 205, 255, 2]} # Class ID : (Class Name, Start Value, Max Value, Channel)
    if gender_labels:
        class_params = get_gender_labeling_params()
    else:
        class_params = get_labeling_params()
    labels = []

    # Add contour
    contours = list()
    while True:
        pts, done_flag, user_label, remove_contour = drawUserContour(img_color, class_params)
        #print(f"user label = {user_label}")

        if remove_contour:
            if len(contours) > 0:
                contours.pop()
            if len(labels) > 0:
                labels.pop() 

            img_color = cv2.imread(fimg,cv2.IMREAD_COLOR)

        # Are we done?
        if done_flag:
            break

        # If a contour was selected...
        if len(pts) >= 3:

            # Convert the points to a numpy array for use in contours
            this_contour = np.zeros(shape=(len(pts),2))
            for i in range(len(pts)):
                this_contour[i,0] = pts[i][0]
                this_contour[i,1] = pts[i][1]

            # Add the contour points to contours
            contours.append(this_contour.astype(int))

        # Draw filled ROIs
        img_final = cv2.drawContours(img_color,contours,-1,(0,0,255),-1) # display
        img_mask = np.zeros(shape=img_color.shape)

        if(mask_rcnn_format):
            # Get the most recent label from the user
            #labels.append(getLabelFromUser(class_params))
            if user_label != -1:
                labels.append(user_label)
            class_pixel_values = [] # Tracks the current pixel value for each class's contour. The index in the list + 1 corresponds to that entry's class ID. (Class ID 1 is at index 0)

            #Get the starting pixel value for each class
            for id in class_params:
                class_pixel_values.append(class_params[id][1])

            print(f'Labels: {labels}')
            for i in range(len(contours)):
                pixel_value = class_pixel_values[labels[i] - 1]
                print(f"drawing contour {i}, label: {labels[i]}, value: {pixel_value}")

                if pixel_value > class_params[labels[i]][2]:
                    print(f"Too many instances of class {labels[i]} ({class_params[labels[i]][0]}). Max allowable instances: {class_params[labels[i]][2] - class_params[labels[i]][1] + 1}")
                    print("This instance will be skipped, and will not be in the final mask.")
                    print(f"You may conitnue to label other classes in the image if you'd like, but no further {class_params[labels[i]][0]} instances can be masked.")
                    continue

                channel_multipliers = [0,0,0]
                channel_multipliers[class_params[labels[i]][3]] = 1

                # Draw the mask corresponding to the current class, with that class's appropriate pixel value based on the number of instances of that class already drawn.
                #img_mask =  cv2.drawContours(img_mask,contours,i,(i+1 ,i+1 ,i+1),-1) # data
                img_mask =  cv2.drawContours(img_mask,contours,i,(pixel_value * channel_multipliers[0] ,pixel_value * channel_multipliers[1] ,pixel_value * channel_multipliers[2]),-1) # data
                # Draw filled ROIs
                img_final = cv2.drawContours(img_color,contours,i,(pixel_value * channel_multipliers[2] ,pixel_value * channel_multipliers[1] ,pixel_value * channel_multipliers[0]),-1) # display (swap channels 1 and 3 because opencv is BGR but we want to display in RGB -- the end mask is saved in RGB.)
                class_pixel_values[labels[i] - 1] += 1 # Increment the pixel value for the class that was just drawn.
        else:
            img_mask =  cv2.drawContours(img_mask,contours,-1,(255,255,255),-1) # data   
            # Draw filled ROIs
            img_final = cv2.drawContours(img_color,contours,-1,(0,0,255),-1) # display

    # Save mask
    if not desired_extension:
        desired_extension = '_contmask.png'

    splitpath = fimg.split("\\")
    fout = fimg.replace( extension, desired_extension)
    if mask_rcnn_format:
        print("\nSaving mask image in Mask RCNN format.")
        img_mask = cv2.cvtColor(img_mask.astype('uint8'), cv2.COLOR_BGR2RGB)
    else:
        print("\nSaving mask image in the normal mask format. (Black background with white masks) - NOT compatible with Mask RCNN training.")

    print(f"Saving mask: {fimg}")
    cv2.imwrite(fout,img_mask)



def getLabelFromUser(class_params):
    if len(class_params) == 0:
        return

    while True:
        print("Please provide a label for the object you just masked.\nEnter:")
        for id in class_params:
            print(f"\t{id} for {class_params[id][0]}")
        class_id = ''
        try:
            if len(class_params) < 10:
                class_id = cv2.waitKey(0)
                class_id -= 48
                #print(f"waitkey entered: {class_id}, type: {type(class_id)}")
            else:
                print('Please enter your label here in the command prompt.')
                class_id = int(input())
        except Exception:
            pass

        if class_id not in class_params:
            print('Invalid number entered. Please enter a number from the class list provided.')
        else:
            print(f'{class_params[class_id][0]} has been selected.')
            return class_id

