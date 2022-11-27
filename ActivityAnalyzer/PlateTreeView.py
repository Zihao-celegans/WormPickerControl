# Imports
import tkinter as tk
from tkinter import ttk
from tkinter import *
from tkinter import filedialog
from tkinter import messagebox
import os
from os import listdir
from PIL import ImageTk,Image
from verifyDirlist import verifyDirlist
import numpy as np

class PlateTreeView(Tk):
    plates_img = []
    plates_ana = []
    plates_master = []
    N_analyzed_sessions = []
    N_image_sessions = []
    plate_ids = []

    #Constructor
    def __init__(self, *args, **kwargs):

        #Unpack Arguments
        parent = args[0]
        title = args[1]
        pack_side = args[2]
        directory = args[3]
        bgcolorhex = args[4]

        # Create frame for the listbox
        self.frame = Frame(parent,background = bgcolorhex)
        self.frame.pack(side=pack_side,fill = tk.BOTH, expand = 1, padx = 10)

        # Upper label
        Label(self.frame, text=title,font =("Helvetica", 16), background = bgcolorhex).pack(side="top")

        # Create the treeview
        self.tree=ttk.Treeview(self.frame)
        self.tree.pack(side="left", fill="both", expand = 1)
        print("Tree created")

        # Create the scrollbar
        self.scrollbar = Scrollbar(self.frame, orient="vertical")
        self.scrollbar.config(command=self.tree.yview)
        self.scrollbar.pack(side="left", fill="y")
        self.tree.config(yscrollcommand=self.scrollbar.set)

        # Define columns
        self.tree["columns"]=("one","two","three")
        self.tree.column("#0", width=200, minwidth=100, stretch=tk.YES)
        self.tree.column("one", width=125, minwidth=100, stretch=tk.NO)
        self.tree.column("two", width=125, minwidth=100, stretch = tk.NO)
        self.tree.column("three", width=125, minwidth=100, stretch = tk.NO)

        # Define column headers 
        self.tree.heading("#0",text="Plate",anchor=tk.W)
        self.tree.heading("one", text="Image sessions",anchor=tk.W)
        self.tree.heading("two", text="STEP 1 sessions",anchor=tk.W)
        self.tree.heading("three",text="STEP 2 completed",anchor=tk.W)

    # Clear Tree
    def clear_tree(self):
        # Delete old values
        self.tree.delete(*self.tree.get_children())
        return

    # Get a list of the selected items
    def getSelected(self,folder_type):

        # Select return type
        if "STEP2DONE" in folder_type:
            returnid = 3

        elif "ana" in folder_type:
            returnid = 2

        elif "img" in folder_type:
            returnid = 1

        elif "name" in folder_type:
            returnid = 0

        else:
            print('Internal error: invalid foldertype specified: ', folder_type)
            sys.exit(0)

        # Extract data matching selection, e.g. analysis or image
        selected_plates_ids= self.tree.selection()

        # STEP2DONE - return the analysis folder, but only if STEP2 is yes
        if "STEP2DONE" in folder_type:
            selected_plates = [self.ids_to_plates[id][2] for id in selected_plates_ids if 'Yes' in self.ids_to_plates[id][3]] # Selected good of requested type
            excluded_names = [self.ids_to_plates[id][0] for id in selected_plates_ids if not 'Yes' in self.ids_to_plates[id][3]] # names of excluded plates

        # Otherwise, return the image or analysis folder requested
        else:
            selected_plates = [self.ids_to_plates[id][returnid] for id in selected_plates_ids if self.ids_to_plates[id][returnid]] # Selected good of requested type
            excluded_names = [self.ids_to_plates[id][0] for id in selected_plates_ids if not self.ids_to_plates[id][returnid]] # names of excluded plates

        # Return the selected list and excluded plates (due to empty name, for example)
        return (selected_plates, excluded_names)

    # Make sure the selected plates can all be analyzed using the requested analysis type
    def validateSelection(self, atype, atypes, N_threads):

        # Get all selections
        selected_names, trash = self.getSelected('name')
        selected_plates_img, excluded_names_img = self.getSelected('img')
        selected_plates_ana, excluded_names_ana = self.getSelected('ana')
        selected_plates_s2done, excluded_names_s2done = self.getSelected('STEP2DONE')


        # First, make sure something was selected
        if len(selected_names) == 0 :
            messagebox.showerror("No selection","You didn't select any plates!")
            return False

        # Analysis type 0 - STEP 1 - multithreading requires whole-folder selection AND must have image folders
        if (atype in atypes[0]) and (len(selected_names) < len(self.ids_to_plates) and (N_threads > 1)) :
            header = "You selected a subset of the plates in this parent folder and specified Step 1 threads = " + str(N_threads)
            header += '\n\nMultithreaded analysis ONLY supports whole-folder input and will be converted accordingly.'
            if not self.showMultiPlateWarning(header,''):
                return False

        if (atype in atypes[0]) and (len(excluded_names_img) > 0 ) :
            header = "The following plates do not have image folders available and cannot be analyzed with STEP 1. They will automatically be skipped:"
            if not self.showMultiPlateWarning(header,excluded_names_img):
                return False        


        # Analysis type 1 - STEP 2 - requires STEP 1 completed
        if (atype in atypes[1]) and (len(excluded_names_ana) > 0 ) :
            header = "The following plates do not have STEP 1 csv files available and cannot be analyzed with STEP 2. They will automatically be skipped:"
            if not self.showMultiPlateWarning(header,excluded_names_ana):
                return False     

        # Analysis type 2 - STEP 3 - Requires images AND STEP 1 done AND STEP 2 done
        if (atype in atypes[2]) and (len(excluded_names_img) > 0 ) :
            header = "The following plates do not have image folders available and cannot be inspected with STEP 3. They will automatically be skipped:"
            if not self.showMultiPlateWarning(header,excluded_names_img):
                return False        

        if (atype in atypes[2]) and (len(excluded_names_ana) > 0 ) :
            header = "The following plates do not have STEP 1 csv files available and cannot be analyzed with STEP 3. They will automatically be skipped:"
            if not self.showMultiPlateWarning(header,excluded_names_ana):
                return False     

        if (atype in atypes[2]) and (len(excluded_names_s2done) > 0 ) :
            header = "The following plates do not have STEP 2 csv files available and cannot be analyzed with STEP 3. They will automatically be skipped:"
            if not self.showMultiPlateWarning(header,excluded_names_s2done):
                return False     
        

        # All remaining analysis types require STEP 2 completion

        if (atype in atypes[3:7]) and (len(excluded_names_s2done) > 0 ) :
            header = "The following plates do not have STEP 2 csv files available and cannot be analyzed. They will automatically be skipped:"
            if not self.showMultiPlateWarning(header,excluded_names_s2done):
                return False     

        return True

    # Show a warning for some plates and ask user whether to continue
    def showMultiPlateWarning(self,header,excluded_names):
        is_good = True
        
        msg =header + '\n\n'
        for (i,excl) in enumerate(excluded_names):
            if i >=5:
                break
            msg+='\t' + excl + '\n'

        if len(excluded_names) >= 4:
            msg+='\tAnd possibly more...\n'

        msg+='\nDo you want to run the analysis anyway?'

        response = messagebox.askquestion('Skip plates?',msg,icon = 'warning')
        if response == 'no':
            is_good = False

        return is_good

    # Return the selected plates for whichever analysis type
    def getSelectedPlatesByType(self,atype,atypes):

        # Get all selections
        selected_names, trash = self.getSelected('name')
        selected_plates_img, excluded_names_img = self.getSelected('img')
        selected_plates_ana, excluded_names_ana = self.getSelected('ana')


        # STEP 1 and data reformat Needs images
        if atype in atypes[0] or atype in atypes[7]:
            return selected_plates_img

        # Anything else: needs analysis path
        else:
            return selected_plates_ana

    


