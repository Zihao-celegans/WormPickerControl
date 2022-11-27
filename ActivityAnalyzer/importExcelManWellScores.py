
import numpy as np
import pandas as pd
import matplotlib.pyplot as plt


def importExcelManWellScores(fname = r'C:\Dropbox\WormWatcher (shared data)\Well score training\WellScores.xlsx',sname = "Data"):

    # Load the excel data
    df = pd.read_excel(fname,sheet_name = sname)
    M=df.as_matrix()

    plate_names = M[:,1]
    plate_names = plate_names.tolist()
    well_data = M[:,2:26]
    well_data = well_data.astype(np.float)

    # Crop past the last plate
    icrop = 0
    for i in range(len(plate_names)):
        if type(plate_names[i]) == float :
            icrop = i
            break

    plate_names = plate_names[:i-1]
    well_data = well_data[:i-1,:]

    # Interpret the types for each well
    well_classes = np.zeros(shape = (well_data.shape[0],well_data.shape[1],10))

    for (row,r) in zip(well_data,range(well_data.shape[0])):
        for (col,c) in zip(row,range(well_data.shape[1])):
            digits = [int(i) for i in str(col) if i.isdigit()]
            digits = np.unique(digits)

            # OVerall array of classifications
            well_classes[r,c,:digits.shape[0]] = digits

    # Rearrange the results so that each classification, e.g. 1, is represented in a layer, e.g. 0
    all_classes = np.zeros(shape = (well_data.shape[0],well_data.shape[1],10))

    for classnum in range(all_classes.shape[2]):

        all_classes[:,:,classnum] = np.max(well_classes == classnum,axis=2)


    # Well has ANY bad
    all_bads = np.max(well_classes,axis=2)>0
    all_classes[:,:,1] = all_bads

    # Special case for all good wells: Must have NONE of the bads
    all_goods = ~all_bads
    all_classes[:,:,0] = all_goods
    print("Fraction good = ", np.mean(all_goods))


    # Plot results if this is the main file
    classes = ["Good","Bad","Empty","Progeny/Bulk","Contaminated","Starved/Clustered","Other"]
    if __name__ == "__main__":
        plt.figure(1)
        for i in range(0,7):
            plt.subplot(1,7,i+1)
            plt.imshow(all_classes[:,:,i])
            plt.title(classes[i])

        plt.show()

    # Return results
    return(classes,plate_names,all_classes)

if __name__ == "__main__":
    importExcelManWellScores