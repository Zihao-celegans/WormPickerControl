"""
    ReformatParentDirectory.py
    Anthony Fouad
    July 2019

    Reformat a WormWatcher parent directory by breaking up sessions into individual session folders instead of 
    individual day folders
"""

# Anthony's Includes
from verifyDirlist import verifyDirlist
from breakUpSession import breakUpSession


# Standard Includes
import os
import time
import sys
import tkinter.messagebox
from tkinter import *

def reformatParentDirectory(pparent):

    # Find all the plate folders in the parent directory
    platefullfolders = verifyDirlist(pparent,True,"","Analysis")[0]

    # Alert user
    msg = "Before analysis, your data must be organized in WormWatcher 2.0 format.\n\n"
    msg += "If your data was acquired using WormWatcher 1.85 or earlier this could take some time, and certain Matlab files may be deleted.\n\n"
    msg += "It is suggested to BACK UP your data before continuing.\n\n"
    msg += "Do you want to auto-format your data?.\n\n"
    answer = tkinter.messagebox.askyesno("Data format check",msg)

    # Signal to abort (Cancel)
    if answer == None:
        return False

    # Signal to skip
    elif not answer:
        return True

    # Apply the session breakup method to each
    for platefullfolder in platefullfolders :
        breakUpSession(platefullfolder)

    print("Completed reorganization.")
    return True


def verifyValidParentDirectory(pparent,Err):


    # Find all the plate folders in the parent directory
    platefullfolders = verifyDirlist(pparent,True,"","Analysis")[0]

    # Count how many of them contain day folders which contain images, confirming that they are plate folders
    is_good = False

    try:

        for platefullfolder in platefullfolders :

            # Load all possible day folders, skip if none
            dayfolders = verifyDirlist(platefullfolder,True,"20","")[0]
            if len(dayfolders)==0: 
                continue

            # For each day folder, make sure it contains at least 3 PNG images. If at least 1 day folder is good this is valid parent
            for dayfolder in dayfolders:
                filespng =  verifyDirlist(dayfolder,False,"png","")[0]
                files20 = verifyDirlist(dayfolder,False,"20","")[0]
                files = set(filespng).intersection(files20)

                if len(files) > 3 :
                    is_good = True
                    break

            # Continue breaking if we found one good subfolder
            if is_good :
                break

    except:
        is_good = False

    # Check whether the parent folder has an _Analysis folder
    analysisfolders = verifyDirlist(pparent,True,"_Analysis")[0]
    has_analysis = len(analysisfolders) > 0


    # report results to user
    if not is_good and not has_analysis:
        msg =  pparent + '\nIs not a usable parent directory. Please select another folder.'
        tkinter.messagebox.showerror('Invalid folder selected',msg)
        Err.notify("PrintRed",msg)
    
    elif not is_good and has_analysis:
        msg = pparent  + '\nHas analysis data but no raw image data. Certain analyses that require raw images may be unusable.'
        #tkinter.messagebox.showerror('No raw image data',msg)
        Err.notify("PrintRed",msg)

    else:
        msg = pparent  + '\nSuccessfully set as parent directory!'
        Err.notify("Print",msg)

    return is_good or has_analysis
