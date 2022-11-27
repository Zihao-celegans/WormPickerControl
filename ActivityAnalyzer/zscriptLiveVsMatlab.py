"""
    zscriptComparePythonToMatlab.py
    Anthony Fouad
    August 2019
    

    Script for comparing the results of the python WWAnalyzer scripts to the legacy Matlab WWAnalyzer scripts
"""

import os
import scipy.io as sio
import numpy as np
import scipy.stats
import matplotlib.pyplot as plt

# Anthony's imports
from verifyDirlist import verifyDirlist
import csvio
from ErrorNotifier import ErrorNotifier
from ActivityCurves import findBlueFrames
from getSessionData import getSessionData
from zscriptLiveVsPost import getPostBlueAcitivity



# Find live-calculated data from WW2-03
plive="E:\RNAi_Project\_Analysis (LIVE)\CX_2D_2\Session data"
fullnames_live, partnames_live = verifyDirlist(plive,False,"csv")

# Find all matching MATLAB-calculated data from WW2-03
ppost = "E:\RNAi_Project\_Analysis (Matlab)"
fullnames_post = list()

for name in partnames_live :

    # The session name could be slightly different at the minutes or seconds level. Extract up to hours, low probability of borderline miss
    namefilter = name[8:30]

    # Get matching name from the post folder
    new_name = verifyDirlist(ppost,False,namefilter)[0]

    # Make sure there is exactly one match
    if len(new_name) > 1 or len(new_name) < 1:
        # Debug point here
        print("name: ", name, " found ", len(new_name), "matches in the post folder")
        fullnames_post.append("")
        continue

    # Add it to the list
    fullnames_post.append(new_name[0])


# For each session, calculate the final row of post-blue activity in the LIVE and POST analysis for comparison
N_points = 24 * len(fullnames_live)
All_live = np.zeros(shape=(0,))
All_post = np.zeros(shape=(0,))

for (livename,postname) in zip(fullnames_live,fullnames_post) :

    """ CSV live version """

    # Get the frame jump
    jump_live = getSessionData(livename,"<Frame Jump>")

    # Get live and post stimulated activity average
    Aroi_stim_live = getPostBlueAcitivity(livename, jump_live) 

    # Add it  to a list of all
    All_live = np.concatenate((All_live,Aroi_stim_live),axis=0)


    """ Matlab version """

    Aroi_stim_post = np.genfromtxt(postname, delimiter=',')
    All_post = np.concatenate((All_post,Aroi_stim_post),axis=0)

# Scale down to get rid of extra zeros
All_live /= 1000
All_post /= 1000

# Compute correlation
slope, intercept, r_value, p_value, std_err = scipy.stats.linregress(All_post, All_live)
xfit = np.arange(np.min(All_post),np.max(All_post))
yfit = slope*xfit + intercept
print("r2 = ",r_value**2)

#  Plot results
plt.figure(1)
plt.plot(All_post,All_live,'ok',markersize = 1)
plt.plot(xfit,yfit)
plt.title("Session stim. activity means")
plt.xlabel("Matlab", fontsize = 16)
plt.ylabel("Python", fontsize = 16)
plt.show()
B=1



    