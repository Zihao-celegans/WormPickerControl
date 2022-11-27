# Use mainly tk
from tkinter import *
#from tkinter.ttk import * # Replace tkinter widgets with styled widgets
from tkinter import filedialog
from tkinter import messagebox
from PIL import ImageTk,Image
import tkinter as tk
import tkinter.scrolledtext as tkst
from tkinter import ttk
import pdb
import time

# Use a couple as TTK
from tkinter.ttk import Style
from tkinter.ttk import Progressbar
from tkinter.ttk import Button
from ReformatParentDirectory import reformatParentDirectory
from ReformatParentDirectory import verifyValidParentDirectory
from breakUpSession import breakUpSession

#Imports from Other WW Files
from ActivityCurveGenerator import activityCurveGenerator
from plotGroupAverages import plotGroupAverages
from plotGroupAverages import all_param
from plotGroupAverages import data_csv
from verifyDirlist import verifyDirlist
from PlateTreeView import PlateTreeView
from LabeledProgressbar import LabeledProgressbar
from ErrorNotifier import ErrorNotifier
from DisplaySubprocessOutput import DisplaySubprocessOutput
from validateAnalysisFolder import validateActivityCurvesFile
from censorWellsAuto import censorWellsAuto

#General Imports
import os
import subprocess
from time import sleep
from threading import Thread
import json
from manualImageInspector import manualImageInspection
import psutil
import math
import numpy as np
from csvio import is_number, str2number, readArrayFromCsv, replaceVariableInCsv
import copy
from natsort import natsorted, ns, index_natsorted

import logging

""" Global variables """
global bgcolor, atypes, helps, buttons, abort_analysis_flag, PB, treeview, selected_plates, Err

"""
    Arguments for listbox:
    [0] parent on which to base it (Tk frame or root)
    [1] Title of the listbox (string)
    [2] 
"""

################################################
#                GENERAL FUNCTIONS
################################################
def rgb2Hex(rgb):
    #translates an rgb tuple of int to a tkinter friendly color code=
    return "#%02x%02x%02x" % rgb   

def focus_next_window(event):
    event.widget.tk_focusNext().focus()
    return("break")

def select_all(event):
    event.widget.tag_add(SEL, "1.0", END)

def combine_funcs(*funcs):
    def combined_func(*args, **kwargs):
        for f in funcs:
            f(*args, **kwargs)
    return combined_func

def update_progress(progress, i):
        progress['value'] = i

################################################
#                GENERAL CLASSES
################################################

class ProgressBar:
    def  __init__(self, frame, max):
        self.max = max
        self.step = tk.DoubleVar()
        self.progbar = ttk.Progressbar

        self.frame = frame
        self.frame.pack(fill=tk.BOTH, padx=2, pady=2)
 
    def add_progbar(self):
        self.progbar = ttk.Progressbar(
            self.frame, 
            orient=tk.HORIZONTAL, 
            mode='determinate', 
            variable=self.step, 
            maximum=self.max)
        self.progbar.pack(fill=tk.X, expand=True)
 
    def start_progbar(self, step):
        self.add_progbar(step)
        self.root.update()

    def update_progbar(self, step):
        self.step.set(step)

    def destroy_progbar(self):
        self.progbar.destroy()


class listBoxScrolling(Tk):
    def __init__(self, *args, **kwargs):

        # Unpack arguments
        parent = args[0]
        title = args[1]
        pack_side = args[2]
        color_tuple = args[3]

        # Create frame for the listbox
        self.frame = Frame(parent)
        self.frame.pack(side=pack_side)
        self.frame.configure(background=rgb2Hex(bgcolor))

        # Upper label
        Label(self.frame, text=title,font =("Helvetica", 14),background=rgb2Hex(bgcolor)).pack(side="top")

        # Create the listbox
        self.listobj = Listbox(self.frame, width=20, height=20, font=("Helvetica", 12),selectmode='multiple',
                               exportselection=0,background=rgb2Hex(color_tuple))
        self.listobj.pack(side="left", fill="y")

        # Create the scrollbar
        self.scrollbar = Scrollbar(self.frame, orient="vertical")
        self.scrollbar.config(command=self.listobj.yview)
        self.scrollbar.pack(side="left", fill="y")
        self.listobj.config(yscrollcommand=self.scrollbar.set)

    def replaceList(self, vals):
        # Delete old values
        self.listobj.delete(0,'end')

        # Add new values
        for x in vals:
            self.listobj.insert(END, str(x))

class optionEntry:

    def __init__(self, *args, **kwargs) :

        # Unpack arguments
        self.parent = args[0]        # Parent frame (or root) on which to create subframe
        self.label = args[1]         # The label for this option
        self.value = args[2]         # The default value for this option
        self.bgcolorhex = args[3]   # Background color for the frame

        # Create the frame to hold both items
        self.frame = Frame(self.parent)
        self.frame.pack()
        self.frame.configure(background=self.bgcolorhex)

        # Create the label
        self.label_obj = Label(self.frame, text = self.label, height=1, width=30, font=('Helvetica',12),background=self.bgcolorhex)
        self.label_obj.pack(side="left")

        # Create the edit text
        self.value_obj = Text(self.frame, height=1, width=20, font=('Helvetica',12))
        self.value_obj.config(tabs=(1,))
        self.value_obj.insert(END, self.value)
        self.value_obj.pack(side="left")
        self.value_obj.bind("<Tab>", focus_next_window) #Tab moves to the next widget
        self.value_obj.bind("<FocusIn>", select_all) # Any entry selects all

    # Destructor cleans up all widgets
    def __del__(self, *args, **kwargs) :
        self.label_obj.pack_forget()
        self.value_obj.pack_forget()
        self.frame.pack_forget()


    # Get method, get the string value of the entry half
    def get(self):
        return self.value_obj.get("1.0",'end-1c')

# A list of optionEntY objects
class optionList():

    def __init__(self, *args, **kwargs) :
        # Fill up default list
        self.json_name = r"C:\WormWatcher\config.json"
        self.populateList(*args)
        


    # Fill up the list with the labels and widgets
    def populateList(self, *args, **kwargs):
        self.use_json = args[0] #Whether to load options from the JSON file
        bgcolorhex = args[1]
        self.input_objects_list = list()
        

        # If not using JSON, default values must be supplied as a tuple
        if not self.use_json:
            self.labels = list(args[2])
            self.values = list(args[3])

            if len(self.labels)!=len(self.values) :
                messagebox.showerror("Internal Error","WWAnalyzer internal error: labels length does not match values when parsing arguments")


        # Load the values for STEP 1 from the JSON as a LOCAL variable (erased after __init__)
        else:
            with open(self.json_name, "r") as myfile:
                json_data = json.load(myfile)
                self.labels = list()
                self.values = list()
                for key in json_data["analysis parameters"]:
                    self.labels.append(str(key))
                    self.values.append(json_data["analysis parameters"][str(key)])
            if len(self.labels)==0 or len(self.labels)!= len(self.values) :
                messagebox.showerror("Internal Error","WWAnalyzer internal error: labels has length zero OR length does not match values when loading from JSON")



        # Create the option-entry objects
        for (lbl,val) in zip(self.labels,self.values) :
            self.input_objects_list.append(optionEntry(subframe_analysis_opts,lbl,val,bgcolorhex))


    # Clear all the option entries, e.g. to switch to a different option type
    def clearList(self, *args, **kwargs) :

        # Delete the list
        self.input_objects_list.clear()

    # Save all the options, either writing to JSON or else simply returning them. 
    def saveList(self, *args) :

        # Default json dictionary to return in case of errors or non-json
        json_data = dict()


        # Grab all changes from the text fields
        for (i,val) in enumerate(self.values,start = 0):

            # Convert to appropriate number or bool
            new_str = self.input_objects_list[i].get()
            old_type = type(val)
            new_num = str2number(new_str,old_type)

            # Check that conversion was successful
            if new_num == None:
                errmsg = "The value of " + self.labels[i] + " that you entered is invalid: " + new_str
                return(self.values,errmsg,json_data)

            # Assign the number to the values list
            self.values[i] = new_num

        # If using JSON, write the changes to disk
        if self.use_json :

            with open(self.json_name, "r") as myfile:

                # Load the JSON again to refresh with the latest values before overwriting
                json_data = json.load(myfile)

                # Overwrite the keys in the analysis parameters
                for (key,val) in zip(self.labels,self.values):
                    try:
                        json_data["analysis parameters"][key] = val
                    except:
                        messagebox.showerror("Internal error","WWAnalyzer internal error: Failed to assign value to key: " + key)
                        return
                
            # Write the JSON file
            with open(self.json_name, 'w') as outfile:
                json.dump(json_data, outfile,  indent=2)

            # Extract only the analysis parameters
            json_data = json_data["analysis parameters"]

        # Regardless, simply return the settings lists with a blank error message
        return (self.values,"",json_data)
    

################################################
#                 BUTTON CALLBACKS
################################################

def dropdown_callback(*args): 

    # Print to console which option was selected
    Err.notify("Print","Selected: " + dd_var.get())

    # Sshow the appropriate options list
    selected_options.clearList()

    if atypes[0] in dd_var.get() :
        selected_options.populateList(True,rgb2Hex(bgcolor))
        

    elif atypes[3] in dd_var.get() :
        selected_options.populateList(False,rgb2Hex(bgcolor),("Level [0,1]",),(0.97,))
        

    elif atypes[4] in dd_var.get() :
        selected_options.populateList(False,rgb2Hex(bgcolor),("Timepoint (d)",),(100,))

    
    elif atypes[5] in dd_var.get(): 
        
        selected_options.populateList(False,rgb2Hex(bgcolor),("Tx 1", "Tx 2", "Total Activity",),(0.97, 0.89, 100,))

    elif atypes[6] in dd_var.get():

        headers6 = ("Min expt length (d)", "First checkpoint (d)", "\tMin fraction","\tMax fraction","Second checkpoint (d)",  "\tMin fraction","\tMax fraction","Max frac increase")

        defaults6 = (25, 3, 0.08, 0.5, 10, 0.07,0.5,1.45)
        selected_options.populateList(False,rgb2Hex(bgcolor), headers6, defaults6)


    else :
        # Default values
        selected_options.populateList(False,rgb2Hex(bgcolor),("No options necessary!",),(0,))

    # Figure out which help label to add
    idx = 0
    idx = atypes.index(dd_var.get())
    help_var.set(helps[idx])

def pushbutton_refresh_callback(*args):
    
    #Call pushbutton_path_callback with the flag to skip selection
    pushbutton_path_callback(True)

def pushbutton_path_callback(*args):
    global PB

    if len(args) > 0:
        skip_getdir = args[0]
    else:
        skip_getdir = False

    global plates, N_analyzed_sessions, N_image_sessions

    # Clear the tree before starting
    treeview.clear_tree()

    # Style
    if not skip_getdir:
        s = Style()
        s.configure('my.TButton',background = "orange")
        pushbutton_path.configure(text = "Validating...",style='my.TButton')

        # Ask the user to select a folder
        tempdir = filedialog.askdirectory(parent=root, initialdir=pparent.get(), title='Please select the PARENT directory')
        if len(tempdir) > 0:
            Err.notify("Print","Selected folder: " + tempdir)

        # If the user selected an "_Analysis" folder, go one level up to get parent folder
        if "_Analysis" in tempdir:
            tempdir,trash = os.path.split(tempdir)

        # Enforce a valid parent directory
        if verifyValidParentDirectory(tempdir,Err) :
            pparent.set(tempdir)
            s.configure('my.TButton',background = "gray")
            pushbutton_path.configure(text = "Change path")

        else:
            s.configure('my.TButton',background = "red")
            pushbutton_path.configure(text = "Change path")
            return


    #Changing directory pushbutton text 

    # -----      Populating Tree View - get all plates in both analysis and image folders       ----- #

    # Get plate image folders
    directory = pparent.get()
    directory_ana = os.path.join(directory, "_Analysis").replace("\\","/")
    plates_img_full, plates_img = verifyDirlist(directory,True,"","_Analysis")

    # Get plate analysis folders
    plates_ana_full, plates_ana = verifyDirlist(directory_ana, True,"")
   
    # Get the master list of plates
    merged_plates = list(np.unique(plates_img + plates_ana))

    # Sort the names naturally
    idx = index_natsorted(merged_plates)
    treeview.plates_master = [merged_plates[i] for i in idx]

    # -----     Determine whether each master plate has image and/or analysis folders       ----- #

    treeview.plates_img = list()
    treeview.plates_ana = list()

    for plate in treeview.plates_master:

        # Determine if there's an image folder for this plate, if so add it to the list
        if plate in plates_img:
            treeview.plates_img.append(plates_img_full[plates_img.index(plate)])

        else:
            treeview.plates_img.append("")

        # Determine if there's an analysis folder for this plate, if so add it to the list
        if plate in plates_ana:
            treeview.plates_ana.append(plates_ana_full[plates_ana.index(plate)])

        else:
            treeview.plates_ana.append("")

    # Make sure they're all the same size...

    if (len(treeview.plates_master) != len(treeview.plates_img)) or  (len(treeview.plates_master) != len(treeview.plates_ana)):
        messagebox.showerror("Internal error","Internal error: Master list does not match analysis or image lists")

    # -----     Count up the STEP 1 sessions in each image and/or analysis folder and determine whether STEP 2 has been completed   ----- #
    treeview.N_image_sessions = []
    treeview.N_analyzed_sessions = []
    treeview.Completed_STEP2 = []
    PB.setStatus(root,0,len(treeview.plates_master)-1,'Searching for data...')

    total_start = time.time()

    for (plate_img,plate_ana,i) in zip(treeview.plates_img,treeview.plates_ana,range(len(treeview.plates_master))):

        # Assemble the session
        session_csv_path = os.path.join(plate_ana,'Session data')

        # Determine image folder session counts. If blank, it's automatically gonna be len=0
        treeview.N_image_sessions.append(len(verifyDirlist(plate_img,True)[0]))
        treeview.N_analyzed_sessions.append(len(verifyDirlist(session_csv_path,False,'.csv','Roi')[0]))
        PB.setStatus(root,i)

        # Determine whether step 2 has been completed yet
        d_s2 = verifyDirlist(plate_ana,False,'Activity','png')[0]
        if len(d_s2) > 0:
            treeview.Completed_STEP2.append("Yes")
        else:
            treeview.Completed_STEP2.append("No")


    #Populating Tree View
    for i, plate in enumerate(treeview.plates_master):
        treeview.tree.insert("", 'end', text=str(plate), values = (treeview.N_image_sessions[i],treeview.N_analyzed_sessions[i],treeview.Completed_STEP2[i]))
    
    #Selecting all plates to start
    children_values = [treeview.tree.item(child)["values"] for child in treeview.tree.get_children()]
    children_written = [treeview.tree.item(child)["text"] for child in treeview.tree.get_children()]
    children = [child for child in treeview.tree.get_children()]
    treeview.tree.selection_set(children)
    treeview.plate_ids = treeview.tree.selection()
    
    #Dictionary comparing plates and id's
    treeview.ids_to_plates = {z[0]:list(z[1:]) for z in zip(treeview.plate_ids,treeview.plates_master, treeview.plates_img, treeview.plates_ana, treeview.Completed_STEP2)}
    treeview.selected_plates_ids= treeview.tree.selection()
    selected_plate_names = [treeview.ids_to_plates[id] for id in treeview.selected_plates_ids]
    #pdb.set_trace()

    #Selected Items
    #print(children_written)

    #Elapsed Time
    #print(plate + "  Total Time to Load Plates = " + str(time.time() - total_start))



def pushbutton_abort_callback(*args):

    # Change button color and text
    pushbutton_abort.configure(text = "Aborting...")

    # First, signal abort for the analysis thread
    global abort_analysis_flag
    abort_analysis_flag = True

    # Second, kill any open ActivityCalc.exe process
    PROCNAME = "ActivityCalc.exe"
    for proc in psutil.process_iter():

        # check whether the process name matches
        if proc.name() == PROCNAME:
            proc.kill()

    # Third, restore window and/or buttons
    root.deiconify()
    for btn in buttons:
        btn['state'] = 'normal'

def pushbutton_execute_callback(*args):
    
    # Verify that the parent folder is valid
    if not verifyValidParentDirectory(pparent.get(),Err): 
        return

    # Get all settings from the settings list
    opts, error_msg, json_data = selected_options.saveList()

    # Validate settings for the requested analysis
    if len(error_msg) > 0 :
        messagebox.showerror("Error",error_msg)
        return

    # Determine whether a multithreaded step1 was requested
    N_threads = 0
    if atypes[0] in dd_var.get() and len(opts) >= 10:
        N_threads = json_data["Step 1 threads"]


    # Show warnings if the selected plates do not meet the requirements
    if not treeview.validateSelection(dd_var.get(), atypes, N_threads):
        return



    # If multithreaded step1 was requested, launch the requested number of instances of ActivityCalc, no need for multithreading
    if N_threads > 1 and N_threads <= 3 and atypes[0] in dd_var.get()  :

        # Figure out the index cutoffs for each thread
        selected_plates_img = treeview.getSelectedPlatesByType(dd_var.get() , atypes)
        N_plates = len(selected_plates_img)
        (trash,cutoffs) = np.histogram(np.arange(0,N_plates), bins=N_threads)
        cutoffs = np.ceil(cutoffs)

        # Spawn the requested number of workers without waiting for completion
        progname = r'C:\WormWatcher\ActivityCalc.exe'
        for i in range(0,N_threads) :
            istart = str(int(cutoffs[i]))
            iend = str(int(cutoffs[i+1]-1))
            output = subprocess.Popen([progname, pparent.get(), istart, iend])
            Err.notify("Print",output) 
            sleep(6)
            


    # If multithreading was requested with an invalid number of threads, show error and exit
    elif atypes[0] in dd_var.get() and N_threads >  3:
        messagebox.showerror("Error","You requested a multithreaded STEP 1 analysis, but more than 3 threads is not recommended. Please decrease the number of threads and try again")

    # Otherwise, start a new thread with the selectedanalysis
    else:
        analysis_for_each_plate(opts)

    # When finished, refresh the data in the table
    pushbutton_path_callback(True);

def analysis_for_each_plate(*args):

    global atypes, buttons, abort_analysis_flag, PB

    # Enable abort pushbutton
    pushbutton_abort['state'] = 'normal'
    pushbutton_abort.configure(text = "Abort")

    # Disable all buttons while analysis is running
    for btn in buttons:
        btn['state'] = 'disabled'

    Err.notify("Print","Execute: " + dd_var.get())

    # Get list of all selected plates for the correct type
    selected_plates = treeview.getSelectedPlatesByType(dd_var.get(),atypes)
    PB.setStatus(root,0,len(selected_plates),"Execute: " + dd_var.get())

    # Hide the window during analysis
    #root.withdraw()

    # Run the analysis for each plate if only a subset of plates was selected
    for (this_plate,j) in zip(selected_plates,range(len(selected_plates))):

        # Extract the plate search name for one individual plate if doing one-at-a-time
        [trash,short_name] = os.path.split(this_plate)

        # Update the progressbar with the current status only
        PB.setStatus(root,None,None,"Execute: " + dd_var.get() + " - " + short_name +" - Plate " + str(j+1) + " of " + str(len(selected_plates)))

        if atypes[0] in dd_var.get(): 
                
            # Call the analyzer program. Exit if escape was pressed (-1 return val)
            progname = r'C:\WormWatcher\ActivityCalc.exe'
            Err.notify("Print","Analyzing: "+short_name)
            #command = "\"" + progname + "\" " + "\""+ pparent.get() + "\"" + " \"0\" " + " \"99999\" " +  "\"" + short_name + "\""
            #DisplaySubprocessOutput(root, scrolltext,command)
            output = subprocess.run([progname,pparent.get(),"0","99999",short_name], shell=True, capture_output = True,text=True)

            # Abort if user pressed escape
            if output.returncode != 0:
                Err.notify("PrintRed","User aborted the analysis before completion.")
                break

            # Otherwise display error encoded by return val
            Err.notifyLongMsg(output.stdout)


        # Step 2 activity curves
        elif atypes[1] in dd_var.get():
            activityCurveGenerator(this_plate,Err,"Log")
            Err.notify("Print","Generated activity curves for: " + short_name) 


        # Manual inspection
        elif atypes[2] in dd_var.get():
            manualImageInspection(pparent.get())

        # 
        elif atypes[3] in dd_var.get():
            
            param_list = all_param(selected_plates, "<Tx>")        
            if args[0] in param_list[0][0]:      # Check if the input value is valid
                plotGroupAverages(selected_plates, args[0], "<Tx>") 
                
            else:
                messagebox.showerror("Error",str(args[0])+" is an invalid parameter value")
            
            break
        
        # Plot Tx levels
        elif atypes[4] in dd_var.get():
            
            
            param_list = all_param(selected_plates, "<ROI cumulative noise-floor subtracted activity>")
                
            if args[0] > selected_plates[1]:      # Check if the input value is larger than the maximum value
                plotGroupAverages(selected_plates, args[0], "<ROI cumulative noise-floor subtracted activity>") 

            elif args[0] in selected_plates[2]:   # The parameter entered should be shared by all selected plates
                plotGroupAverages(selected_plates, args[0], "<ROI cumulative noise-floor subtracted activity>")

            else:
                messagebox.showerror("Error",str(args[0])+" is an invalid parameter value")
            break
        
        # Write to csv file
        elif atypes[5] in dd_var.get():

            
            data_csv(pparent.get(), selected_plates, selected_plates, [[args[0][0], args[0][1]], [args[0][2]]])
            break

        # Censor bad wells
        elif atypes[6] in dd_var.get():

            # Convert plateful from analysis path to actual file name, it must already exist due to PlateTreeView's checking
            AC_file = validateActivityCurvesFile(this_plate,Err)
            if len(AC_file) == 0:
                continue

            # Load labels from this file, make sure labels exist
            labels = readArrayFromCsv(AC_file, "<Group name>")
            if "empty" in labels:
                messagebox.showerror("Error",this_plate + "\nDoes not have group names in STEP 2 file.\n\nTry running STEP 2 again (any old groups / censor decisions will not be deleted")
                break

            # Set parameters by input arguments
            min_days = args[0][0]
            min_wf = list()
            min_wf.append(args[0][1:4]) # Day 3 must have between 0.15 and 0.50 worm fraction
            min_wf.append(args[0][4:7]) # Day 10 
            frac_inc = args[0][7]

            # Apply censors and write to file
            new_labels,reason = censorWellsAuto(AC_file,labels,min_days,min_wf,frac_inc)
            replaceVariableInCsv(AC_file,",,<Group name>",new_labels,2)
            replaceVariableInCsv(AC_file,",,<Censor reason>",reason,2) # Or adds it if it doesn't already exist


        # Convert old style data
        elif atypes[7] in dd_var.get():

            breakUpSession(this_plate)



        # Error
        else:
            Err.notify("Print","Internal error - invalid analysis type selected.")

        # Update progressbar
        PB.setStatus(root,j+1,len(selected_plates),"Execute: " + dd_var.get() + " - " + short_name +" - Plate " + str(j+1) + " of " + str(len(selected_plates)))


        # Exit if requested
        if abort_analysis_flag :
            abort_analysis_flag = False
            pushbutton_abort.configure(text = "Abort")
            pushbutton_abort['state'] = 'disabled'
            break

    Err.notify("Print","Completed analysis: " + dd_var.get())

    # Restore window and/or buttons
    root.deiconify()
    for btn in buttons:
        btn['state'] = 'normal'


def pushbutton_grab_callback(*args):
    print(treeview.tree.selection())
    return

def pushbutton_select_all_callback(*args):
    #Getting all childrenchildren = []
    children = [child for child in treeview.tree.get_children()]
    treeview.tree.selection_set(children)
    print(treeview.tree.selection())

def pushbutton_select_none_callback(*args):
    treeview.tree.selection_set()
    print("Selected Cleared")


################################################
#                 MAIN
################################################


logging.basicConfig(filename='C:/WormWatcher/MyLog.log',level=logging.DEBUG)


try:
    #Setup for tkinter GUI
    root = Tk()
    s = Style()
    s.theme_use('vista')
    root.geometry("1400x980") #TODO: adapt to screen size
    root.title("WWAnalyzer 2.0")
    bgcolor = (214,219,233)
    yellowfont = (255,242,147)
    root.configure(background=rgb2Hex(bgcolor))
    root.resizable(False, False)

    #Colors
    greenfade = (172,222,172)
    redfade = (222,172,172)


    """------ TOP FRAME - SETTING THE PATH ------"""
    # Create the frame
    topbgcolor = (41,58,86)
    frame_path = Frame(root,relief = SUNKEN, borderwidth = 2)
    frame_path.pack(side="top",fill="both")
    frame_path.configure(background=rgb2Hex(topbgcolor))


    # Add buttons and labels
    pparent = StringVar(root)
    pparent.set(r"C:\WormWatcher")
    Label(frame_path, text="Parent Directory",font='Helvetica 16 bold',wraplength=260,justify=CENTER,background=rgb2Hex(topbgcolor),foreground=rgb2Hex(yellowfont)).pack()
    Label(frame_path,textvariable=pparent, text="Select Directory",font='Helvetica 13',wraplength=800,justify=LEFT,background=rgb2Hex(topbgcolor),foreground=rgb2Hex(yellowfont)).pack()
    path_style = Style()
    path_style.configure('my.TButton', font=('Helvetica', 13))
    pushbutton_path = Button(frame_path,text="Change Path",command = pushbutton_path_callback, style = 'my.TButton')
    pushbutton_path.pack() # Must be done on separate line



    """------ VERY BOTTOM - SCROLLING TEXT ----- """

    frame_scroll = tk.Frame(root)
    frame_scroll.pack(side = tk.BOTTOM, fill=tk.X,padx=10, pady=10)

    scrolltext = tkst.ScrolledText(frame_scroll,wrap = tk.WORD,height = 18)
    scrolltext.pack(padx=0, pady=0, fill=tk.X, expand=False, side = tk.BOTTOM)
    scrolltext.insert(tk.INSERT,"\nWelcome to WWAnalyzer!")


    # Create Err notifier that feeds to this scrolled text
    Err = ErrorNotifier(scrolltext)

    """------ BOTTOM FRAME - PROGRESS BAR ------"""
    #Creating progress bar
    PB = LabeledProgressbar(root,"bottom",rgb2Hex(bgcolor))


    """------ LEFT FRAME - SELECTING PLATES ------"""
    # Create the frame
    frame_tree = Frame(root,relief = RIDGE, borderwidth = 1)
    frame_tree.pack(side="left", fill=tk.BOTH,expand=1)
    frame_tree.configure(background=rgb2Hex(bgcolor))

    # Create Tree View
    treeview = PlateTreeView(frame_tree,"Highlight Plates to Select","top",pparent,rgb2Hex(bgcolor))


    # Subframe for buttons
    subframe_buttons = Frame(frame_tree, bd=0, relief=GROOVE)
    subframe_buttons.configure(background=rgb2Hex(bgcolor))
    subframe_buttons.pack(side="bottom",padx=5,pady = 5)
    s.configure("my.TButton",font=('Helvetica',12,'bold'), compound = "c", background = rgb2Hex(greenfade))
    button_select_all = ttk.Button(subframe_buttons,text="Select All",command = pushbutton_select_all_callback, style = 'my.TButton')
    button_select_none = ttk.Button(subframe_buttons, text = "Select None", command = pushbutton_select_none_callback, style = 'my.TButton')
    button_refresh = ttk.Button(subframe_buttons, text = "Refresh", command = pushbutton_refresh_callback, style = 'my.TButton')
    button_select_all.pack(side="left",padx=5)
    button_select_none.pack(side="left",padx=5)
    button_refresh.pack(side="left",padx=5)


    # Create frame for analysis type selection
    frame_analysis = Frame(root)
    frame_analysis.configure(background=rgb2Hex(bgcolor),relief = RIDGE, borderwidth = 1)
    frame_analysis.pack(side="left",fill=BOTH, expand=1)

    Label(frame_analysis, text="Analysis type",font =("Helvetica", 14),background=rgb2Hex(bgcolor),pady = 5).pack(side="top")

    # Set analysis types
    atypes = [
                "STEP 1: Image differences",
                "STEP 2: Activity over time",
                "STEP 3: Inspect and label plates",
                "Lifespan, Healthspan, or Tx level",
                "Total activity",
                "Export data",
                "Auto-censor bad wells",
                "Convert old format data"
              ]

    helps = [
            "Description:\nFind all imaging sessions in the selected parent folder and run the pixel difference computation for them",
            "Description:\nCondense the activity of each session into a single point and plot activity over time",
            "Description:\nInteractively inspect activity in individual wells, specify the groupings of wells on a plate, or censor wells",
            "Description:\nPlot the lifespan estimate (T97), healthspan estimate (T89) or any other T-level",
            "Description:\nPlot the total activity. For activity on the last day, enter an out-of-bounds timepoint such as 100 days",
            "Description:\nWrite selected data to csv file. The file will be saved in parent folder.",
            "Description:\nUse the worm detection neural network to remove bad wells.",
            "Description:\nConvert data collected using version 1.85 or earlier of WormWatcher to the latest organizational format.\n\nIt's recommended to back up your data first - this process permanently changes the folder structure and some older analysis files may be deleted.\n\nIf you're not sure whether the data format is current it's safe to run this procedure multiple times"
            ]


    # Create special string variable to hold current option in the dropdown menu
    dd_var = StringVar(root)
    dd_var.set(atypes[0]) # default value
    #dd_var.trace("w",dropdown_callback)
    sddm = ttk.Style()
    sddm.configure("my.TMenubutton", font='Helvetica 12',background = rgb2Hex(yellowfont),anchor='E')
    dropDownMenu = ttk.OptionMenu(frame_analysis,dd_var,atypes[0],*atypes,command=dropdown_callback,style="my.TMenubutton")
    dropDownMenu.configure(width = 44)
    dropDownMenu.pack()

    # Create a text label that describes each analysis
    help_var = StringVar(root)
    help_var.set(helps[0])
    labelhelp = Label(frame_analysis,textvariable=help_var,wraplength=300,justify=LEFT, anchor = NW, font='Helvetica 12', width = 33,background=rgb2Hex(bgcolor)).pack();

    # Subframes for analysis options
    subframe_analysis_opts = Frame(frame_analysis,background=rgb2Hex(bgcolor),relief = RIDGE, borderwidth = 1,height=300)
    subframe_analysis_opts.pack()
    Label(subframe_analysis_opts,text="Analysis Options",wraplength=300,justify=CENTER, font='Helvetica 14', width = 33, height = 2,background = rgb2Hex(bgcolor)).pack();


    # pushbutton_execute to run the selected analysis, below everything tlese
    subframe_exec_abort = Frame(frame_analysis,background = rgb2Hex(bgcolor))
    subframe_exec_abort.pack(side='bottom',pady=5)
    s.configure('my.TButton', font=('Helvetica',14,'bold'),width = 20)
    pushbutton_execute = Button(subframe_exec_abort,text="Execute",command = pushbutton_execute_callback, style = 'my.TButton')
    pushbutton_execute.pack(side='left',padx=5)

    # pushbutton_abort to end all analysis
    s.configure('my.TButton', font=('Helvetica',14,'bold'),width = 20)
    pushbutton_abort=Button(subframe_exec_abort,text="Abort",command = pushbutton_abort_callback, style = 'my.TButton')
    pushbutton_abort.pack(side='left',padx=5)
    pushbutton_abort['state'] = 'disabled'
    abort_analysis_flag = False

    # Consolidate all buttons into a list for convenience later
    buttons = [button_select_all, button_select_none,pushbutton_execute,pushbutton_path]
    selected_options = optionList(True,rgb2Hex(bgcolor))



    # If the default folder is a valid parent folder load the data
    pushbutton_path_callback()
    mainloop()
except Exception as e:
    logging.info(e)