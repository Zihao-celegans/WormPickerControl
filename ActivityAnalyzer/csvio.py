""" 
    csvio.py
    Anthony Fouad
    August 2019

    Read or write CSV data with headers used to organize WormWatcher data
"""

import csv
import math
import numpy as np
import os
import ErrorNotifier
import sys
import pandas as pd

from verifyDirlist import verifyDirlist

# Helper function: Print an array to an ALREADY-OPEN CSV file
def printArrayToCsv(outfile, header, arr, col_offset = 0) : 

    # Write header
    outfile.write(header)

    # Validate shape
    if len(arr.shape) < 2 :
        print("ERROR: Invalid shape passed to printArrayToCsv")
        return

    # Write newline if not manually specified in the header. Allows user to specify custom spacing if desired 
    if not "\n" in header :
        outfile.write("\n")

    # Write array, applying column offset before first column if requested
    for row in arr : 

        for i in range(0,col_offset) :
            outfile.write(',')

        for col in row :
            outfile.write(str(col) + ',')
        outfile.write("\n")



def find_in_list_of_list(mylist, char):
    for sub_list in mylist:
        if char in sub_list:
            return (mylist.index(sub_list), sub_list.index(char))
    raise ValueError("'{char}' is not in list".format(char = char))


# Helper function: replace a variable in the CSV file. If variable does not exist, will just append it.
def replaceVariableInCsv (fullfilename,header,new_array, col_offset = 0):

    # Get the indices where the variable is
    search_header = header.replace(',','') # Search header does not get increment commas, if any
    start_reading, stop_reading = readArrayFromCsv(fullfilename,search_header,True) 

    # Return immediately if failed to find it, we're just going to add it to the end. No need to throw error.
    #if (start_reading < 0) or (stop_reading < 0):
    #    print("ERROR: Failed to delete variable :", header, " from: ", fullfilename)
    #    return False

    # Shift up to include header in overwrite but not next header 
    start_reading -=1
    stop_reading -=2 


    # Read the entire CSV contents, except for the to-be-delete variable
    with open(fullfilename) as csv_file:

        # Open CSV file
        csv_reader = csv.reader(csv_file, delimiter=',')
        all_data = list()

        # Read rows
        row_num = -1
        for row in csv_reader:

            # Increment row number
            row_num+=1

            # If this is a to-be-deleted row, skip it
            if (row_num >= start_reading) and (row_num <=stop_reading):
                continue;

            # If this is the <end> skip that too
            if "<end>" in row:
                continue;

            # Otherwise, add to the list to re-write
            all_data.append(row)

    # Overwrite the file with the non-deleted information, except for the END

    # Try to open the CSV file
    try:
        outfile = open(fullfilename, "w") # or "a+", whatever you need
    except IOError:
        print("ERROR: Could not open file:\n" + fullfilename + "\nPlease close Excel and try again!")
        return False

    # Open the CSV file
    with outfile :

        # add the old variable data, except for the END
        for row in all_data:
            for element in row:
                outfile.write(element + ',')
            outfile.write('\n')

        # Add the new variable data
        printArrayToCsv(outfile,header,new_array,col_offset)

        # Add the end
        printEndToCsv(outfile)

    
    return True



def printEndToCsv(outfile) :

    outfile.write("<end>\n")



# Read specific array from file
def readArrayFromCsv(fullfilename,header,return_idx_instead = False) :

    start_reading = -1
    stop_reading = -1
    start_col = -1
    stop_col = -1
    row_num = 0

    with open(fullfilename) as csv_file:

        # Scan through the lines looking for the right header
        csv_reader = csv.reader(csv_file, delimiter=',')

        
        # Create a list of lists to temporarily store the values
        A = list()

        # Populate the list of lists from the file
        for row in csv_reader:

            # Increment row number
            row_num+=1

            # Check for stop reading (any other header) if we are already reading, if so abort reading
            if start_reading >= 0 and hasAnyHeader(row):
                stop_reading = row_num
                break

            # Check for start reading, if so, get specific column to start reading from
            if header in row:

                # Set start reading
                start_reading = row_num

                # Set the start column equal to the index where the desired header was foudn
                start_col = row.index(header)

                # Find any other headers to the right of start_col
                all_headers = [i for i, x in enumerate(row) if "<" in x] # Finds all headers
                right_headers = [i for i in all_headers if i > start_col]

                if len(right_headers) == 0:
                    stop_col = 9999 # actual size needs to be set on next line
                else:
                    stop_col = right_headers[0]

                # Skip to the next row and start reading. This header row should not be read
                continue

            # Continue if not reading
            if start_reading < 0 :
                continue

            #------------------ Reading valid portion of file -------------------#

            # Find array end if not already known - it's the last nonempty cell
            if stop_col > 1000:
                stop_col = len(row) 
                for i in range(stop_col,start_col,-1):                    
                    if row[i-1] == "":
                        stop_col = i-1
                    else:
                        break
            

            # Extract valid portion of row
            row_slice = row[start_col:stop_col]

            # Read the row element-by-element to determine the variable type and add it to the list of rows
            B= list()
            for v in row_slice :
                vtest = str2number(v)
                if vtest == None:
                    vtest = str(v)
                B.append(vtest)
            A.append(B)


    # Convert list of lists to numpy array

    # Header is not found in the whole file
    if start_reading < 0:    
        C = [["empty"]]
        C = np.asarray(C)

    # Determine whether data was read successfully, if so convert list of lists to numpy array    
    elif start_reading >= 0  : # and stop_reading > start_reading, removed by ADF 11/13 so that files without <end> still parse
        C = np.asarray(A)
        #If the result is a single integer, return it as an integer
        if C.shape == (1,1):
            ele = C[0,0]
        
            if int(ele) == ele : # It's an integer. 
                C = int(ele)
            else :
                C = float(ele)
    
    # If start reading >=0 but stop_reading < start reading (could happen if failed to find end)
    else :
        C = np.zeros(shape = (1,1))
        print("Failed to parse file")
 
    # Depending on what was requested, return either the array or the indices at which it is found
    if return_idx_instead:
        return (start_reading,stop_reading)
    else:
        return C

# Find all WormWatcher CSV data fies (Activity curves only, not individual session files)
def findAllActivityCurveCsv(panalysis, Err) : 
    
    # Enforce that this is an _Analysis path or _Analysis subpath
    (p,head) = os.path.split(panalysis)

    # Analysis folder: Proceed with finding subfolders
    if "_Analysis" in head :
        
        plate_folders = verifyDirlist(panalysis,True)[0]

    # Individual plate folder: Only use this one folder
    elif "_Analysis" in p :

        plate_folders = list()
        plate_folders.append(panalysis)

    else :
        Err.notify("Abort","Path: " + panalysis + " must be an individual plate folder or an _Analysis folder")
        

    # Find activity curve CSV files within the plate folder and add all of them to a list
    all_activity_curves_csv = list()
    for plate_folder in plate_folders : 

        # Search for csv files in the plate folder
        csv_files = verifyDirlist(plate_folder,False,".csv","Session")[0]

        # Make sure there are not multiple files
        if len(csv_files) > 1:
            Err.notify("Abort","Plate folder: " + plate_folder + " Contains multiple CSV files. Which one is the activity curves file?")

        # Add it to the list
        elif len(csv_files) == 1 : 
            all_activity_curves_csv.append(csv_files)


    return all_activity_curves_csv
   
# Check whether the element is string or number
def is_number(s):
    try:
        float(s)
        return True
    except ValueError:
        return False


# Convert the string s to a number or else throw an error if not possible
def str2number(s,expected_type = int, inf_val = 0) :

    # if it's an inf return 0 in whatever type is expected
    if "INF" in s.upper():
        s = "0"

    # Convert to float nan if nan
    if is_number(s) and math.isnan(float(s)):
        return float(s)

    # Convert to boolean if it's numeric AND rounded AND bool is requested  (could also use    round(float(s)) == float(s)    )
    elif is_number(s) and (not ('.' in s)) and expected_type == bool : 
        return bool(int(s))

    # Convert to INTEGER if it's numeric AND rounded AND integer is requested
    elif is_number(s) and (not ('.' in s))  and expected_type == int :
        return int(s)

    # Otherwise convert to float
    elif is_number(s):
        return float(s)

    # If not numeric return None
    else:
        return None


def hasAnyHeader(row):
    headers = [i for i in row if "<" in i]
    return len(headers) > 0




# Helper function for data analysis - read plate WormCamp 24-well key from xlsx file using pandas
def findKeyInXlsx(platename,data,Nrows=4,Ncols=6):
    r0 = -1
    c0 = -1
    found = False
    duplicate = False

    for (row,r) in zip(data,range(data.shape[0])):
        for (elem,c) in zip(row,range(data.shape[1])):

            # If it's not a string, continue immediately
            if type(elem) != str:
                continue

            # Plate is found for the first time - assign r0,c0
            if (r0<0) and (platename.upper() == elem.upper()):
                r0 = r+1
                c0 = c
                found = True

            elif platename.upper() == elem.upper():
                duplicate = True


    # if found, extract its key
    if r0>=0:
        key = data[r0:r0+Nrows,c0:c0+Ncols]
    else:
        key = np.zeros((Nrows,Ncols))

    return (key,found, duplicate)







# For testing
if __name__ == "__main__":

    fname = r'F:\RNAi_project\_Analysis\C4_4C_1\ActivityCurves_C4_4C_1.csv'
    A = readArrayFromCsv(fname,"<x>")
    B = readArrayFromCsv(fname,"<Tx>")
    C = 1;