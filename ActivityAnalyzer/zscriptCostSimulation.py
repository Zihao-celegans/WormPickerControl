"""
    Cost simulation analysis

"""

import scipy.stats
import numpy as np
import matplotlib.pyplot as plt

#Parameters from each raw dataset from LIam's Matlab script:
# "C:\Dropbox\WormWatcher (shared data)\_old\RNAi Rescreens\_Simulations\AnalyticalSimulation.m"

std_EV = 3.7749
std_RNAi = 3.8474   # All data with EV removed. 

var_EV = std_EV**2
var_RNAi = std_RNAi**2

mean_RNAi = 17.76
median_RNAi = 17.65

HALF_IQR_EV = 3.00
HALF_IQR_RNAi = 2.47

# Estimate the Raw RNAi variance by subtracting EV variance from RNAi variance
var_RNAi_isolated = (var_RNAi - var_EV)*1
cauchy_spread_EV = (HALF_IQR_RNAi - HALF_IQR_EV)/1

""" Rules for simulation:

    Each RNAi LIFESPAN is sampled from a gaussian CENTERED at the RNAi mean with variance of var_RNAi_isolated
    Each RNAi MEASURED LIFESPAN sampled from a gaussian CENTERED at the LIFESPAN and with Variance of var_EV

"""

# NORMAL / GAUSSIAN SIMULATION Simulate 5000 wells with corresponding measurements 
rnais_screened = 25000

# CAUCHY SIMULATION

# In order to customize our Cauchy Distribution, we need to do the Inverse transform Sampling
# method
def inverted_cdf_cauchy(gamma,x0,x):
    return gamma*np.tan(np.pi*(x-(1/2))) + x0

# We use a uniform sample to complete the Inverse transform sampling method
RandomUniVals = np.random.random((rnais_screened,1))

# Get cauchy sampled lifespans
#lifespan = inverted_cdf_cauchy(cauchy_spread_EV,median_RNAi,RandomUniVals)
lifespan =  np.random.normal(mean_RNAi,np.sqrt(var_RNAi_isolated),(rnais_screened,1))

# Delete samples below 3 or greater than 50 days
idxDis = np.where(lifespan > 50)
lifespan[idxDis] = np.nan

idxDisLess = np.where(lifespan < 3)
lifespan[idxDisLess] = np.nan

# Calculate measured distributions up to 50 times
max_samples = 50
measured_lifespan = np.random.normal(lifespan,std_EV,(rnais_screened,max_samples))

# Show histograms of both distributions
plt.figure(1,figsize=(8,4))
plt.subplot(1,2,1)
binwidth = 0.1
plt.hist(lifespan,bins=np.arange(min(lifespan), max(lifespan) + binwidth, binwidth))
plt.xlim((0,35))

plt.subplot(1,2,2)
plt.hist(measured_lifespan.flatten(),bins=np.arange(min(measured_lifespan.flatten()), max(measured_lifespan.flatten()) + binwidth, binwidth))
plt.xlim((0,35))


# Calculate accuracy as a function of number of samples
accuracy = np.zeros(shape=(max_samples,1))

for i in range(1,max_samples+1) :
    slice = measured_lifespan[:,0:i]
    this_measured_lifespan = np.mean(slice,axis=1)

    # Select the top 5% outliers for rescreening etc...
    # Question: How many of those top 200 are actually outliers, i.e. actually in the top 5%?

    # Select the top 5%
    cutoff = np.nanpercentile(this_measured_lifespan,95)
    idx = np.where(this_measured_lifespan>cutoff)

    # Find the true lifespans of the selected wells
    selected_lifespans = lifespan[idx]

    # Find how many selected lifespans were actually in the top 5%
    cutoff = np.nanpercentile(lifespan,95)
    selected_lifespans_top_5 = np.where(selected_lifespans > cutoff)


    accuracy[i-1] = selected_lifespans_top_5[0].size / selected_lifespans.size


# Plot scatterplots showing the spreads

plt.figure(2,figsize=(8,4))
plt.subplot(1,2,1)
plt.plot(lifespan,measured_lifespan[:,0],'ro',markersize = 0.2)
plt.subplot(1,2,2)
plt.plot(lifespan,np.mean(measured_lifespan[:,0:49],axis=1),'ro',markersize = 0.2)
plt.rc('font', size=16)





# Plot accuracy as a function of samples 
x = np.arange(1,max_samples+1);
plt.figure(3,figsize=(8,4))
plt.subplot(1,2,1)
plt.rc('font', size=16)
plt.plot(x,accuracy)
plt.xlabel("# samples")
plt.ylabel("Accuracy")



"""
    Efficiency as a function of samples

    Units: hits per RNAi screened

"""

# Calculate # of wells screened for each # of sample
frac_selected = 0.05
N_rnais_per_month = 2000/x
N_selected_per_month = N_rnais_per_month * frac_selected
N_true_hits_per_month = N_selected_per_month * accuracy.flatten()
plt.plot(x,N_rnais_per_month/2000)
plt.legend(("Accuracy","Rel. Capacity"))

plt.subplot(1,2,2)
plt.rc('font', size=16)
plt.plot(x,N_true_hits_per_month)
plt.xlabel("# samples")
#plt.ylabel("# true hits/month")
plt.show()
B=1




