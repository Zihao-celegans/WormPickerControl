

# Anthony's Includes
from verifyDirlist import verifyDirlist
from breakUpSession import breakUpSession
from getSessionData import getSessionData
from getSessionData import getSessionTimepoint
from getSessionData import validateSessionData
from getSessionData import plotSessionData
from ActivityCurves import ActivityCurves
from validateAnalysisFolder import validateAnalysisFolder
from ErrorNotifier import ErrorNotifier
from ActivityCurves import findBlueFrames
 

# Standard Includes
import os
import numpy as np
import time
import sys

# Matplotlib backend
import matplotlib as plt


def activityCurveGenerator(panalysis, Err, bad_folder_action = "Abort"):

    # Numpy settings
    try:
        plt.use("TkAgg")
    except:
        print("Failed to change backend to TkAgg")

    np.seterr(divide='ignore', invalid='ignore')

    """         Hardcorded parameters to replace            """

    save_session_plots = False
    show_activity_plots = True
    apply_censors = False  # Sets actual data values to nan. Better to leave them but mark censored

    """         End of hardcoded paramters to replace       """

    # Parent directory or _Analysis folder
    #panalysis = sys.argv[1]
    (panalysis,is_single_plate, is_bad) = validateAnalysisFolder(panalysis, Err, bad_folder_action)

    # If a good analysis folder cannot be found, abort
    if is_bad:
        return

    # Find all plate folders in the parent, unless a specific plate folder was supplied as panalysis
    if not is_single_plate :
        platefullfolders = verifyDirlist(panalysis,True)[0]
    else :
        platefullfolders = list()
        platefullfolders.append(panalysis)

    # Apply the session breakup method to each
    for (platefullfolder,j) in zip(platefullfolders,range(len(platefullfolders))) :

        # Update progress bar
        # print("Starting plate ", j+1, " of " ,len(platefullfolders))
       

        # for each plate full folder find the session files
        sessionfullnames = verifyDirlist(os.path.join(platefullfolder,"Session data"),False,"Session_","png")[0]

        # Create output arrays as dummies in case they are not created later
        AC = ActivityCurves()

        #  Preserve labels, if already present in the ActivityCurves file


        # For each session:
        i = -1 # start at -1, gets incremented to 0 on first successful round

        for sessionfullname in sessionfullnames :

            # Extract the CSV info
            Wells= getSessionData(sessionfullname,"<ROI Number>")
            Aroi = getSessionData(sessionfullname,"<ROI Activity>")
            Aann = getSessionData(sessionfullname,"<Annular Activity>")
            Aout = getSessionData(sessionfullname,"<Non-ROI Activity>")
            jump = getSessionData(sessionfullname,"<Frame Jump>")
            N_rows = getSessionData(sessionfullname,"<N_rows>")
            N_cols = getSessionData(sessionfullname,"<N_cols>")
            coords = getSessionData(sessionfullname,"<ROI Coordinates>")
            outer_frac = getSessionData(sessionfullname,"<Outer Fraction>")
            worm_mvmt = getSessionData(sessionfullname,"<N worm movements>")
            bulk_mvmt = getSessionData(sessionfullname,"<N bulk movements>")
            worm_frac = getSessionData(sessionfullname,"<Worm Fraction>")

            # If no worm frac variable (older datasets) just set all to zeros to disable this analysis. 
            if worm_frac.size == 1:
                worm_frac = np.zeros(shape=(1,N_rows*N_cols))

            # Validate inputs
            if not validateSessionData(Err, Aroi, Aann, Aout, jump, N_rows, N_cols, coords, sessionfullname,worm_mvmt,bulk_mvmt) :
                Err.notify("PrintRed","Error analyzing session:\n\t" + platefullfolder + "\n\tCould not validate session data csv file")
                continue

                
            # Get the areas of an ROI, an Annulus, 
            ro = coords[3,0]
            ri = coords[2,0]
            Area_roi = 3.14159 * ri**2
            Area_ann = 3.14159 * (ro**2 - ri**2)
            Area_all_rois = N_cols*N_rows*3.14159 * (ro/outer_frac)**2

            # Estimate the area of the entire image assuming the wells were roughly centered in the image
            lhs_gap = np.min(coords[0,:],axis = 0)
            upper_gap = np.min(coords[1,:],axis = 0)
            img_width_est = np.max(coords[0,:],axis = 0) + lhs_gap
            img_height_est= np.max(coords[1,:], axis = 0) + upper_gap
            img_area_est = img_width_est * img_height_est

            # Estimate the non-ROI area and validate it
            Area_non_roi = img_area_est - Area_all_rois
            if Area_non_roi <= 0 :
                Err.notify("PrintRed","Error analyzing session:\n\t" + platefullfolder + "\n\tNon-ROI area is <= 0")
                continue

            # Passed all validations - now increment i (starts at 0)
            i += 1

            # normalize all activity measurements to the appropriate areas
            Aroi = Aroi / Area_roi
            #Aann = Aann / Area_ann
            Aout = Aout / Area_non_roi

            # Extract the session timepoint
            tp = getSessionTimepoint(sessionfullname)

            # Create the output array if it doesn't exist already
            if i == 0 :
                AC.initializeArrays(N_rows,N_cols,len(sessionfullnames),platefullfolder)

            # Find the blue light-affected frames, if any, otherwise it remains nan by default
            AC.blue_idx[i], AC.blue_excl_start[i] = findBlueFrames(Aout,jump)


            # Add the timestamp to the lists and calculate
            AC.timepoints.append(tp[0])
            AC.timepoints_str.append(tp[1])
            if i==0:
                AC.time[i] = 0
            else :
                elapsed_time = AC.timepoints[i]-AC.timepoints[0]
                AC.time[i] = round(elapsed_time.total_seconds() / 3600 / 24,2)

            # Add stimulated data for this timepoint to output arrays, unless there's no blue light
            if not np.isnan(AC.blue_idx[i]) : 
                bi = int(AC.blue_idx[i])
                bes = int(AC.blue_excl_start[i])

                AC.Aroi_stim[i,:] = np.mean(Aroi[bi:,:],axis=0)
                #AC.Aann_stim[i,:] = np.mean(Aann[bi:,:],axis=0)
                AC.Aout_stim[i] = np.mean(Aout[bi:,:],axis=0)

            else : 
                bi = None
                bes = Aroi.shape[0]
        
            # Add unstimulated data
            AC.Aroi_unstim[i,:] = np.mean(Aroi[: bes,:],axis=0)
            #AC.Aann_unstim[i,:] = np.mean(Aann[: bes,:],axis=0)
            AC.Aout_unstim[i] = np.mean(Aout[: bes,:],axis=0)

            # Add up the TOTAL number of bulk movements in this session (should be 1 per well if blue light)
            AC.N_bulk_mvmts[i,:] = np.sum(bulk_mvmt,axis=0)

            # Determine the MAXIMUM (95%ile to reduce noise) worm movements in this session
            AC.N_worm_mvmts[i,:] = np.round(np.percentile(worm_mvmt,95,axis=0))

            # Record the fraction of worms detected in this session. 
            AC.worm_frac[i,:] = worm_frac

            # Plot the session's data if requested (slows analysis down alot)
            if save_session_plots :
                plotSessionData(sessionfullname, Aroi, Aann, Aout, bes, bi)


        # Abort further analysis of this plate if no sessions
        if len(sessionfullnames) <= 5 :
            Err.notify("PrintRed","Error analyzing plate:\n\t"+platefullfolder+"\n\tThe total number of sessions is: "+ str(len(sessionfullnames))+ "\n\tSkipping...")
            continue

        # Find bad sessions - irrelevant
        # AC.markBadSessions(apply_censors)

        # Remove outlier sessions
        AC.medianFilterData()

        # Determine whether this is generally a stimulated or unstimulated experiment 
        frac_unstim_sessions = np.mean(np.isnan(AC.blue_idx))
        is_unstim_expt = frac_unstim_sessions == 1.0
        is_stim_expt = frac_unstim_sessions < 0.25
        
        if not is_unstim_expt and frac_unstim_sessions > 0.05:
            Err.notify("Print","WARNING analyzing plate:\n\t"+platefullfolder+"\n\tThe fraction of unstimulated sessions is: "+ str(frac_unstim_sessions)+ "\n\tExpected either all unstimlated or mostly stimulated. Unstimulated sessions will be skipped.")

        #elif not is_unstim_expt and not is_stim_expt : 
        #    Err.notify("Print","Error analyzing plate:\n\t"+platefullfolder+"\n\tThe fraction of unstimulated sessions is: "+ str(frac_unstim_sessions)+ "\n\tExpected either all unstimlated or mostly stimulated.\n\tPlate will be skipped.")
        #    continue

        # If this is deemed a stimulated experiment, go back and interpolate the stimulated activity values of missing datapoints
        avg_activity = np.nanmean(AC.Aroi_stim,axis=1).flatten()
        all_good_idx = np.argwhere(np.invert(np.isnan(AC.blue_idx.flatten())) & (avg_activity > 0) & (np.isnan(avg_activity) == 0))

        if is_stim_expt:
            for snum in range(0,len(sessionfullnames)):

                # If not good, interpolate
                if np.isnan(AC.blue_idx[snum]):

                    # Get next good index
                    last_good_idx = all_good_idx[all_good_idx < snum]
                    next_good_idx =all_good_idx[all_good_idx > snum]

                    if last_good_idx.size > 0 and next_good_idx.size > 0:
                        AC.Aroi_stim[snum,:] = (AC.Aroi_stim[last_good_idx[-1],:] + AC.Aroi_stim[next_good_idx[0],:]) / 2

                    elif last_good_idx.size > 0:
                        AC.Aroi_stim[snum,:] = AC.Aroi_stim[last_good_idx[-1],:]

                    elif next_good_idx.size > 0:
                        AC.Aroi_stim[snum,:] = AC.Aroi_stim[next_good_idx[0],:]

                    else:
                        aalsdj = "no valid neighbors..."



        # Calculate noise-floor subtracted activity curves (if mostly blue lightsessions ) or just use unstimulated (if not)
        if is_stim_expt : 

            # Find the last 5 good sessions. Since median filtering was applied, should just be the last 5 sessions. 
            idx_good_sessions = np.argwhere(np.invert( AC.bad_sessions))
            idx_good_sessions = idx_good_sessions[:,0] # First column only
            N_good_sessions = idx_good_sessions.shape[0] 
            N_last_5 = min((N_good_sessions,5))
            idx_last_5 = idx_good_sessions[-N_last_5:]

            # Peform subtractions
            AC.Aroi_subtr = AC.Aroi_stim - np.mean(AC.Aroi_unstim[idx_last_5,:],axis=0)
            #AC.Aann_subtr = AC.Aann_stim - np.mean(AC.Aann_unstim[idx_last_5,:],axis=0)
            AC.Aout_subtr = AC.Aout_stim - np.mean(AC.Aout_unstim[idx_last_5,:],axis=0)

        else :
            AC.Aroi_subtr = AC.Aroi_unstim
            #AC.Aann_subtr = AC.Aann_unstim
            AC.Aout_subtr = AC.Aout_unstim

        # Calculate cumulative activity curves and T levels
        #try:
        AC.calculateCumulativeAct()
        AC.calculateTx(AC.Tlevels,AC.Tx)
        AC.calculateTx(AC.Tlevels99,AC.Tx99)

        # Calculate activity per worm fraction, if available. Worm fractions of zero should be treated as no activity
        AC.act_per_worm_frac = np.true_divide(AC.Aroi_subtr,AC.worm_frac)
        AC.act_per_worm_frac[AC.worm_frac == 0] = 0

        # Find Bad curves
        AC.markBadWellsByShape(apply_censors)

        # Save curves to disk
        AC.printActivityCurves(platefullfolder,is_stim_expt,Err)

        # Plot all curves
        if show_activity_plots :
            AC.plotActivityCurves(platefullfolder,j==0,apply_censors)
        #except:
        #    print("FAILED to generate activity curves for plate: ", platefullfolder)

        B=1


# Script run capability
if __name__ == "__main__" :
    Err = ErrorNotifier()
    activityCurveGenerator(r"H:\RNAi_Project\_Analysis",Err)

