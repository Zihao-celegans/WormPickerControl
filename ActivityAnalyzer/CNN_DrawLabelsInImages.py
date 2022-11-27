"""
    CNN_DrawLabelsInImages.py
    formerly named: CNN_LabelContaminationManually.py

    Displays each image to the user, draw ROIPOLY style masks around regions of interest,
    for example contamination in a well. 

    The mask files are saved to the same folder with the replacement extension appended to the end.

    You can pass command line arguments --extension= and --mrcnn_format when running the program.
        --extension=<new_extension> will change the appended extension to the new images from the default of "_contmask" to whatever you specify
        --mrcnn_format will change the output images so they are in the proper format for our mask_rcnn training data.

    You can then use CNN_ExportFromLabeledFrames.py (formerly CNN_exportPickBackgroundV2.py)
    to chop the images up into training and testing images

    Press SPACE to finish the current contour.
    Press ESC to move to the next image
    Press Backspace to remove the last point

"""

from verifyDirlist import *
from roipoly_adf import *
import tkinter as tk
import tkinter.filedialog
import sys
import getopt

passed_args = sys.argv[1:]

replacement_extension = '_contmask'
mask_rcnn_format = False
gender_labels = False

try:
    opts, args = getopt.getopt(passed_args, "", ["extension=", "mrcnn_format", "gender_labels"])
except getopt.GetoptError:
    print('Urecognized argument passed. Allowable arguments are: "extension=" and "mrcnn_format="\nYou can use these arguments by passing them in the command line when running the program like so:\n\t"python CNN_DrawLabelsInImages.py --extension=<my_extension> --mrcnn_format"\nIf the mrcnn_format flag is passed then the output images will be formatted appropriately for the Mask_rcnn training data. If it is absent the output images will be the normal black images with white masks.')
    sys.exit()
for opt, arg in opts:
    if opt == "--extension":
        replacement_extension = arg
    elif opt == "--mrcnn_format":
        #if arg.upper() == "TRUE":
        mask_rcnn_format = True
    elif opt == "--gender_labels":
        #if arg.upper() == "TRUE":
        gender_labels = True

# Script - load all images and work on them
pimg = r'C:\Dropbox\FramesData\FramesForAnnotation\train\Renamed'

# Select folder containing the images to label
root = tk.Tk()
root.withdraw()
pimg = tk.filedialog.askdirectory(parent=root, initialdir=pimg, title='Please select the images folder')


print("\n\n######################## WELCOME TO THE MASK LABELING PROGRAM! ########################\n")
if mask_rcnn_format:
    print("FORMAT:\n\tMask RCNN - The masks you label will be saved in the format compatible with mask rcnn training data.")
else:
    print("FORMAT:\n\tNormal - The masks you label will be saved in the normal black and white format. (NOT compatible with MASK RCNN training.)")


print("\nINSTRUCTIONS:\n\tClick inside the image to draw contours around objects you would like to mask.\n\tPress backspace to delete a point.\n\tPress space to finalize a contour (The contour you created will be filled in red.)\n\tPress x to delete the most recent contour.\n\tPress esc to finalize the image, save the mask, and move on to the next image.")
if mask_rcnn_format:
    print("\n\tEach time you complete a mask you will need to provide a label for it by selecting\n\tone from the list that appears in the command prompt.")
print("\n\tIf you would like, you can exit the program early (before you've labelled every image)\n\tby selecting this command prompt and pressing Ctrl+C.")

input("\nSelect this command prompt and press enter to begin labeling images...")


extension = ".png"
#replacement_extension = '_contmask'

dimg, dshort = verifyDirlist(pimg,False,extension, replacement_extension)
if len(dimg) == 0:
    extension = ".jpg"
    dimg, dshort = verifyDirlist(pimg,False,extension, replacement_extension)

# Check if already done, if not do it
for (imgname, short_name) in zip(dimg,dshort):

    # Check if contmask exists
    ftest = short_name.replace(extension, replacement_extension + '.png')
    test = len(verifyDirlist(pimg,False,ftest)[0]) > 0

    if test:
        print('Skipping - already annotated image: ', imgname)
        continue

    # Otherwise do the annotation
    annotateImage(imgname,extension, replacement_extension + '.png', mask_rcnn_format, gender_labels)





