# Calculate FOV of a lens

# ----------- PARAMETERS --------------------#
h = 570         #height in mm
angular_FOV =   [14.2,10.2] #Angular FOV in degrees
# -------------------------------------------#


import numpy as np



# Convert angles to radians and halve them
angular_FOV = np.array(angular_FOV)
aFOVrad = np.radians(angular_FOV) / 2

# Calculate opposite side half-lengths
opphalf = np.tan(aFOVrad) * h
oppfull = opphalf * 2

# Print result
print("FOV is ",oppfull[0], ' x ', oppfull[1],'mm')
