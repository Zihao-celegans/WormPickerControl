
"""
    REQUIRES DATA ANALYZED USING zscriptTestContaminationStats!!!

"""


from verifyDirlist import *
from csvio import *
import os
import numpy as np
import numpy.matlib
from shutil import copyfile
import matplotlib.pyplot as plt
import cv2
import pandas as pd
import seaborn as sns
from sklearn import svm

# PArameters
ftrain = r'C:\Dropbox\WormWatcher (shared data)\Well score training\WellScores_TrainData_WW2-02_Norm.csv'
ftest = r'C:\Dropbox\WormWatcher (shared data)\Well score training\WellScores_TrainData_WW2-03_Norm.csv'


def importEqualSizeDataset(fname, label):

    
    # Load excel file
    data = pd.read_csv(fname).values
    X = data[:,0:3]
    Y = data[:,4]

    # EQUAL DATA SIZES
    idx_clean = np.argwhere(Y==0)
    idx_cont = np.argwhere(Y==1)

    N_clean = len(idx_clean)
    N_cont = len(idx_cont)

    if N_cont > N_clean:
        idx_keep = idx_cont[:N_clean-1]
        idx_all = np.append(idx_clean,idx_keep);

    else:
        idx_keep = idx_clean[:N_cont-1]
        idx_all = np.append(idx_cont,idx_keep)

    X = X[idx_all,:]
    Y = Y[idx_all]
    print(label, ' fraction contam: ',np.mean(Y))
    print(label, ' number contam: ',np.sum(Y))

    return(X,Y)

def modelAccuracy(model,X,Y,label):

    Ypred = model.predict(X)
    agree = Ypred==Y
    total = len(Y)
    accuracy = np.sum(agree)/total
    print("SVM ", label, " Accuracy: ", accuracy)
    return accuracy




# Load training data
X,Y = importEqualSizeDataset(ftrain, 'Train')

# Train Model
#model = svm.SVC(tol=0.000000001,max_iter = 1000000)
#model = svm.SVC(tol=0.000000001,max_iter = 1000000,kernel = 'poly',degree=4)
model = svm.LinearSVC(tol=0.000000001,max_iter = 1000000)
model.fit(X, Y)
modelAccuracy(model,X,Y,'Train')

# Load test data
X,Y = importEqualSizeDataset(ftest, 'Test')
modelAccuracy(model,X,Y,'Test')
