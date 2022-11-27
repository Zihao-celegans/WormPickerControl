"""
Hongjing Sun
Created in Sep 2019

"""

import csvio
import seaborn as sns
import math
import matplotlib.pyplot as plt; plt.rcdefaults()
import numpy as np
import matplotlib.pyplot as plt
import os
from verifyDirlist import verifyDirlist
import csv
import math
import numpy as np
import copy
import pandas as pd
from itertools import zip_longest


global finalGroup, finalArray

# Put data form different files (stored as different arrays) into one array   
def put_together(list):
    if len(list) != 1: # Check if the list contains a single element; if so, no more adding needed
        newArray = list[0]
        for i in range(1,len(list)):
            newArray = np.append(newArray, list[i], axis = 1)
        return newArray
    else: 
        newArray = list[0]
        return newArray

# Find the nearest value in a list and return its index
def find_nearest(array, value):
    array = np.asarray(array)
    idx = (np.abs(array - value)).argmin()
    return idx
   
# Read the Tx values for x (one row)
def find_lifespan(x, x_list, array):
    for i in range(len(x_list)): # Pick out the specific row
        if x_list[i] == x:
            index = i
            break
    lifespan_row = array[index]
    lifespan_row = list(filter(None, lifespan_row))
    lifespan_num = [float(numeric_string) for numeric_string in lifespan_row] # Convert to float type for further calculation
    return lifespan_num

# Read the total activity values for selected day or maximum day value
def find_totalAct(x, x_list, array):
    if x > max(x_list):
        lifespan_row = array[-1]
        lifespan_row = list(filter(None, lifespan_row))
        lifespan_num = [float(numeric_string) for numeric_string in lifespan_row]
    
    else:
        # Pick out the specific row with the closest value
        index = find_nearest(x_list, x)    
        lifespan_row = array[index]
        lifespan_row = list(filter(None, lifespan_row))
        lifespan_num = [float(numeric_string) for numeric_string in lifespan_row] # Convert to float type for further calculation
    return lifespan_num


# A function to calculate the mean of a list
def average(lst): 
    return sum(lst) / len(lst) 

# Function to calculate sample standard deviation.
def SSD(arr) : 
    sm = 0
    n = len(arr)
    if n == 1:
        return 0
    else:
        for i in range(0,n) : 
            sm = sm + (arr[i] - average(arr)) * (arr[i] - average(arr)) 
   
        return (math.sqrt(sm / (n - 1))) 
   
   
# Function to calculate sample error. 
def sampleError(arr) : 
    n = len(arr)
    # Formula to find sample error. 
    return SSD(arr) / (math.sqrt(n)) 

# A function to create empty list of desired length
def createlist(length):
    mylist = []
    for i in range(length): 
        mylist.append(i)
    return mylist

# Identify each group name and calculate the mean of the group associated data
def group_average(group_name, array):
    
    Output = [[],[],[]]                     # The values on x&y axis
    grouping = np.unique(group_name)        # Identify how many unique group names, produce a list of unique group names
    group_origin = copy.deepcopy(grouping)  # Keep the one containing censor group
    dot = []                                # Individual value
    groupList = grouping.tolist()           # Covert to list to use remove()
    
    for name in grouping: 
        if "censor" in name.casefold():     # Case insensitivity
            groupList.remove(name)          # Not show Censor group          
    grouping = np.asarray(groupList)
    
    for index in range(len(grouping)):      # Go through group names one by one
        A = [grouping[index]]               # The group name to be compared with
        A_tx = []                           # Initial element in group average list, will be replace
        for i in range(len(group_name[0])):             # Compare each element to group them
            if math.isnan(array[0,i]):                  # Do not add it into the list if nan
                continue
            if group_name[0,i] == grouping[index]:      # If one element can be matched to the first one in the list, add it into the existing group and skip to the next one
                A_tx.append(array[0,i])
        
        # Check if the whole group only contain data Nan
        if not A_tx:
            continue
        
        dot.append(np.asarray(A_tx))
        mean_A = average(A_tx)
        std_error = sampleError(A_tx)
        Output[0].append(A[0])
        Output[1].append(mean_A)
        Output[2].append(std_error)
    
    # When there is no useful data in the file
    if not Output[0]: 
        return Output

    # Plotting
    x = np.array(Output[0])
    y = np.array(Output[1]) 
    e = np.array(Output[2])
    
    
    X = [Output[0][i] for i, data in enumerate(dot) for j in range(len(data))] 
    Y = [val for data in dot for val in data]
    
    a = Y
    sns.set(style="darkgrid")
    group = ["{k}".format(k=k) for k in X]
    df  =pd.DataFrame({"data":a, " ":group})

    palette = sns.dark_palette("blue", reverse=False,  n_colors=len(grouping) )
    a=sns.swarmplot(x=' ', y = 'data' ,s=6.8, data = df, palette=palette) # Scatter point
    sns.boxplot(x=' ', y = 'data', data=df)
    
    a.set_xticklabels(a.get_xticklabels(), rotation=45, fontsize=8)
    plt.show()
    
    return Output

# Find the associated parameter list of the desired header
def find_param_name(fullfilename, data_name):
    with open(fullfilename) as csv_file:
        csv_reader = csv.reader(csv_file, delimiter=',')

        row_num = 0
        for row in csv_reader:
            row_num+=1                          # Identify the row containing the header
            for i in range(len(row)):           # Check each element in the row
                if data_name in row[i] :
                    H = row[i]
                    H_index = i                 # Slice starting point
                    break
                else:
                    H = "No_header"
            for i in range(len(row)):           # Check each element in the row
                if "<" in row[i] :              # Find the parameter name
                    param_name = row[i]
                    break
            if not "No_header" in H:
                break                           # Stop reading when header's row found
        
        # Open the file and find the indices matching the requested header
    with open(fullfilename) as csv_file:
        csv_reader = csv.reader(csv_file, delimiter=',')
        row_num -= 1
        start_row = copy.deepcopy(row_num)      # Scanning begins at specific row, to avoid repeated header
        # Scan through the lines looking for the right header
        start_reading = -1
        stop_reading = -1
        # Create a list of lists to temporarily store the values
        A = list()
        # Populate the list of lists from the file
        count = 0
        for row in csv_reader:
            if count < start_row:
                count += 1
                continue
            # Increment row number
            row_num+=1
            # Check for stop reading (any other header), if so abort reading
            S = "00"
            if start_reading >= 0 :
                for i in range(len(row)):       # Check each element in the row. If no header, continue reading
                    if "<" in row[i]:
                        S = "h_exist"
                        break
                    else:
                        S = "No_h"
                stop_reading = row_num          # Python slicing not inclusive of last item
            if "h_exist" in S:
                break

            # Check for requested header to start reading, if so skip to next line and read
            for i in range(len(row)):           # Check each element in the row
                if param_name in row[i] :
                    H = row[i]
                    start_reading = row_num+1
                    H_index = i                 # Slice starting point
                    if len(row) > 1:            # Check if the "hear" row is readable
                        for j in range(H_index+1,len(row)): # Instead of reading the whole row, reading part of it
                            if "<" in row[j]:
                                slice_end = j
                                break
                            else:
                                slice_end = len(row)
                    else:
                        slice_end = 000         # Value to be assigned later
                    break
                else:
                    H = "No_header"


            if param_name in H: # Skip to the next row and start reading. This header row should not be read
                continue
            
            # Are we reading now?
            if start_reading < 0 :
                continue

            #------------------ Reading valid portion of file -------------------#
            B = list()
            
            if slice_end == 000:
                slice_end = len(row)                  # assign slice_end value by length of the data row, instead of the header row
            
            row_slice = row[H_index:slice_end]
            row_slice = list(filter(None, row_slice)) # Eliminate empty element
            for v in row_slice :
                if csvio.is_number(v):                      # If v is a string, then int(v) won't work out 
                    if not not v and not "." in v:
                        if math.isnan(float(v)):
                            B.append(float(v))
                        else:
                            B.append(int(v))          # No decimal point, import integer
                    elif not not v :
                        B.append(float(v))            # Yes decimal point, import float
                else:
                    B.append(str(v))
            A.append(B)


    # Convert list of lists to numpy array

    # Determine whether data was read successfully, if so convert list of lists to numpy array
   
    if start_reading >= 0 and stop_reading > start_reading :
        C = np.asarray(A)
    else :
        C = np.zeros(shape = (1,1))
        print("Failed to parse file")
  
    #If the result is a single integer, return it as an integer
    if C.shape == (1,1):
        ele = C[0,0]
        try:                      # If the result is empty or a string
             if int(ele) == ele : # It's an integer. 
                C = int(ele)
             else :
                C = float(ele)

        except:
            C = "empty"
  
    return C

def all_param(plate_folders, data_name):
    A =[]
    max_A = []
    A_lst = []
    for plate_folders in plate_folders: 
        csv_file = verifyDirlist(plate_folders,False,".csv")[0]
        param_name = find_param_name(csv_file[0], data_name)      
        A.append(param_name)
        max_A.append(max(param_name))
        
        A_lst.append([row[0] for row in param_name])
    max_A = max(max_A)
    # All the common parameters from all plates
    if len(plate_folders) > 1:
        result = set(A_lst[0])
        for s in A_lst[1:]:
            result.intersection_update(s)
    else:
        result = A
    return [A,max_A,result]



# Find all desired files in parent file and plot for all groups
# pparent is the parent file selected, x is the parameter chosen by user
# data_name is the header of data type to be analyse, e.g. "<Tx>" 
def allDatafromPlates(plate_folders, x, data_name):

    # Create empty lists to store data read from file
    percent = []
    data = [] 
    group_name = []
    for plate_folders in plate_folders:                                         # Go through every file in the folder to pick up the matched one
        data_list = []
        csv_file = verifyDirlist(plate_folders,False,".csv")[0]
        param_name = find_param_name(csv_file[0], data_name)                    # The whole column of the desired parameter
        data_array = csvio.readArrayFromCsv(csv_file[0],data_name)
        if "Tx" in data_name:
            selected_data = find_lifespan(x, param_name, data_array)
        if "activity" in data_name:
            selected_data = find_totalAct(x, param_name, data_array)  
       
        data_list.append(selected_data)
        selected_data = np.asarray(data_list)
        data.append(selected_data)
        
        group_name_b =[]
        group_name_a = csvio.readArrayFromCsv(csv_file[0],"<Group name>")   # Extract the desired session in this file
        
        group_name_a = list(filter(None, group_name_a[0]))
        group_name_b.append(group_name_a)
        group_name_array = np.asarray(group_name_b)
        #for i in range(len(group_name_array[0])):                                      # If no group name assigned for a particular one
            #if not group_name_array[0][i]:
                #group_name_array[0][i] = "NoGroup"
        
        if "empty" in group_name_array:                                         # The file has no group assigned; assign as no group for each
            if "Tx" in data_name:
                group_name_array = ["No group" for i in range(len(find_lifespan(x, param_name, data_array)))]
            if "activity" in data_name:
                group_name_array = ["No group" for i in range(len(find_totalAct(x, param_name, data_array)))]
            group_name_array = np.asarray([group_name_array])
        group_name.append(group_name_array)          
    
    finalArray = put_together(data)
    finalGroup = put_together(group_name)                    # Merge arrays into a large one
    
    return [finalGroup, finalArray]

# Plotting the grouped data
def plotGroupAverages(plate_folders, x, data_name):
    finalGroup = allDatafromPlates(plate_folders, x, data_name)[0]
    finalArray = allDatafromPlates(plate_folders, x, data_name)[1]
    A = group_average(finalGroup, finalArray)

    return A

# Create a different csv file to outwrite desired data
# parameter_entered should be a list containing all number user entered, [[Tx number],[Activity number]]
def data_csv(pparent, selected_plate, fullpathname, parameter_entered):
 
    # Data to be printed out for this plotting
    
    wellNum = [j for i in range(len(selected_plate)) for j in range(1,25)]              # Wells in each plate
    plateName = [selected_plate[i] for i in range(len(selected_plate)) for j in range(1,25)]
    plateNum = [i for i in range(1, len(selected_plate)+1) for j in range(1,25)]
    plateNum.append("<end>")                                                            # Add a <end> so that the data is readable
    path = os.path.join(pparent,"_Analysis","All Data "+str(len(selected_plate))+" plates.csv")     # Full path name of the csv file
    group_name = allDatafromPlates(fullpathname, 0.97, "<Tx>")[0][0]
    groupName = group_name.tolist() 

    # Create a new csv file if it's the first time plotting
    if not os.path.exists(path):
         
        d = [plateNum, plateName, wellNum, groupName]                    
        header = ["<plateNum>", "<plateName>", "<wellNum>", "<group>"]
        # Lifespan
        for i,parameter in enumerate(parameter_entered[0]):
            percent_value = int(parameter*100)
            param_data = allDatafromPlates(fullpathname, parameter, "<Tx>")[1][0]
            d.append(param_data)
            header.append("<T"+str(percent_value)+">")
        # Total activity
        for i,parameter in enumerate(parameter_entered[1]):
            param_data = allDatafromPlates(fullpathname, parameter, "<ROI cumulative noise-floor subtracted activity>")[1][0]
            d.append(param_data)
            header.append("<TotAct"+str(parameter)+">")

        export_data = zip_longest(*d, fillvalue = '')
        # Writing to the csv file
        with open (path,'w',newline='') as myfile:
            wr = csv.writer(myfile)

            wr.writerow(header)
            
            wr.writerows(export_data)
    
    # Same file name exists
    else:
        # Overwrite the old file if same number of plates chosen (different plates)
        plates_old = csvio.readArrayFromCsv(path, "<plateName>")
        plates_old = np.unique(plates_old)
        plateName_unique = np.unique(plateName)
        if not np.array_equal(plates_old, plateName_unique):
            d = [plateNum, plateName, wellNum, groupName]                    
            header = ["<plateNum>", "<plateName>", "<wellNum>", "<group>"]
            # Lifespan
            for i,parameter in enumerate(parameter_entered[0]):
                percent_value = int(parameter*100)
                param_data = allDatafromPlates(fullpathname, parameter, "<Tx>")[1][0]
                d.append(param_data)
                header.append("<T"+str(percent_value)+">")

            for i,parameter in enumerate(parameter_entered[1]):
                param_data = allDatafromPlates(fullpathname, parameter, "<ROI cumulative noise-floor subtracted activity>")[1][0]
                d.append(param_data)
                header.append("<TotAct"+str(parameter)+">")
            
            export_data = zip_longest(*d, fillvalue = '')
            # Writing to the csv file
            with open (path,'w',newline='') as myfile:
                wr = csv.writer(myfile)

                wr.writerow(header)
            
                wr.writerows(export_data)
        
        # If file already existed for the same plates, add the new data column to it
        else:
            for i,parameter in enumerate(parameter_entered[0]):
                percent_value = int(parameter*100)
                array_list = allDatafromPlates(fullpathname, parameter, "<Tx>")[1][0]
                data = array_list.tolist()
                df = pd.read_csv(path)                  # Read from the existing file

                new_column = pd.DataFrame({"<T"+str(percent_value)+">": data})
                df = df.merge(new_column, how='left', left_index = True, right_index = True) # Add the new data the the existing one
                df.to_csv(path, index=False)            # Write to csv file
            
            for i,parameter in enumerate(parameter_entered[1]):
                array_list = allDatafromPlates(fullpathname, parameter, "<ROI cumulative noise-floor subtracted activity>")[1][0]
                data = array_list.tolist()

                df = pd.read_csv(path)
                new_column = pd.DataFrame({"<TotAct"+str(parameter)+">": data})
                df = df.merge(new_column, how='left', left_index = True, right_index = True)
                df.to_csv(path, index=False)


if __name__ == "__main__":
    pparent = "F:\\test_WI"
    panalysis = os.path.join(pparent,"_Analysis")
    plate_folders = verifyDirlist(panalysis,True)[0]
    plate_folders = [r"F:\test_WI\_Analysis\WI_2"]
    #A = plotGroupAverages(plate_folders, 0.86, "<Tx>")
    data_csv(pparent, ["WI_2"], plate_folders, [[0.66,0.89],[5,100]])
    
   



