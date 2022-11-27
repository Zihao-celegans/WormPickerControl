
""" 
    AnalysisPlots.py
    Anthony Fouad
    July 2019

    Various functions for displaying common WormWatcher data visualizations
    
"""

import matplotlib.pyplot as plt
import numpy as np



""" Plot an array as a colormap across the plate wells on current axis """

def plotWellMapImage(datavector,N_rows,N_cols,TITLE) :

    # Convert the data vector to a 24 well (for example) array
    N_wells = N_rows*N_cols
    Arr = np.zeros((N_rows,N_cols))
    for i in range(0,N_wells) :
        c = int(i/N_rows)
        r = i%N_rows
        Arr[r,c] = datavector[i]
    plt.imshow(Arr)

    # Show Disable all ticks
    plt.tick_params(
        axis='x',          # changes apply to the x-axis
        which='both',      # both major and minor ticks are affected
        bottom=False,      # ticks along the bottom edge are off
        top=False,         # ticks along the top edge are off
        labeltop = False,
        labelbottom=False) # labels along the bottom edge are off    
    
    plt.tick_params(
        axis='y',          # changes apply to the x-axis
        which='both',      # both major and minor ticks are affected
        left=False,      # ticks along the bottom edge are off
        right=False,         # ticks along the top edge are off
        labelleft = False,
        labelright=False) # labels along the bottom edge are off     

    plt.colorbar()
    plt.title(TITLE)

    # Add the well numbers in the center
    roinum = 0;
    for c in range(0,N_cols) :
            for r in range(0,N_rows) :
                roinum+=1
                if (N_cols < 20):
                    plt.text(c,r,str(roinum),color="white",
                             verticalalignment="center",
                             horizontalalignment="center",
                             fontsize = 6)



