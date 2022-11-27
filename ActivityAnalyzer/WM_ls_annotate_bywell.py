"""
    WM_ls_annotate.py

    Manually score the lifespan of worms in the wormotel. Requires already analyzed by STEP 1.

    Only works for 240 well wormotels

    ADAPTED FROM WM_ls_annotate. GOES BY WELL INSTEAD OF BY SESSION
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


def getWormScore(crops,buf,r, session_idx):

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
       
        # Format text labels
        if decision == 1:
            display = cv2.rectangle(display,(0,0),(int(2*r*scalefactor+2*buf),int(2*r*scalefactor+2*buf-1)),(0,255,0),3)

        elif not np.isnan(decision):
            display = cv2.rectangle(display,(0,0),(int(2*r*scalefactor+2*buf),int(2*r*scalefactor+2*buf-1)),(0,0,255),3)


        cv2.imshow("Preview",display)
        key = cv2.waitKey(250)

        # If a decision was already made, then show the image color and return
        if not np.isnan(decision):
            cv2.waitKey(250)
            break


        if key == 49:
            print('Alive')
            decision = 1

        elif key == 48:
            print('Dead')
            decision = 0

        elif key == 52:
            print("Missing")
            decision = 4

        elif key == 53:
            print("WELL IS DEFECTIVE, CONTAMINATION ETC")
            decision = 5

        elif key == 54:
            print("BLURRY AMORPHOUS MOTION")
            decision = 6

        elif key == 55:
            print("MULTIPLE WORMS")
            decision = 7

        elif key == 27:
            exit(0)

        elif key == 8 and (session_idx > 0):
            decrement_session_flag = True
            break

        elif key == 8:
            print('Cannot go back paste first session !!')

        else:
            iter+=1


    return (decision, decrement_session_flag)



# Set parameters
pplate = r'E:\Matt eLIfe paper data (merged)\161005_Lifespan'
pick_up_where_left_off = True
jump = 30 # 30 frames * 5s = 150s = 2.5 min
register_crops = False
blur_images = True

if register_crops:
    scalefactor = 6.0 # Needed to correct sub-pixel shifts
else:
    scalefactor = 1.0



# Select plate folder
root = tk.Tk()
root.withdraw()
pplate = filedialog.askdirectory(parent=root, initialdir=pplate, title='Please select the PLATE folder')


# Find output analysis folder
pparent, platename = os.path.split(pplate)
fout = os.path.join(pparent,'_Analysis',platename,'ManualWMLifespan.csv')



# Find sessions
psessions, __ = verifyDirlist(pplate,True,"","")
N_sessions = len(psessions)
N_wells = 240

# Create (or, if we're continuing a previously started plate, load) the data array

print('\n\n\n')

if os.path.exists(fout):
    AllScores = csvio.readArrayFromCsv(fout,"<Manual>")
    print('Loaded already started csv file: ', fout)

else:
    AllScores = np.ones((N_sessions,N_wells))*np.nan
    print('Starting from scratch.')


# Confirm valid loaded
if (AllScores.shape[0] != N_sessions) or (AllScores.shape[1] != N_wells):

    print('Loaded csv DOES NOT MATCH EXPECTED SHAPE - starting over')
    AllScores = np.ones((N_sessions,N_wells))*np.nan


# Pre-load sample images from all sessions at once
images = list()
rois = list()

print('Loading images...')
for (psession,session_idx) in zip(psessions,range(N_sessions)):
    
    # Find the coordinates file
    fcsv, __ = verifyDirlist(psession,False,"ROI_coordinates")

    if len(fcsv) == 0:
        print("No ROI coordinates for session: ", psession)
        N_sessions -= 1
        continue

    # Load the coordinates
    roi = csvio.readArrayFromCsv(fcsv[0],"<ROI Coordinates>")

    # Save the coordinates to the list
    rois.append(roi)

    # Load first and last image from session
    fimg, __ = verifyDirlist(psession,False,".png")

    if len(fimg) < 35:
        print('Not enough images for session: ', psession)
        continue

    mid = int(len(fimg)/2)-5

    current_images = list()
    
    current_images.append(cv2.imread(fimg[mid-jump],cv2.IMREAD_GRAYSCALE))
    current_images.append(cv2.imread(fimg[mid-int(jump/2)],cv2.IMREAD_GRAYSCALE))
    current_images.append(cv2.imread(fimg[mid],cv2.IMREAD_GRAYSCALE))
    current_images.append(cv2.imread(fimg[mid+int(jump/2)],cv2.IMREAD_GRAYSCALE))
    current_images.append(cv2.imread(fimg[mid+jump],cv2.IMREAD_GRAYSCALE))

    # Save the images to the list
    images.append(current_images)

    # Blur images?
    if blur_images:
        for i in range(len(current_images)):
            current_images[i] = cv2.GaussianBlur(current_images[i] , (3,3), 0.25)




# Print out instructions
print("\n--------------- INSTRUCTIONS ---------------\n")
print("SPACE - next well")
print("0 - mark worm as DEAD")
print("1 - mark worm as ALIVE")
print("4 - mark worm as MISSING")
print("5 - mark well as DEFECTIVE / CONTAMINATION")
print("6 - mark BLURRY, AMORPHOUS MOTION")
print("7 - mark MULTIPLE WORMS")
print("BACKSPACE - go back to last worm")
print("s - skip entire session")
print("ESC - exit")
print("\n------------------------------------------\n")


# Start scanning each well every 10 sessions, then narrow down on the death
well_idx = 0
decrement_session_flag = False
dead_idx = np.nan

while well_idx < N_wells:

    session_idx = 0
    sessions_list = [] # Store which sessions we have analyzed
    dead_mode = False

    # Check if this well is already done... must have 10+ "dead/missing/bad" readings
    dead_readings = (AllScores == 0) | (AllScores > 1)
    N_dead_readings = np.sum(dead_readings,axis=0)

    # Additional check for already done - last reading is near end
    HasReading = ~np.isnan(AllScores).astype(np.uint8)
    last_readings = np.zeros((AllScores.shape[1],))
    for i in range(AllScores.shape[1]):
        well_readings = np.argwhere(HasReading[:,i]==255)
        if len(well_readings) > 0:
            last_readings[i] = well_readings.max()

    # Additional check for already done - look for 10 contiguous zeros
    AllScoresFilt = medfilt2d(HasReading,(9,1)) 
    has5contig0 = np.max((AllScoresFilt==255).astype(np.int),axis=0) > 0 

    # If this session is already analyzed, skip it
    already_done = (N_dead_readings[well_idx] > 13) or (last_readings[well_idx] > (0.70*AllScores.shape[0]))
    already_done = already_done or has5contig0[well_idx]
    
    if pick_up_where_left_off and already_done:
        print("Skipping well ", well_idx, "already done...")
        well_idx += 1
        continue

    while True:

        # Print current status
        print("Session ", session_idx, " of " , N_sessions , " Well index ", well_idx, "... ", end=' ')


        # Get square centers and radius
        buf = 20
        roi = rois[session_idx]
        xc = roi[0,well_idx]
        yc = roi[1,well_idx]
        r = roi[2,well_idx]

        # Get images for this session
        this_series = images[session_idx].copy()

        # Resize images for this session
        if register_crops:
            for i in range(len(this_series)):
                this_series[i] = cv2.resize(this_series[i],(0,0),fx=scalefactor,fy=scalefactor)


        # Convert to color for this session
        for i in range(len(this_series)):
            this_series[i] = cv2.cvtColor(this_series[i],cv2.COLOR_GRAY2BGR)

        crops = list()

        # Convert to force-in-bounds rectangle coords
        x0 = int(max(((xc - r - buf)*scalefactor,0)))
        y0 = int(max(((yc - r - buf)*scalefactor,0)))
        x1 = int(min(((xc + r + buf)*scalefactor,this_series[0].shape[1]-1)))
        y1 = int(min(((yc + r + buf)*scalefactor,this_series[0].shape[0]-1)))

        for (this_frame,i) in zip(this_series,range(len(this_series))):

            # If we need to register (e.g. robot), find the coordinates that most closely match the first image
            dx = 0
            dy = 0
            lowest_diff = 9999
            if register_crops and i > 0:

                img0 = crops[0].astype(np.float)

                for xx in range(-4,5,1):
                    for yy in range(-4,5,1):

                        img1 = this_frame[y0+yy:y1+yy,x0+xx:x1+xx].astype(np.float)

                        diff_img = np.abs(img1 - img0)
                        this_diff = np.mean(diff_img)

                        if this_diff < lowest_diff:
                            lowest_diff = this_diff
                            dx = xx
                            dy = yy

                        # print(dx,' ',dy,' ',this_diff,' ',lowest_diff)
                        #cv2.namedWindow('Test',cv2.WINDOW_NORMAL)
                        #cv2.imshow('Test',diff_img / np.max(diff_img))
                        #cv2.waitKey(0)

            crops.append(np.copy(this_frame[y0+dy:y1+dy,x0+dx:x1+dx]))

        iter = 0
        key = -1

        # Get user's decision
        decrement_session_flag = False
        decision, decrement_session_flag = getWormScore(crops,buf,r,session_idx)

        # Before saving check if we need to go back to the last analyzed session for this worm
        if decrement_session_flag:
            session_idx = sessions_list[-1]
            continue

        # Save results to CSV as we go along
        AllScores[session_idx,well_idx] = decision
        
        with open(fout, "w") as outfile:
            csvio.printArrayToCsv(outfile,"<Manual>",AllScores)
            csvio.printEndToCsv(outfile)

        # Store the analyzed sessions
        sessions_list.append(session_idx)

        # Go to next well...

        # If we've reached the end we're done
        if session_idx == N_sessions - 1:
            break

        # Worm is alive - try 10 sessions ahead OR the last session, whichever is next. Doesn't matter if in dead mode
        elif decision == 1:
            session_idx = min(session_idx+10,N_sessions-1)
            dead_idx = np.nan
            dead_mode = False
            #print('\tdead_idx = ', dead_idx)

        # Worm is dead/missing/etc for the FIRST TIME - Go back and forward 5 sessions to make sure
        elif not dead_mode:
            dead_mode = True
            dead_idx = session_idx
            #print('\tdead_idx = ', dead_idx)
            session_idx = max(0,session_idx - 9)

        # We are in dead mode and the worm is STILL dead
        elif dead_mode and (session_idx < dead_idx+15):
            dead_idx = min(dead_idx,session_idx)
            session_idx += 1
            #print('\tdead_idx = ', dead_idx)
            

        # The worm is STILL DEAD AFTER COMPLETING DEAD MODE
        elif dead_mode:
            #print('\tdead_idx = ', dead_idx)
            break


    # Print result
    print('Worm in well idx ',well_idx, 'died by session #', dead_idx,'\n\n')
    well_idx += 1
        