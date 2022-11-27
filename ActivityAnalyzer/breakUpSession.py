
# Anthony's Includes
from verifyDirlist import verifyDirlist
from setImageOutpathsBySession import setImageOutpathsBySession

# Standard Includes
import os
import tkinter.messagebox
from tkinter import *



def breakUpSession(pathname):

    # Set path to reorganize
    dayfolders = verifyDirlist(pathname,True,"20","")[0]

    if len(dayfolders) is 0 :
        print("SKIPPED SESSION:\n\t",pathname,"\n\tBecause it does not contain day folders")

    # Keep a list of folders that could not be deleted
    not_deleted = list()

    # For each folder, change the name
    for day in dayfolders:

        # Find images and corresponding ROI coordinates in the folder
        imagenames = verifyDirlist(day,False,".png","ROI")
        csvfullnames, csvpartnames = verifyDirlist(day,False,"ROI_coordinates_",".mat")

        # Skip folder if it has no images - not necessary, it just automatically skips
        #if (not imagenames[1]):
        #    continue


        # Break up the images in this folder into their output folders
        thresh_min = 1 # A gap of 1 minute between images signifies a new session.
        fullnames = imagenames[0]
        newfullnames, outpath = setImageOutpathsBySession(fullnames,thresh_min) 
        folder_already_valid = False

        # Move each image to its new folder
        for oldname,newname in zip(fullnames,newfullnames) :

            # Make the output path if it doesn't exist
            (newpath,trash) = os.path.split(newname)
            if not os.path.isdir(newpath) :
                os.mkdir(newpath)

            # Keep track of whether the folder was already done or not
            folder_already_valid = oldname == newname
            if folder_already_valid: 
                break

            # Perform the move (rename) operation unless it's already been done
            os.rename(oldname,newname)



        # Move any NEW-STYLE roi coordinates to the new folder
        for (csvfullname,csvpartname) in zip(csvfullnames,csvpartnames) :
            newname = os.path.join(outpath,csvpartname)
            os.rename(csvfullname,newname)

        # Delete ROI images or .mat files from the old folder, likely calculated using the old Matlab codes
        # DO NOT Delete if this is an already valid folder, not in need of reorganization!!
        if not folder_already_valid:
            roinames = verifyDirlist(day,False,"ROI","")[0]
            for name in roinames:
                os.remove(name)

            matnames = verifyDirlist(day,False,".mat","")[0]
            for name in matnames:
                os.remove(name)

        # Verify that the original day folder is empty, excluding Roi stuff (old or new format), which can be deleted 
        nonames = verifyDirlist(day,False,"","")
        nodirs = verifyDirlist(day,True,"","")
        if len(nonames[0]) == 0 and len(nodirs[0]) == 0 :
            os.rmdir(day)

        # Handle case where it is not empty because this folder was already formatted
        elif folder_already_valid: 
            BBBB=1
            #no need to print anything if folder is all good.
            #print("Information:\n\t", day, "\n\tis already in the correct format")

        else : 
            print("WARNING: DID NOT DELETE:\n\t", day, "\n\tBecause it contains has some files or folders")
            not_deleted.append(day)


    # Any not deleted folders? Show a warning
    #if len(not_deleted) > 0 :
    #    msg = "Reorganization completed. Some folders were not deleted because they contain unrecognized files or folders.\n"
    #    msg = "Note: Plate folders should ONLY contain session folders, like *2019-01-01_(01-01-01)*\n"
    #    msg += "You should move or delete other folders manually.\n" 
    #    for (i,name) in enumerate(not_deleted) :
    #        msg += "\n" + name
    #        if i > 4:
    #            break
    #    msg += "\n\nAnd possibly some others..."
    #    tkinter.messagebox.showwarning("Warning",msg)


    return