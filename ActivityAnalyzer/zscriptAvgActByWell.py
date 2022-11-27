"""
    Evaluate the average activity across the plate at a specific timepoint, e.g. 24 hours
"""

from verifyDirlist import *
from csvio import *
import numpy as np
from AnalysisPlots import *

# Set the timepoint of cumulative activity to use
time_d = 0.75

# Find the plate folders files
panalysis = r'E:\Mike\_Analysis'
d_plates = verifyDirlist(panalysis,True)[0]

# For each CSV file load in the total activity at timepoint 0.75 days
Act = np.zeros(shape = (len(d_plates),24))

for (pplate,plate_num) in zip(d_plates,range(len(d_plates))):

    # Find the CSV file
    d_csv = verifyDirlist(pplate,False,".csv")[0]

    if len(d_csv) != 1:
        print("Skipped ", pplate)
        continue

    # Load the cumulative activity data
    cumact = readArrayFromCsv(d_csv[0],"<ROI cumulative activity NO noise-floor subtraction>")
    t = readArrayFromCsv(d_csv[0],"<Day>")

    # Find the first timepoint above 0.75 d
    higher_than_limit = t>time_d
    idx = np.min(np.argwhere(higher_than_limit.flatten()))
    Act[plate_num,:] = cumact[idx,:]


    print(t,cumact)

# Get the median activity for all plates
ActMed = np.median(Act,axis=0)

# Convert to an image for display
plotWellMapImage(ActMed,4,6,"Median activity at 18h")
plt.show()
