"""
    Function port of verify_dirlist from Matlab
    Anthony Fouad
    Fang-Yen Lab
    July 2019

    Arguments:
        pathname:           string with the path
        foldersflag:        1 if you want only the folders
                            0 if you want only the files
        Filters to include / Exclude - set to empty string "" if you don't want to use it

        NOTE: Filters are NOT case sensitive

    Outputs:
        [0]:    List of the full path names of the files or folders
        [1]:    List of the file or folder names only
"""


import os
from natsort import natsorted, ns, index_natsorted


def verifyDirlist(pathname,foldersflag,includefilter = "",excludefilter= "", use_natsort = True):

    # Alternative implementation, not functional now
    #fullnames = [os.path.join(pathname,name) for name in os.listdir(pathname)           # Add each name in listdir IF
    #                if os.path.isdir(os.path.join(pathname, name)) == foldersflag       # if the path exists (is a folder or non-folder)
    #                & ((includefilter in name) | (includefilter == "" ))             # AND if the includefilter is in the nume (unless no includefilter supplied)
    #                & ((excludefilter not in name) | (excludefilter == ""))]        # AND if the exclude filter is not in the name (unless no excludefilter supplied)
    
    names = list()
    fullnames = list()


    # Return empty if this path doesn't exist
    if not os.path.isdir(os.path.join(pathname, pathname)):
        return (fullnames , names)
    try:
        for name in os.listdir(pathname):
            is_folder = os.path.isdir(os.path.join(pathname, name)) == foldersflag      # if the path exists (is a folder or non-folder)
            is_include= (includefilter.upper() in name.upper()) | (includefilter == "" )                # AND if the includefilter is in the nume (unless no includefilter supplied)
            is_not_excl=(excludefilter.upper() not in name.upper()) | (excludefilter == "")             # AND if the exclude filter is not in the name (unless no excludefilter supplied)   
            if is_folder & is_include & is_not_excl:                                    # Add each name in listdir IF above is true
                fullnames.append(os.path.join(pathname, name))
                names.append(name)
    except:
        print('WARNING: Corrupted file or directory was skipped in:\n\t',pathname)

    # Sort the names naturally
    if use_natsort:
        idx = index_natsorted(names)
        names = [names[i] for i in idx]
        fullnames = [fullnames[i] for i in idx]

    return (fullnames , names)

