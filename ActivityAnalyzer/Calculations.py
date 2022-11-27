

"""     Non-Member Helper functions for reading and plotting activity data     """

import numpy as np
import matplotlib as plt

# Helper function: Identify outliers more than m-times the median of a vector
def findOutliers(Arr,m) :

    # Calculate the median
    assert Arr.shape[1] == 1, "Array passed to findOutliers must have shape of (N,1)"
    med = np.median(Arr)

    # Find points more than m-times the median
    is_bad = Arr>m*med
    return is_bad
