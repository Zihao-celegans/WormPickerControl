"""
    Extract sample images from each folder in a parent folder
"""

from verifyDirlist import *
from csvio import *
import os
import numpy as np
from shutil import copyfile

# Parameters
pparent = r'F:\WI_Project'
pout = r'C:\Dropbox\WormWatcher (shared data)\Wild Isolate TAC WW2'
sessions_per_day = 2
days_to_sample = [3,8,16]
days_to_sample = np.asarray(days_to_sample)

# Find the plate folders files
d_plates, plate_names = verifyDirlist(pparent,True,"","_Analysis")

# For each CSV file load in the total activity at timepoint 0.75 days

for (pplate,name) in zip(d_plates,plate_names):

    # Find the Sessions for this plate
    d_sessions = verifyDirlist(pplate,True,"")[0]

    # Extract one image from requested sessions 
    sessions_to_sample = np.round(days_to_sample * sessions_per_day)

    for (snum,daynum) in zip(sessions_to_sample,days_to_sample):
        d_img_full,d_img = verifyDirlist(d_sessions[snum],False,'png')
        f_img = d_img_full[5]

        p_out_this = os.path.join(pout,r'Day ' + str(daynum))
        try:
            os.mkdir(p_out_this)
        except:
            print('Dir already exists')

        f_out = os.path.join(p_out_this, name + "_" + d_img[5])
        copyfile(f_img,f_out)