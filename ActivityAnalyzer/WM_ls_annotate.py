"""
    WM_ls_annotate.py

    Manually score the lifespan of worms in the wormotel. Requires already analyzed by STEP 1.

    Only works for 240 well wormotels
"""

from verifyDirlist import verifyDirlist
import csvio
import cv2
import numpy as np
import os
from tkinter import filedialog
import tkinter as tk


# Set parameters
pplate = r'E:\Matt eLIfe paper data (merged)\161005_Lifespan'
pick_up_where_left_off = True

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







# Analyze each session
for (psession,session_idx) in zip(psessions,range(N_sessions)):

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


    # Find the coordinates file
    fcsv, __ = verifyDirlist(psession,False,"ROI_coordinates")

    if len(fcsv) == 0:
        print("No ROI coordinates for session: ", psession)
        continue

    # Load the coordinates
    roi = csvio.readArrayFromCsv(fcsv[0],"<ROI Coordinates>")

    # Load first and last image from session
    fimg, __ = verifyDirlist(psession,False,".png")

    if len(fimg) < 35:
        print('Not enough images for session: ', psession)
        continue

    mid = int(len(fimg)/2)+5
    jump = 12
    img0 = cv2.imread(fimg[mid],cv2.IMREAD_COLOR)
    img1 = cv2.imread(fimg[mid+jump],cv2.IMREAD_COLOR)


    # Examine cropped images
    well_idx = 0
    decrement_well_flag = False

    while well_idx < N_wells:

        # If this session is already analyzed, skip it
        if pick_up_where_left_off and not np.isnan(AllScores[session_idx,well_idx]) and not decrement_well_flag:
            well_idx += 1
            continue

        # Get square centers and radius
        buf = 20
        xc = roi[0,well_idx]
        yc = roi[1,well_idx]
        r = roi[2,well_idx]

        # Convert to force-in-bounds rectangle coords
        x0 = max((xc - r - buf,0))
        y0 = max((yc - r - buf,0))
        x1 = min((xc + r + buf,img0.shape[1]-1))
        y1 = min((yc + r + buf,img0.shape[0]-1))

        crop0 = np.copy(img0[y0:y1,x0:x1])
        crop1 = np.copy(img1[y0:y1,x0:x1])

        iter = 0
        key = -1

        # Display first cropped image
        decrement_well_flag = False
        skip_session_flag = False
        decision = np.nan
        cv2.namedWindow("Preview",cv2.WINDOW_NORMAL)


        while(True):
            if iter%2 == 0:
                display = crop0
            else:
                display = crop1

            # Format text labels
            if decision == 1:
                display = cv2.rectangle(display,(0,0),(2*r+2*buf,2*r+2*buf-1),(0,255,0),3)

            elif not np.isnan(decision):
                display = cv2.rectangle(display,(0,0),(2*r+2*buf,2*r+2*buf-1),(0,0,255),3)




            cv2.imshow("Preview",display)
            key = cv2.waitKey(250)

            # If a decision was already made, then show the image color and skip to next well
            if not np.isnan(decision):
                cv2.waitKey(250)
                break


            if key == 49:
                print("Session ", session_idx, " of " , N_sessions , " Well index ", well_idx, ": ",'Alive')
                decision = 1

            elif key == 48:
                print("Session ", session_idx, " of " , N_sessions ," Well index ", well_idx, ": ", 'Dead')
                decision = 0

            elif key == 52:
                print("Session ", session_idx, " of " , N_sessions , " Well index ", well_idx, ": ","Missing")
                decision = 4


            elif key == 53:
                print("Session ", session_idx, " of " , N_sessions , " Well index ", well_idx, ": ","WELL IS DEFECTIVE, CONTAMINATION ETC")
                decision = 5

            elif key == 54:
                print("Session ", session_idx, " of " , N_sessions , " Well index ", well_idx, ": ","BLURRY AMORPHOUS MOTION")
                decision = 6

            elif key == 55:
                print("Session ", session_idx, " of " , N_sessions , " Well index ", well_idx, ": ","MULTIPLE WORMS")
                decision = 7

            elif key == 115:
                print("Skipping to next session...")
                skip_session_flag = True
                break

            elif key == 27:
                exit(0)

            elif key == 8 and (well_idx > 0):
                decrement_well_flag = True
                
                break

            elif key == 8:
                print('Cannot go back paste ROI 0 !!')

            else:
                iter+=1


        # BEFORE SAVING, check if we're actually supposed to skip to the next session
        if skip_session_flag:
            break

        # Save results to CSV as we go along
        AllScores[session_idx,well_idx] = decision
        
        with open(fout, "w") as outfile:
            csvio.printArrayToCsv(outfile,"<Manual>",AllScores)
            csvio.printEndToCsv(outfile)

        # Go to next (or last) well
        if not decrement_well_flag:
            well_idx += 1
        else:
            well_idx -= 1
            print("Back to well index# ", well_idx)




