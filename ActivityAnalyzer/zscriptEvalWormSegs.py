"""
    Script to evaluate worms segmented either by the difference image method (only gets moving worms) or the adaptive threshold method
    Runs using the 

"""

from csvio import readArrayFromCsv
from segmentWormsAdaptThresh import *
import matplotlib.pyplot as plt
import AnthonysPlots as ap
from importExcelManWellScores import *
import seaborn as sns
import pandas as pd




# Set params----------------------------------------------------------
panalysis = r"C:\Users\antho\Documents\Temp_WW2-02\_Analysis"
N_wells = 24
N_adapt_lvls = 6
# --------------------------------------------------------------------


# Load the manual segmentation for WW2-02
man_classes,man_plate_names,man_all_classes = importExcelManWellScores()



# Find all plates that have been analyzed
d_plates = verifyDirlist(panalysis,True,"")[0]

All_Counts_diff_4 = np.empty((0,N_wells))
All_Counts_adapt_4 = np.empty((0,N_wells))

All_Counts_adapt_20 = np.empty((0,N_wells))

All_Counts_diff_34 = np.empty((0,N_wells))
All_Counts_adapt_34 = np.empty((0,N_wells))

All_Manual_Empty = np.empty((0,N_wells))

for (pplate,j) in zip(d_plates,range(len(d_plates))):

    # Find CSV files
    p_sessions = os.path.join(pplate,"Session data")
    d_sessions = verifyDirlist(p_sessions,False,"csv","Roi")[0]

    # Skip this session if it doesn't have enough sessions
    if len(d_sessions) < 35:
        print("Skipped: ", pplate, " had only ", len(d_sessions))
        continue

    # Preallocate data arrays
    Counts_diff = np.zeros(shape=(len(d_sessions),N_wells))
    Counts_diff[:] = np.nan

    Counts_adapt = np.zeros(shape=(len(d_sessions),N_wells))
    Counts_adapt[:] = np.nan

    skip_flag = False
    for (session,i) in zip(d_sessions,range(len(d_sessions))):    

        # Load data from CSV
        worm_mvmt = readArrayFromCsv(session,"<N worm movements>")
        worm_statics = readArrayFromCsv(session,"<N worms>")
        good_areas = readArrayFromCsv(session,"<Worm area>")
        bad_areas = readArrayFromCsv(session,"<Bad area>")

        thresh_levels = range(2,-11,-1);

        if worm_mvmt.shape[0] != 1 or worm_statics.shape[0] < len(thresh_levels):
            skip_flag = True
            break
        
        # Find the optimum thresh level for each well
        gba = good_areas/bad_areas # Get ratio between good and bad area
        i_gba_max = np.argmax(gba,axis=0)
        i_gba_max[:] = 11

        
        # Display a plot of the relative area metrics
        show_thresh_plot = False
        if (show_thresh_plot):
            x = range(len(thresh_levels))
            plt.figure(1)
            plt.subplot(2,2,1)
            plt.plot(x,worm_statics)
            plt.title('Worm count')

            plt.subplot(2,2,2)
            plt.plot(x,good_areas)
            plt.title('Worm area')

            plt.subplot(2,2,3)
            plt.plot(x,bad_areas)
            plt.title('Bad area')

            plt.subplot(2,2,4)
            plt.plot(x,gba)
            plt.title('good / bad area')

            plt.show()
            exit()


        # Get the maximum number of worm movements recorded (should just be 1 row anyway)
        Counts_diff[i,:] = worm_mvmt

        # Get the static worm movements recorded
        temp = np.ones(shape =  (24,))
        for (idx,wn) in zip(i_gba_max,range(24)):
            Counts_adapt[i,wn] = worm_statics[i_gba_max[wn],wn]

    if skip_flag:
        print("Skipped: ", pplate, " array had wrong shape - reprocess")
        continue

    # Get the plate name to compare with the manual
    (trash,plate_name) = os.path.split(pplate)

    # Attempt to find the plate name in the list and get its data
    try: 
        i_man = man_plate_names.index(plate_name)
    except:
        print("Could not find manual entry for ", plate_name)
        continue

    # Extract the Manual data slice for whether these wells are empty


    this_empty = man_all_classes[i_man,:,2]
    wrong = np.any(~this_empty.astype(np.bool) & (Counts_adapt[4,:] < 4) )

    # Append the relevant rows to the final array
    All_Counts_diff_4 = np.concatenate((All_Counts_diff_4,np.reshape(Counts_diff[4,:],(1,24))),axis=0)
    All_Counts_diff_34 = np.concatenate((All_Counts_diff_34,np.reshape(Counts_diff[34,:],(1,24))),axis=0)
    All_Counts_adapt_4 = np.concatenate((All_Counts_adapt_4,np.reshape(Counts_adapt[4,:],(1,24))),axis=0)
    All_Counts_adapt_34 = np.concatenate((All_Counts_adapt_34,np.reshape(Counts_adapt[34,:],(1,24))),axis=0)
    All_Counts_adapt_20 = np.concatenate((All_Counts_adapt_20,np.reshape(Counts_adapt[20,:],(1,24))),axis=0) # Day 10
    All_Manual_Empty = np.concatenate((All_Manual_Empty,np.reshape(this_empty,(1,24))),axis=0)

    # Success!
    print("Successfully analyzed: ", pplate)




""" PART 3: CORRELATION BETWEEN THE TWO METRICS, EARLY ON AND LATER """

# Censor diff outliers
maxdiff = 55
tokeep = (All_Counts_diff_4<maxdiff) & (All_Counts_diff_34<maxdiff)


    #All_Counts_adapt_4 = All_Counts_adapt_4[tokeep]
    #All_Counts_adapt_34 = All_Counts_adapt_34[tokeep]

    #All_Counts_diff_4 = All_Counts_diff_4[tokeep]
    #All_Counts_diff_34 = All_Counts_diff_34[tokeep]



plt.figure(3, figsize = (8,4))

ax1 = plt.subplot(1,2,1)
ap.scatterBestFit(All_Counts_diff_4,All_Counts_adapt_4, use_jitter = True, c = 'k', s = 2)
plt.xlabel("N worms (diff. img.)")
plt.ylabel("N worms (adapt. thresh.)")

ax2 = plt.subplot(1,2,2, sharex=ax1)
ap.scatterBestFit(All_Counts_diff_34,All_Counts_adapt_34, use_jitter = True, c = 'k', s = 2)
plt.xlabel("N worms (diff. img.)")





""" PART 4: HOW PREDICTIVE IS WORM COUNT OF EMPTY/NONEMPTY CLASS """

# Flatten the arrays
All_Manual_Empty = All_Manual_Empty.flatten()
All_Counts_diff_4 = All_Counts_diff_4.flatten()
All_Counts_adapt_4 = All_Counts_adapt_4.flatten()
All_Counts_adapt_20 = All_Counts_adapt_20.flatten()
All_Counts_adapt_34 = All_Counts_adapt_34.flatten()

# Remove all nan manuals (if no manual correlate was found) - not necessary using concat method as no nans present
#to_keep = ~np.isnan(All_Manual_Empty)
#All_Manual_Empty = All_Manual_Empty[to_keep]
#All_Counts_diff_4 = All_Counts_diff_4[to_keep]
#All_Counts_adapt_4 = All_Counts_adapt_4[to_keep]

# Get counts of empty wells, organized in a dict
data = dict()
data["All_diff_empty"] = All_Counts_diff_4[All_Manual_Empty.astype(np.bool)]
data["All_adapt_empty"] =  All_Counts_adapt_4[All_Manual_Empty.astype(np.bool)]
data["All_adapt_empty_d10"] = All_Counts_adapt_20[All_Manual_Empty.astype(np.bool)]

# Get counts of full wells
data["All_diff_full"] = All_Counts_diff_4[~All_Manual_Empty.astype(np.bool)]
data["All_adapt_full"] = All_Counts_adapt_4[~All_Manual_Empty.astype(np.bool)]
data["All_adapt_full_d10"] = All_Counts_adapt_20[~All_Manual_Empty.astype(np.bool)]

# Organize the data in a dict for pandas

# Show beeswarm plot
plt.figure(4,figsize=(12,5))
sns.set(style="whitegrid")
group = ["diff empty"]
df0  =pd.DataFrame({"All_diff_empty":data["All_diff_empty"]})
df1  =pd.DataFrame({"All_adapt_empty":data["All_adapt_empty"]})
df1a  =pd.DataFrame({"All_adapt_empty_d10":data["All_adapt_empty_d10"]})


df2  =pd.DataFrame({"All_diff_full":data["All_diff_full"]})
df3  =pd.DataFrame({"All_adapt_full":data["All_adapt_full"]})
df3a = pd.DataFrame({"All_adapt_full_d10": data["All_adapt_full_d10"]})

df_diff = pd.concat([df0,df2],ignore_index = False, axis = 1)
df_adapt= pd.concat([df1,df3],ignore_index = False, axis = 1)
df_adapt_10 = pd.concat([df1a,df3a],ignore_index = False, axis = 1)

palette = sns.light_palette("blue", reverse=False,  n_colors=len(All_Counts_diff_4) )
#sns.swarmplot(s=6.8, data = df, palette=palette) # Scatter point
plt.subplot(1,3,1)
sns.boxplot(data=df_diff,whis = 1.0)
plt.subplot(1,3,2)
sns.boxplot(data=df_adapt,whis = 1.0)
plt.subplot(1,3,3)
sns.boxplot(data=df_adapt_10,whis = 1.0)

""" PART 5: SUMMARY STATISTICS OF  THE UNFILTERED / FILTERED DATASET """

plt.figure(5,figsize = (16,6))

# Show a histogram of all distributions
N_bins = 26
plt.subplot(1,3,1)
plt.hist(All_Counts_adapt_4, bins = N_bins)
plt.hist(All_Counts_adapt_20, bins = N_bins)
plt.hist(All_Counts_adapt_34, bins = N_bins)
plt.legend(("Day 2", "Day 10", "Day 17"))
plt.xlabel("Worms Counted")
plt.ylabel("# Wells")

# Create filter for wells with less than 10 worms counted on day 3 or day 10
bad_day2 = All_Counts_adapt_4 < 8
bad_40pctloss = np.logical_and((All_Counts_adapt_20 / All_Counts_adapt_4) < 0.5,np.logical_not(bad_day2))
bad_too_many = All_Counts_diff_4 > 55


ratio = All_Counts_adapt_20 / All_Counts_adapt_4
ratio = ratio[ratio<50]



to_delete = bad_day2 | bad_40pctloss | bad_too_many
All_Counts_adapt_4[to_delete] = np.nan
All_Counts_adapt_20[to_delete] = np.nan
All_Counts_adapt_34[to_delete] = np.nan

# Show histogram of filtered distributions
plt.subplot(1,3,2)
plt.hist(All_Counts_adapt_4, bins = N_bins)
plt.hist(All_Counts_adapt_20, bins = N_bins)
plt.hist(All_Counts_adapt_34, bins = N_bins)
plt.legend(("Day 3", "Day 10", "Day 17"))
plt.xlabel("Worms Counted")
plt.ylabel("# Wells")

plt.subplot(1,3,3)
plt.hist(ratio, bins = N_bins)

# Print out kept numbers
print("Wells: ", to_delete.shape[0], " | Kept: " , to_delete.shape[0] - sum(to_delete), " | frac: ", (to_delete.shape[0] - sum(to_delete))/to_delete.shape[0])

plt.show()
B=1
