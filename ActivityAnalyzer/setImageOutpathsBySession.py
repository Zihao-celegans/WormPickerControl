""" 
    setImageOutpathsBySession.py
    Anthony Fouad
    July 2019

    
"""

#Includes
from datetime import datetime
from dateutil.parser import parse
import os


def setImageOutpathsBySession(fullnames,thresh_min) :

    # Create blank list of when the sessions in this folder start
    newfullnames = list()
    current_outpath = ""

    # Go throught he folder and add unique session start times within it to the list
    last_dt = datetime(1900,1,1,0,0,0,0)
    namelen = 25        # wormwatcher filenames always exactly 25 characters

    for name in fullnames :

         # Get name length
        end = len(name)

        # Skip image if its name is not long enough
        if end < namelen :
            print("WARNING: Skipped file: ", name," because its name is too short.")
            continue

        # Find the date / name of this image
        (session_path,fname) = os.path.split(name)
        (plate_path,trash) = os.path.split(session_path)
        timestr = fname[:len(fname)-4]

        # pre-check for incorrect times that end in "-60)" instead of correctly incrementing to the next minute
        err_60s = '-60)' in timestr
        if err_60s :
            timestr = timestr.replace('-60)','-59)')

        # Convert it to a datetime
        dt = datetime.strptime(timestr,'%Y-%m-%d (%H-%M-%S)')

        # Calculate time since last image
        elapsed_time = dt-last_dt
        elapsed_time_s = elapsed_time.total_seconds() / 60

        # Create a new session grouping if we are above the threshold
        if elapsed_time_s > thresh_min :
            current_outpath = plate_path + "\\" + timestr + "\\"
        
        # Remember this datetime for next image comparison
        last_dt = dt

        # Format the fullname of this image in the new directory
        this_newfullname = current_outpath + timestr + ".png"
        newfullnames.append(this_newfullname)


    return (newfullnames,current_outpath)

