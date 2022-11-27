
import matplotlib.pyplot as plt
import numpy as np
import scipy.stats



# Jitter plot (scatter with jitter)
# https://stackoverflow.com/questions/8671808/matplotlib-avoiding-overlapping-datapoints-in-a-scatter-dot-beeswarm-plot
def rand_jitter(arr):
    stdev = .01*(np.max(arr)-np.min(arr))
    return arr + np.random.randn(len(arr)) * stdev

def jitter(x, y, s=20, c='b', marker='o', cmap=None, norm=None, vmin=None, vmax=None, alpha=None, linewidths=None, verts=None, **kwargs):
    return plt.scatter(rand_jitter(x), rand_jitter(y), s=s, c=c, marker=marker, cmap=cmap, norm=norm, vmin=vmin, vmax=vmax, alpha=alpha, linewidths=linewidths, verts=verts, **kwargs)

# Anthony's scatter plot
def scatterBestFit(x,y, s=20, use_jitter = False, c='b', marker='o', cmap=None, norm=None, vmin=None, vmax=None, alpha=None, linewidths=None, verts=None, **kwargs):

    # Flatten x and y
    x = x.flatten()
    y = y.flatten()

    # Plot the scatter with or without jitter
    if use_jitter:
        h_scatter = jitter(rand_jitter(x), rand_jitter(y), s=s, c=c, marker=marker, cmap=cmap, norm=norm, vmin=vmin, vmax=vmax, alpha=alpha, linewidths=linewidths, verts=verts, **kwargs)
    else:
        h_scatter = plt.scatter(x, y, s=s, c=c, marker=marker, cmap=cmap, norm=norm, vmin=vmin, vmax=vmax, alpha=alpha, linewidths=linewidths, verts=verts, **kwargs)

    # Generate the best fit line and plot 
    slope, intercept, r_value, p_value, std_err = scipy.stats.linregress(x, y)
    xfit = np.unique(x)
    yfit = slope*xfit + intercept    
    h_fit = plt.plot(xfit,yfit,'-r')

    # Assemble legend with r^2
    h_fit_label = "r^2 = " + str(round(r_value**2,2))
    h_legend = plt.legend([h_fit_label])


    # Return handles 
    return (h_scatter,h_fit,h_legend)


