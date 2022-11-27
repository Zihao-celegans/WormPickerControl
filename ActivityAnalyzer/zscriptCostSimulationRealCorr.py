"""

    Cost simulation but instead of fitting gaussians or Cauchy's, using the correlations from rescreen 1:
    1 vs 6
    2 vs 6

    etc.

    PROBLEM: not using independent data for the two samples being correlated, so OVERESTIMATING the correlations
    and also OVERESTIMATING how much adding a point improves the accuracy


"""

import numpy as np
from scipy import stats
import matplotlib.pyplot as plt

corrvals = [0.4, 0.69, 0.80, 0.89, 0.95, 1.0]
accuracy = np.zeros((6,1))

# Set plotting range
xx = np.array([12,25])
yy = np.array([12,25])
means = [xx.mean(), yy.mean()]  
stds = [xx.std() / 3, yy.std() / 3]

for (i,r2) in enumerate(corrvals) :

    # Calculate simulated true and measured LS data
    r = np.sqrt(corrvals[i])
    covs = [[stds[0]**2          , stds[0]*stds[1]*r], 
        [stds[0]*stds[1]*r,           stds[1]**2]] 
    y = np.random.multivariate_normal(means, covs, size=100000)

    lifespans = y[:,0]
    measured = y[:,1]

    # Select top 5% 
    cutoff = np.nanpercentile(measured,95)
    idx_selected = np.where(measured>cutoff)
    ls_selected = lifespans[idx_selected];

    # Find out how many of these were actually in the top 5%
    cutoff = np.nanpercentile(lifespans,95)
    ls_selected_good =  ls_selected > cutoff

    # Accuracy
    accuracy[i] = np.sum(ls_selected_good) / ls_selected.size

    # View the correlation plot to check
    plt.figure(i)
    plt.plot(lifespans,measured,'ro',markersize = 0.5)

    # Print the r squared values to check
    slope, intercept, r_value, p_value, std_err = stats.linregress(y)

    print("r^2 = ", r_value**2)




# Plot accuracy as a function of samples 
max_samples = 6
plt.figure(max_samples,figsize=(8,4))
x = np.arange(1,max_samples+1);
plt.subplot(1,2,1)
#plt.rc('font', size=16)
plt.plot(x,accuracy)
plt.xlabel("# replicates",fontsize = 14)

# Calculate # of wells screened for each # of sample
frac_selected = 0.05
N_rnais_per_month = 2000/x
N_selected_per_month = N_rnais_per_month * frac_selected
N_true_hits_per_month = N_selected_per_month * accuracy.flatten()
plt.plot(x,N_rnais_per_month/2000)
plt.legend(("Accuracy","Rel. throughput"))


# True hits per month
plt.subplot(1,2,2)
#plt.rc('font', size=16)
plt.plot(x,N_true_hits_per_month)
plt.xlabel("# replicates",fontsize = 14)
plt.legend(("True_hits_selected/Mo"))

#plt.rc('font', size=16)

plt.show()


