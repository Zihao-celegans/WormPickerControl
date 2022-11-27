"""
    getSessionData()
    Anthony Fouad
    July 2019

    INPUTS:
        fullfilename - the full file name of the CSV file to parse
        header - Which data to extract, for example <ROI Activity>

"""

import csv
import numpy as np
import os
from datetime import datetime
import matplotlib.pyplot as plt
import csvio



def getSessionData(fullfilename,header) :
    return csvio.readArrayFromCsv(fullfilename,header)


# Get the timestamp from the CSV file name
def getSessionTimepoint(fullfilename) :

    # Split filename into head
    (session_path,name) = os.path.split(fullfilename)
    timestr = name[-25:-4]

    # pre-check for incorrect times that end in "-60)" instead of correctly incrementing to the next minute
    err_60s = '-60)' in timestr
    if err_60s :
        timestr = timestr.replace('-60)','-59)')

    # Convert it to a datetime
    dt = datetime.strptime(timestr,'%Y-%m-%d (%H-%M-%S)')

    return (dt,timestr)


def validateSessionData(Err, Aroi, Aann, Aout, jump, N_rows, N_cols, coords, sessionfullname, worm_mvmt, bulk_mvmt) : 

    # Validate the integers are not arrays
    good_ints = type(N_rows) == int and type (N_cols) == int and type (jump) == int
    if not good_ints :
       print("Error analyzing session:\n\t",sessionfullname,"\n\tN_rows, N_cols, and Frame Jump must be integers")
       return False

    # Validate the arrays are arrays
    good_arrays = isinstance(Aroi,np.ndarray) and isinstance(Aann,np.ndarray) and isinstance(Aout,np.ndarray)
    if not good_arrays :
       print("Error analyzing session:\n\t",sessionfullname,"\n\tROI, Annular, or Non-ROI activity must be numpy arrays")
       return False

    # Validate the number of wells
    N_wells = N_rows * N_cols
    good_wells = N_wells == Aroi.shape[1] #and N_wells == Aann.shape[1] - NOTE that annular activity might not have been calculated on all versions
    if not good_wells :
        print("Error analyzing session:\n\t",sessionfullname,"\n\tInvalid number of wells: ", N_wells, " Does not match array sizes")
        return False

    # Validate the shape of the arrays
    N_frames = Aroi.shape[0]
    good_frames =N_frames == Aout.shape[0] and Aout.shape[1] == 1 #  N_frames == Aann.shape[0] and  - NOTE that annular activity might not have been calculated on all versions
    if not good_frames : 
        print("Error analyzing session:\n\t",sessionfullname, "\n\tThe activity arrays in the CSV file for ROI, Annular, or Non-ROI don't have the correct shape.")
        return False

    # Validate the shape of the ROI coordinates
    good_coords = coords.shape[0] == 4 and coords.shape[1] == N_wells;
    if not good_coords : 
        print("Error analyzing session:\n\t",sessionfullname, "\n\tThe ROI coordinates array does not have the correct shape.")
        return False

    # Validate that the movement data was computed
    good_mvmt = type(worm_mvmt) != str and type(bulk_mvmt) != str
    if not good_mvmt:
        Err.notify("Abort","Error analyzing session:\n" + sessionfullname + "\nbulk and worm movement calculations not found. Please re-run STEP 1.")
        return False

    # Should only reach here if everything is good, but just in case...
    return good_wells and good_frames and good_ints and good_arrays and good_coords and good_mvmt



def plotSessionData(sessionfullname, Aroi, Aann, Aout, blue_excl_start, blue_idx) :

    fout_sessionfig = sessionfullname.replace("Session_","SessionPlot_");
    fout_sessionfig = fout_sessionfig.replace(".csv",".png");


    # Set Blue effected areas to nan if blue is preset
    if blue_idx is not None :
        Aroi[blue_excl_start:blue_idx+1,:] = 0
        Aann[blue_excl_start:blue_idx+1,:] = 0
        Aout[blue_excl_start:blue_idx+1,:] = 0

    max_Act = np.max(Aroi,axis=None)

    f = plt.figure(num=2, figsize = (8,2.8), dpi = 200)
    plt.clf()

    plt.subplot(1,3,1)
    plt.plot(Aroi,'k')
    plt.ylabel('Activity (x1000 a.u.)')
    plt.title('ROI Activity')
    plt.axis(xmin=0,xmax=Aroi.shape[0],ymin = 0,ymax = max_Act)

    plt.subplot(1,3,2)
    plt.plot(Aann,'k')
    plt.xlabel('Frame')
    plt.title('Annular Activity')
    plt.axis(xmin=0,xmax=Aroi.shape[0],ymin = 0,ymax = max_Act)

        
    plt.subplot(1,3,3)
    plt.plot(Aout,'k')
    plt.title('Non-ROI Activity')
    plt.tight_layout()
    plt.axis(xmin=0,xmax=Aroi.shape[0],ymin = 0,ymax = max_Act)

    f.show()
    f.savefig(fout_sessionfig,dpi = 200)


