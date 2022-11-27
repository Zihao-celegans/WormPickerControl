"""
    RNAi_AssignRSNames.py
    Assign group names to the Rescreen (RS) dataset from Julia's spreadsheets

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


panalysis = r'C:\Dropbox\WormWatcher (shared data)\RS57-268\_Analysis'
#panalysis = 'G:\RNAi_Project\_Analysis'
flabels = r'C:\Dropbox\WormWatcher (shared data)\RS57-268\Rescreens key.xlsx'
plate_filter = 'RS'
combine_all_rnai = False


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

# Load excel data 
data = pd.read_excel(flabels,'All RS').values

# Scan through plates and update their Group variable
platesfull,plates = verifyDirlist(panalysis,True,plate_filter)

# Keep track of censor reasons:
censor_reasons = np.zeros((5,1))

# Find a plate in the spreadsheet
for (platefull,plate) in zip(platesfull,plates):

    # Modify plate name for consistency to Julia's sheet
    plate_jdh = plate.replace(' ','-')
    plate_jdh = plate_jdh.replace('_','-')

    key, found, duplicate = findKeyInXlsx(plate_jdh,data)

    # Make sure exactly one entry was found
    if duplicate:
        print('Did not modify plate: ', plate, ' DUPLICATE')
        continue

    elif not found:
        print('Did not modify plate: ', plate, ' NOT FOUND')
        continue

    # Flatten group names to match spreadsheet formats
    new_group_names = key.flatten(order='F')
    new_group_names = np.reshape(new_group_names,(1,len(new_group_names)))

    # Censor group  names if requested
    fcsv = os.path.join(platefull,'ActivityCurves_' + plate + '.csv')
    new_group_names, reason = censorWellsAuto(fcsv,new_group_names,min_days,min_wf,frac_inc)

    # Replace the group names with RNAi vs EV, instead of individual RNAis, if requested
    if combine_all_rnai:
        cond1 = ~(new_group_names=='EV')
        cond2 = ~(new_group_names=='Censor')
        new_group_names[cond1 & cond2] = 'RNAi'

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


# Generate output csv
pparent, __ = os.path.split(panalysis)
data_csv(pparent, platesfull, platesfull, [[0.89, 0.99], [100]])

