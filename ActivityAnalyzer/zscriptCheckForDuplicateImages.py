"""
    Scan a plate folder for Patrick's bug: duplicate images with similar file save times

"""


from verifyDirlist import verifyDirlist
import cv2
import sys


def imageScore(img) :

    (mean,stdev) = cv2.meanStdDev(img)
    return mean[0] + stdev[0]

pplate = sys.argv[1]
if len(sys.argv) > 2 :
    dayfilter = sys.argv[2]
else :
    dayfilter = ""

# Look for all day folders in the plate folder
daynames = verifyDirlist(pplate,True,dayfilter)[0];
flagged_files = list()
total_files = 0

for dayname in daynames :

    # Load all images
    imnames = verifyDirlist(dayname,False,"png","ROI")[0]
   
    for (i,imname) in enumerate(imnames) :

        total_files += 1

        # Skip if this is the first image.
        if i == 0 :
            last_val = imageScore(cv2.imread(imname))
            continue

        # Get the current image score value
        this_val = imageScore(cv2.imread(imname))

        # Flag if equal
        if this_val == last_val :
            print("File: ", imname)
            flagged_files.append(imname)

        # Set current one to last one 
        last_val = this_val

print(len(flagged_files), " out of ", total_files, "flagged")
B=1





