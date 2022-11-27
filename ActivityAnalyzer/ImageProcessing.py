import numpy as np
import scipy as sp
import scipy.ndimage

# OpenCV includes
import cv2
cv2.__version__



# Gaussian filter excluding nans
# https://stackoverflow.com/questions/18697532/gaussian-filtering-a-image-with-nan-in-python

def gaussFiltExclNan(U,sig):
    
    V=U.copy()
    V[np.isnan(U)]=0
    VV=sp.ndimage.gaussian_filter(V,sigma=sig)

    W=0*U.copy()+1
    W[np.isnan(U)]=0
    WW=sp.ndimage.gaussian_filter(W,sigma=sig)

    Z=VV/WW

    return Z



# Set an image to NaN except for a circular ROI

def circularROI(img,x,y,r,set_zero_to_nan = False) :
    height,width = img.shape
    circle_img = np.zeros((height,width), np.uint8)
    circle_img[:] = np.nan
    cv2.circle(circle_img,(int(x),int(y)),int(r),255,thickness=-1)
    one_well = cv2.bitwise_and(img, img, mask=circle_img)
    one_well = one_well.astype(np.float)
    if set_zero_to_nan:
        one_well[one_well==0] = np.nan # Set non-roi pixels to nan
    return one_well



# Remove small objects
# https://stackoverflow.com/questions/42798659/how-to-remove-small-connected-objects-using-opencv/42812226


def removeObjectsBySize(img,min_size,max_size):

    # Force the image to be uint8
    img = img.astype(np.uint8)

    #find all your connected components (white blobs in your image)
    nb_components, output, stats, centroids = cv2.connectedComponentsWithStats(img, connectivity=8)
    #connectedComponentswithStats yields every seperated component with information on each of them, such as size
    #the following part is just taking out the background which is also considered a component, but most of the time we don't want that.
    sizes = stats[1:, -1]; nb_components = nb_components - 1

    #your answer image
    img2 = np.zeros((output.shape))
    #for every component in the image, you keep it only if it's above min_size
    for i in range(0, nb_components):
        if sizes[i] >= min_size and sizes[i] <= max_size:
            img2[output == i + 1] = 255


    return img2