""" 

    Merge All Image files - for undoing a session splitup gone wrong


"""


from  verifyDirlist import *
import os
import tkinter as tk
import tkinter.filedialog

# Default start path (replaced later)
pplate = r'E:\Matt eLIfe paper data (merged)\161005_Lifespan'

# Ask user to confirm
root = tk.Tk()
root.withdraw()
pplate = tk.filedialog.askdirectory(initialdir=pplate, title='Please select the PARENT directory')
root.destroy()

# Exit immediately if no folder provided
if len(pplate) == 0:
    print("No file provided - image generator will exit")
    time.sleep(2)
    exit(0)


print('Reorganizing images - please wait...')

# Set output folder and make it
pout = os.path.join(pplate,'AllImages')
if not os.path.isdir(pout) :
    os.mkdir(pout)

# Find all image folders except the output folder
psessions, _ = verifyDirlist(pplate,True,"","AllImages")

# Loop over all session folders
for psession in psessions:

    # Find all image files in this folder
    fimages, fshorts = verifyDirlist(psession,False,"png")

    for (fimg_long, fimg_short) in zip(fimages,fshorts):

        fout = os.path.join(pout,fimg_short)
        os.rename(fimg_long,fout)


print('\nAll PNG images have been moved to:\n\t',pout,'\n\tYou may delete the remaining folders if they do not contain other important files.')