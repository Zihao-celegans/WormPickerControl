"""
    LabeledProgressbar:
    Progress bar that has a text header on top to explain what it's doing
"""
import tkinter as tk
from tkinter import ttk
from tkinter import *
import os
from os import listdir
from PIL import ImageTk,Image
from verifyDirlist import verifyDirlist
import numpy as np
import time


class LabeledProgressbar(Tk):

    # Constructor
    def __init__(self,*args, **kwargs):

        #Unpack Arguments
        parent = args[0]
        pack_side = args[1]
        bgcolorhex = args[2]

        # Create frame
        self.frame = Frame(parent, borderwidth = 2, relief = tk.GROOVE, background = bgcolorhex)
        self.frame.pack(side=pack_side,fill = tk.X, expand = 0, padx = 0, pady = 1)

        # Create title label
        self.TITLE = "Idle"
        self.title_label = Label(self.frame,text=self.TITLE,font = ('Helvetica',12),background = bgcolorhex)
        self.title_label.pack(side=tk.TOP, fill = tk.Y, expand=1)

        # Create progressbar
        self.progress_bar = ttk.Progressbar(self.frame, orient = HORIZONTAL, length = 1000, mode = 'determinate') 
        self.progress_bar.pack(side="bottom",fill="x", expand=0)
        self.progress_bar["value"] =0
        self.MAX = 100;

    # Member function: Set progressbar value
    def setStatus(self,root,curr = None, max = None, TITLE = ""):

        # Max value of PB, multiply by 10 in case of sub integer
        if max != None:
            self.MAX = max
            self.progress_bar['maximum'] = max*10

        # Current value of PB, multiply by 10 in case of sub integer
        if curr != None:
            self.CURR = curr
            self.progress_bar["value"] = curr*10

        # Title, if completed 
        if curr == self.MAX:

            # Clear COMPLETED if it already exists, e.g. aborting?
            self.TITLE =self.TITLE.replace("  COMPLETED","")

            self.TITLE = self.TITLE + "  COMPLETED"
            self.title_label.configure(text = self.TITLE,font = ('Helvetica',12))

        # Title, if set by input
        elif TITLE != "":
            self.TITLE=TITLE
            self.title_label.configure(text = self.TITLE,font = ('Helvetica',12))
            
        # Apply current title
        

        # Refresh widgets in case inside loop
        root.update()
        return

