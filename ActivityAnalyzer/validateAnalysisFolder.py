"""
    validateAnalysisFolder
    Anthony Fouad
    August 2019

    Check that a valid _Analysis folder exists in the parent folder supplied.
    Returns the ..\parent\_Analysis\ folder.

    Does nothing if the _Analysis folder was supplied.

    Causes exception if cannot resolve an _Analysis folder
"""


from verifyDirlist import verifyDirlist
from ErrorNotifier import ErrorNotifier
import os


def validateAnalysisFolder(panalysis, Err, bad_folder_action = "Abort") :

    is_bad = False

    # If this is not a valid _Analysis folder, perhaps it is a parent folder that contains an _Analysis folder
    if not "_Analysis" in panalysis :
        ptemp = verifyDirlist(panalysis,True,"_Analysis")[0]
        if len(ptemp) >= 1 :
            panalysis = ptemp[0]

    # If still not valid, perhaps it's a single plate folder and we need to find the corresponding analysis folder
    if not "_Analysis" in panalysis :
        (path,head) = os.path.split(panalysis)
        ptemp = os.path.join(path,"_Analysis",head)
        if len(verifyDirlist(panalysis,True,"")[0]) > 0 :
            panalysis = ptemp;

    # If still not valid, abort
    if not "_Analysis" in panalysis :
        Err.notify(bad_folder_action,"A Parent or _Analysis folder must be selected.\nBelow folder is invalid:\n" + panalysis)
        is_bad = True

    # Determine whether this is an _Analysis folder or a subfolder (e.g. a specific plate to do just once)
    (path,head) = os.path.split(panalysis)
    is_single_plate = not "_Analysis" in head and len(head) > 0

    # Verify that the path exists
    if not os.path.isdir(panalysis) : 
        Err.notify("Log","The selected folder does not exist:\n" + panalysis)
        is_bad = True

    # Return the PARENT folder
    return (panalysis, is_single_plate, is_bad)


""" Validate that a given plate, specified by any available path/file, has a STEP 2 completed, and return the file name """

def validateActivityCurvesFile(plate_path, Err):

    AC_file = ""
    # If panalysis is a full plate name, proceed with check
    if ".csv" in plate_path:
        AC_file = plate_path

    # If it's a raw image path, convert to _Analysis path
    if not "_Analysis" in plate_path:
        parent,plate = os.path.spliter(plate_path)
        plate_path = os.path.join(parent,"_Analysis",plate)


    # Now we have an analysis path, check that it contains exactly 1 ActivityCurves file and return it
    if not ".csv" in plate_path:
        files = verifyDirlist(plate_path, False, "ActivityCurves",'png')[0]

        # Must be exactly 1 activitycurves file
        if len(files) < 1:
            Err.notify("PrintRed","No STEP 2 (ActivityCurves) file in: " + plate_path)
            return AC_file

        elif len(files) > 1:
            Err.notify("PrintRed","No STEP 2 (ActivityCurves) file in: " + plate_path)
            return AC_file

        else:
            AC_file = files[0]
            return AC_file