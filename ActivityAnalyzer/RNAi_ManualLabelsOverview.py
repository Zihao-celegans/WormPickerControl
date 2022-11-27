

from verifyDirlist import *
import pandas as pd
import numpy as np
from csvio import *
import os
import scipy.signal
import matplotlib.pyplot as plt



fxlsx = r'C:\Dropbox\WormWatcher (shared data)\Well score training\WellScores.xlsx'
sheet = r''



# Load the excel file
xls = pd.ExcelFile(fxlsx)
data67 = pd.read_excel(xls, 'DataInclGrp6-7_IMAGEBASED').as_matrix()
dataorig = pd.read_excel(xls, 'Data').as_matrix()

# Organize excel data
labels67 = data67[:,1].astype(np.str)
data67 = data67[:,2:26].astype(np.float)

labelsorig = dataorig[:,1].astype(np.str)
dataorig = dataorig[:,2:26].astype(np.float)

hascont = 0
hasfudrfail = 0
hasempty = 0
total = 0
hasboth = 0


for (row,label) in zip(data67,labels67):

    if 'nan' in label:
        continue

    for elem in row:

        if '2' in str(elem):
            hasempty += 1

        if '4' in str(elem):
            hascont += 1

        if ('2' in str(elem)) and ('4' in str(elem)):
            hasboth += 1

        if '3' in str(elem):
            hasfudrfail += 1

        total += 1
B=1