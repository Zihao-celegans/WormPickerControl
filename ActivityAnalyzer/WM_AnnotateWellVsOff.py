


from roipoly_adf import *
from verifyDirlist import *


#pimg = r'C:\Dropbox\WormWatcher (shared data)\Wormotel simulations\SampleImages\SamplesFrom PDM AUXIN\Kernel plates'
#pimg = r'C:\Dropbox\WormWatcher (shared data)\Wormotel simulations\SampleImages\SamplesFrom PDM AUXIN\Test plates'
#pimg = r'C:\Dropbox\WormWatcher (shared data)\Wormotel simulations\SampleImages\SamplesFrom PDM AUXIN\Kernel plates (Matt)'
pimg = r'C:\Dropbox\WormWatcher (shared data)\Wormotel simulations\SampleImages\SamplesFrom PDM AUXIN\Test plates (Matt)'

images_full,images_part = verifyDirlist(pimg,False,'png','_contmask.png')

for image_full in images_full:
    annotateImage(image_full)