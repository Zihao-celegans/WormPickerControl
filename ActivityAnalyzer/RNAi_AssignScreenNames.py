"""
    RNAi_AssignScreenNames.py
    Assign group names to the first-round screen dataset from Julia's spreadsheets

    Assign "Censored" group to any well failing certain filters

"""

from verifyDirlist import *
import pandas as pd
import numpy as np
from csvio import findKeyInXlsx, replaceVariableInCsv
import os

from censorWellsAuto import *
from plotGroupAverages import data_csv


""" Parameters for naming wells """


#panalysis = r'C:\Dropbox\WormWatcher (shared data)\WW2-03_Analysis\_Analysis'
panalysis = r'C:\Dropbox\WormWatcher (shared data)\WW2-03-2020\_Analysis' # WW2-02
plate_filter = 'C'
combine_all_rnai = True


""" Parameters for censoring the wells: """


# 25 days minimum for experiment
min_days = 25        

# Day 3 and day 10 must have
min_wf = list()
min_wf.append([3,0.08,0.50]) # Day 3 must have between 0.15 and 0.50 worm fraction
min_wf.append([10,0.07,0.50]) # Day 10 

# Worm fraction should not increase except in FUDR failure, check 5d bins, should not increase by >25%
frac_inc = 1.45



""" Apply labels """


# Scan through plates and update their Group variable
platesfull,plates = verifyDirlist(panalysis,True,plate_filter)
skip = np.zeros((len(plates),))

# Keep track of censor reasons:
censor_reasons = np.zeros((5,1))

# Find a plate in the spreadsheet
for (platefull,plate,i) in zip(platesfull,plates,range(len(plates))):


    # Define group names
    if combine_all_rnai and (('_4' in plate) or ('_5' in plate)):
        new_group_names = ['RNAi','RNAi','RNAi','EV','RNAi','RNAi','RNAi','EV','RNAi','RNAi','RNAi','EV','RNAi','RNAi','RNAi','EV','RNAi','RNAi','RNAi','daf-2','RNAi','RNAi','RNAi','daf-16']
    elif combine_all_rnai:
        new_group_names = ['RNAi','RNAi','RNAi','EV','RNAi','RNAi','RNAi','EV','RNAi','RNAi','RNAi','EV','RNAi','RNAi','RNAi','EV','RNAi','RNAi','RNAi','RNAi','RNAi','RNAi','RNAi','RNAi']
    new_group_names = np.array(new_group_names)
    new_group_names = np.reshape(new_group_names,(1,24))

    # Censor group  names if requested
    fcsv = os.path.join(platefull,'ActivityCurves_' + plate + '.csv')
    if os.path.exists(fcsv):
        new_group_names, reason = censorWellsAuto(fcsv,new_group_names,min_days,min_wf,frac_inc)
    else:
        print('Skipped - no ActivityCurves file - ', fcsv)
        skip[i] = 1
        continue

    # Replace group name
    replaceVariableInCsv(fcsv,",,<Group name>",new_group_names,2)

    # Replace Censor reason, or, if it doesn't exist, add it
    replaceVariableInCsv(fcsv,",,<Censor reason>",reason,2)
    
    # Keep track of the reasons for censoring
    for j in range(censor_reasons.shape[0]-1) :
        censor_reasons[j] += np.sum(reason==j)

        
    B=1

# Print out the censorship results
for j in range(censor_reasons.shape[0]-1) :
    print('Censor reason: ',j, ' occurrences: ', censor_reasons[j])

print('Total wells:' , np.sum(censor_reasons))

# Remove skipped (no csv file)
platesfinal = [thisplate for (thisplate,i) in zip(platesfull,range(len(platesfull))) if skip[i]==0]


# Generate output csv
pparent, __ = os.path.split(panalysis)
data_csv(pparent, platesfinal, platesfinal, [[0.89, 0.99], [100]])

