"""
    Censor wells by marking their group as "Censor"

    Also return the reason code in case of multiple codes, the last is used:

    0 - not censored
    1 - too few days to analyze
    2 - FUDR failure / progeny / increasing worm count
    3 - too few worms at any checkpoint (supercedes #2)


"""


from verifyDirlist import *
import pandas as pd
import numpy as np
from csvio import *
import os
import scipy.signal
import matplotlib.pyplot as plt



def censorWellsAuto(AC_file,labels,min_days,min_wf,frac_inc):

    # Load the worm fraction from the plate
    worm_frac = readArrayFromCsv(AC_file,"<Worm Fraction>")
    t = readArrayFromCsv(AC_file,"<Day>")
    worm_frac = worm_frac[:,0:labels.shape[1]]

    # Keep track of the reason for censoring
    reason = np.zeros(labels.shape)

    # Censor all if not enough days to analyze
    if  np.max(t) < min_days:
        labels[:] = 'Censor'
        reason[:] = 1
        return labels, reason

    # Worm fraction based rules - median filter the wf
    kernel_size = (5,1)
    worm_frac = scipy.signal.medfilt2d(worm_frac, kernel_size)
    worm_frac_avg = np.mean(worm_frac, axis=1)


    # Worm fraction should not increase, except perhaps at the very beginning. Otherwise FUDR failure (later superseded by too low WF)
    has_increase = np.zeros((1,24))
    jump_days = 5
    sessions_per_day = int(worm_frac.shape[0]/np.max(t))
    jump = int(jump_days * sessions_per_day)
    is_progeny = False

    for i in range(2*jump,worm_frac.shape[0],jump):
        
        # Set compare range to be 5 days before
        i0 = i - jump

        # More worms on day+5 than day? it's bad
        is_progeny = np.divide(worm_frac_avg[i], worm_frac_avg[i0]) > frac_inc
        if is_progeny:
            labels[:] = 'Censor'
            reason[:] = 2
            break        


    #print(is_progeny)
    #plt.plot(worm_frac_avg)
    #plt.show()

    # Worm fraction must be greater than the minimum threshold at each requested checkpoint
    for rule in min_wf:

        idx = np.min(np.argwhere(t.flatten() >= rule[0]))
        this_worm_frac = worm_frac[idx,:]
        this_bad = (this_worm_frac<rule[1]) | (this_worm_frac>rule[2])
        labels[0,this_bad] = 'Censor'
        reason[0,this_bad] = 3



    return (labels,reason)
