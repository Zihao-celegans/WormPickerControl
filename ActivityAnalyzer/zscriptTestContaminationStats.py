"""
    Test whether a combination of simple imaging metrics can be used to predict whether
    a well is marked contaminated or not


    Adapted from CNN_ExportROIContClean.py
"""


from verifyDirlist import *
from csvio import *
import os
import numpy as np
import numpy.matlib
from shutil import copyfile
import matplotlib.pyplot as plt
import cv2
import pandas as pd
import seaborn as sns


# Parameters
pparent = r'E:\RNAi_Project'
pout = r'E:\RNAi_project_well_images_unsorted'
fxlsx = r'C:\Dropbox\WormWatcher (shared data)\Well score training\WellScores_Data67ImageBased.csv'
fcsvout = r'C:\Dropbox\WormWatcher (shared data)\Well score training\WellScores_TrainData.csv'
sessions_per_day = 2
days_to_sample = [20]    # only do last day for now
days_to_sample = np.asarray(days_to_sample)
crop_middle_flag = True
outsize = 400 # Size to use for metrics, affects things like adaptive thresholding kernel relative size


# Load the excel file
#xls = pd.ExcelFile(fxlsx)
print('Loading excel or csv file...')
data67 = pd.read_csv(fxlsx).values
#dataorig = pd.read_excel(xls, 'Data').values

# Organize excel data
labels67 = data67[:,1].astype(np.str)
data67 = data67[:,2:].astype(np.float)

#labelsorig = dataorig[:,1].astype(np.str)
#dataorig = dataorig[:,2:].astype(np.float)

# Find the plate folders files
d_plates, plate_names = verifyDirlist(pparent,True,"","_Analysis")

# Going to keep a list of all fout images
foutlist = list()



# Preallocate arrays to hold data
var_names = ["Rel. Mean","Stdev","Bright area","Worm Fraction"]
N_sample_sessions = len(days_to_sample)
N_vars_per_session = len(var_names) 


is_contaminated = np.ones((10000,24))*np.nan

stats_all_by_sess = list()
for i in range(N_sample_sessions):
    stats_all_by_sess.append(np.ones(shape=(10000,24,N_vars_per_session))*np.nan)

print('Analyzing plates...')


# For each CSV file load in the total activity at timepoint 0.75 days
for (pplate,name,i) in zip(d_plates,plate_names,range(len(plate_names))):

    # Find the corresponding entry in the excel data
    has_name = np.char.find(labels67,name)
    idx = np.argwhere(has_name>=0)

    if len(idx) == 0:
        print('Could not find matching XLSX entry for: ', name)
        continue

    elif len(idx) > 1:
        print('Found too many match XLSX entries for: ', name)
        continue

    else:
        idx = idx[0]
        xlsxdata = data67[idx,:].flatten()


    # Find the Sessions for this plate
    d_sessions_full,d_sessions = verifyDirlist(pplate,True,"")

    # Skip plate if not enough sessions
    sessions_to_sample = np.round(days_to_sample * sessions_per_day)

    if len(d_sessions) < max(sessions_to_sample+1):
        print("-----------------------------> Skipping (not enough sessions): ",name)
        continue



    # Extract one image from requested sessions 
    d_sessions_full = [d_sessions_full[i] for i in sessions_to_sample]
    d_sessions = [d_sessions[i] for i in sessions_to_sample]


    # Determine whether this plate will be training or testing
    if i%4 == 0:
        subfolder = 'test'
    else:
        subfolder = 'train'


    # Loop over sessions
    for (session_full,session,snum) in zip(d_sessions_full,d_sessions,range(len(d_sessions))):

        # Get the ROI coordinates
        d_roi = verifyDirlist(session_full,False,'ROI_Coord')[0]

        if len(d_roi) != 1:
            print("-----------------------------> Skipping (no ROI file!!): ",session_full)
            continue

        rois = readArrayFromCsv(d_roi[0],"<ROI Coordinates>")
        x = rois[0,:].astype(np.int)
        y = rois[1,:].astype(np.int)
        ri = rois[2,:].astype(np.int)
        ro = rois[3,:].astype(np.int)

        # Get the worm fraction
        pplate_ana = os.path.join(pparent,'_Analysis',name,"Session data")
        d_csv_ana = verifyDirlist(pplate_ana,False,session)[0]

        if len(d_csv_ana) != 1:
            print("-----------------------------> Skipping (no STEP 1 CSV file!!): ",session_full)
            continue

        worm_fracs = readArrayFromCsv(d_csv_ana[0],"<Worm Fraction>").flatten()

        # Get the images for this image series
        d_img_full,d_img = verifyDirlist(session_full,False,'png')

        if len(d_img_full) < 1:
            print("-----------------------------> Skipping (no images!!): ",session_full)
            continue

        fimg = d_img_full[0]
        img = cv2.imread(fimg)

        # Extract the ROI images and mark the worms
        for (thisx,thisy,thisri,thisro,roinum,thiswf) in zip(x,y,ri,ro,range(1,len(x)+1),worm_fracs):

            # Determine whether this well is marked contaminated or not
            if ("4" in str(xlsxdata[roinum-1])):  # xlsxdata[roinum-1] > 0: #"6" in str(xlsxdata[roinum-1]):
                this_pout = os.path.join(pout,subfolder,'Contaminated')
                contaminated = True
                
            else:
                this_pout = os.path.join(pout,subfolder,'Clean')
                contaminated = False


            # Create the output path if it doesn't already exist
            if not os.path.exists(this_pout):
                os.makedirs (this_pout)

            # Crop to ROI
            crop_img = img[thisy-thisro:thisy+thisro, thisx-thisro: thisx+thisro].astype(np.float)
            crop_img = crop_img[:,:,0] # Grayscale
            crop_img_unmasked = np.copy(crop_img)

            # Resize to standard size
            crop_img = cv2.resize(crop_img,(outsize,outsize))


            # Crop to only inside 50% of image?
            if crop_middle_flag:
                cr = int(crop_img.shape[0]/4)
                crop_img = crop_img[cr:cr+2*cr,cr:cr+2*cr]

                # Clip corners from the crop images?


            # Segment the well using adaptive  threshold
            bwWide = cv2.adaptiveThreshold(crop_img.astype(np.uint8),255,cv2.ADAPTIVE_THRESH_MEAN_C,cv2.THRESH_BINARY,99,-3)

            # Find out how many objects are there (excluding very small puncta)
            bwTemp = np.copy(bwWide)
            bwTemp = cv2.erode(bwTemp,np.ones((3,3)))

            # Blur the image heavily so we can get STD stats at low frequency
            # crop_img_blur = cv2.GaussianBlur(crop_img,(33,33),15) 

            # Store contamination decision and simple metrics about the wells
            is_contaminated[i,roinum-1] = contaminated

            stats_all_by_sess[snum][i,roinum-1,0] = np.nanmean(crop_img)        # Layer 0 - mean value - must be changed to rel mean later
            stats_all_by_sess[snum][i,roinum-1,1] = np.nanstd(crop_img)         # Layer 2  - stdev
            stats_all_by_sess[snum][i,roinum-1,2] = np.mean(bwWide)             # 
            stats_all_by_sess[snum][i,roinum-1,3] = np.mean(thiswf) 

            # Display the crop image
            #print("Contamination: ", contaminated, " | intensity: ", stats_intensity[i,roinum-1], " | std: ", stats_std[i,roinum-1], " | #objects: ", len(contours))
            #plt.subplot(1,2,1)
            ##plt.imshow(bwPuncta)
            ##plt.subplot(1,3,2)
            #plt.imshow(bwWide)
            #plt.subplot(1,2,2)
            #plt.imshow(crop_img)
            #plt.show()
            



# Analysis of the set


# Find out how many variables were used, excluding is_contaminated


# Extract statistics from last day sampled
stats_all = stats_all_by_sess[-1]
N_vars = N_vars_per_session

# Compare well MEAN to overal MEAN for this well position
overall_mean = np.nanmean(stats_all[:,:,0],axis = 0)
overall_mean = numpy.matlib.repmat(overall_mean,stats_all[:,:,0].shape[0],1)
relative_intensity = np.divide(stats_all[:,:,0], overall_mean)
stats_all[:,:,0] = relative_intensity

# Normalize ALL metrics to their average (median) value
for i in range(0,stats_all.shape[2]):
    stats_all[:,:,i] = np.divide(stats_all[:,:,i],np.nanmedian(stats_all[:,:,i]))

# Change Intensity to relative intensity change over time - didn't work
# stats_all[:,:,0] = np.divide(stats_all_by_sess[-1][:,:,0],stats_all_by_sess[0][:,:,0])



# Add well data, divided between contaminated and clean, and plot
datalist = list()
for var in range(0,N_vars):

    # Get data for this variable
    this_var_data = list()
    for i in range(2):
        this_var_data.append(stats_all[:,:,var][is_contaminated == i])
    datalist.append(this_var_data)



# Plot
print("Generating ", N_vars, " plots")
plt.figure(1)
for var in range(N_vars):
    plt.subplot(1,N_vars,var+1)
    #sns.set(style="darkgrid")

    palette = sns.dark_palette("blue", reverse=False,  n_colors=len(datalist) )
    a=sns.swarmplot(data = datalist[var], palette=palette) # Scatter point
    sns.boxplot(data=datalist[var])

    a.set_xticklabels(a.get_xticklabels(), rotation=45, fontsize=8)
    plt.title(var_names[var])
plt.show()


# Print out data

# tabulate all data
for i in range(N_vars):

    if i == 0:
        data_out = stats_all[:,:,i].flatten()
        data_out = data_out[~np.isnan(data_out)]
        data_out = np.reshape(data_out,(len(data_out),1))
    else:
        this_data = stats_all[:,:,i].flatten()
        this_data = this_data[~np.isnan(this_data)]
        this_data = np.reshape(this_data,(len(this_data),1))

        data_out = np.concatenate((data_out,this_data),axis=1)

# Tabulate results
results = is_contaminated.flatten()
results = results[~np.isnan(results)]
results = np.reshape(results,(len(results),1))


data_out = np.concatenate((data_out,results),axis=1)

# Save csv
np.savetxt(fcsvout,data_out,delimiter=',',fmt='%0.2f')


# Set thresholds to make a decision


# Does Worm fraction predict contamination?????