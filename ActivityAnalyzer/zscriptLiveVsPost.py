from verifyDirlist import verifyDirlist
from ActivityCurves import findBlueFrames
from getSessionData import getSessionData
import numpy as np
import scipy.stats
import matplotlib.pyplot as plt


""" HELPER FUNCITONS """
def getPostBlueAcitivity(sessionfullname,jump) :

    # Load ROI activity from each CSV and extract the average post-blue activity
    Aout = getSessionData(sessionfullname,"<Non-ROI Activity>")
    Aroi = getSessionData(sessionfullname,"<ROI Activity>")
    i1,i0 = findBlueFrames(Aout,jump)

    # Get the average activity after the blue light
    Aroi_stim = np.mean(Aroi[i1:,:],axis=0)

    # Return the values
    return Aroi_stim


def liveVsPost():


    plive="E:\multi-roi-comparison\_Analysis_ann=0.7\CX_2D_1\Session data"
    ppost = "E:\multi-roi-comparison\_Analysis_ann=0.8\CX_2D_1\Session data"

    # Set the paths
    #plive= r"E:\old_RNAi_Project_Delete_Me\_Analysis (1)\CX_2D_2\Session data"
    #ppost = r"E:\old_RNAi_Project_Delete_Me\_Analysis\CX_2D_2\Session data"

    # Find live-calculated data from WW2-03
    fullnames_live, partnames_live = verifyDirlist(plive,False,"csv")

    # Find all matching post-calculated data from WW2-03
    fullnames_post = list()

    for name in partnames_live :

        # The session name could be slightly different at the minutes or seconds level. Extract up to hours, low probability of borderline miss
        namefilter = name[0:30]

        # Get matching name from the post folder
        new_name = verifyDirlist(ppost,False,namefilter)[0]

        # Make sure there is exactly one match
        if len(new_name) > 1 or len(new_name) < 1:
            # Debug point here
            print("name: ", name, " found ", len(new_name), "matches in the post folder")
            fullnames_post.append("")
            continue

        # Add it to the list
        fullnames_post.append(new_name[0])


    # For each session, calculate the final row of post-blue activity in the LIVE and POST analysis for comparison
    N_points = 24 * len(fullnames_live)
    All_live = np.zeros(shape=(0,))
    All_post = np.zeros(shape=(0,))

    for (livename,postname) in zip(fullnames_live,fullnames_post) :

        # Get the frame jump
        jump_live = getSessionData(livename,"<Frame Jump>")
        jump_post = getSessionData(postname,"<Frame Jump>")
        assert(jump_live == jump_post)


        # Get live and post stimulated activity average
        Aroi_stim_live = getPostBlueAcitivity(livename, jump_live) 
        Aroi_stim_post = getPostBlueAcitivity(postname, jump_post) 

        # Add them to a list of all
        All_live = np.concatenate((All_live,Aroi_stim_live),axis=0)
        All_post = np.concatenate((All_post,Aroi_stim_post),axis=0)

    # Scale down to get rid of extra zeros
    All_live /= 1000
    All_post /= 1000

    # Compute correlation
    slope, intercept, r_value, p_value, std_err = scipy.stats.linregress(All_post, All_live)
    xfit = np.arange(np.min(All_post),np.max(All_post))
    yfit = slope*xfit + intercept

    print("r2 = ",r_value**2)

    #  Plot results
    plt.figure(1)
    plt.plot(All_post,All_live,'ok',markersize = 2)
    plt.plot(xfit,yfit)
    plt.title("Session stim. activity means")
    plt.xlabel("Computed post", fontsize = 16)
    plt.ylabel("Computed live", fontsize = 16)
    plt.show()
    B=1

if __name__ == "__main__" :
    liveVsPost()


    
