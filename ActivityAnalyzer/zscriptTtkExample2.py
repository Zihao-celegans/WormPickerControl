# https://riptutorial.com/tkinter/example/31880/treeview--basic-example

#Complete Imports
import tkinter as tk
from tkinter import ttk
import os
from PIL import ImageTk,Image
from tkinter import *
from WWAParent import WWAParent

#General Imports
import os
import subprocess

# ----- Callbacks for Treeview --------
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

def pushbutton_refresh_callback(*args): 
     #reopen folder that was take
     return


 # ----- Creating Frame for Treeview ------

# Creating Root and Setting Theme
root = tk.Tk()
root.geometry("1000x1000")
s = ttk.Style()
s.theme_use('vista')

#Creating Frame for Treeview
frame_tree = Frame(root,relief = RIDGE, borderwidth = 1)
frame_tree.pack(side="top", fill=tk.X)

#Creating Treeview Object
redfade = (222,172,172)
treeview = WWAParent(frame_tree,"HighLight Plates to Select","top",redfade)

#Subframe for Buttons
subframe_buttons = Frame(frame_tree)
subframe_buttons.pack(side="bottom",pady=30)

# -------- Buttons ----------

# Button Style
s.configure('my.TButton',font = ('Helvetica',12),height = 16,width = 10)
#Buttons to Grab, Select All, Select None, Refresh
grab_button = ttk.Button(subframe_buttons, text = "Grab", command = pushbutton_grab_callback, style = 'my.TButton')
select_all_button = ttk.Button(subframe_buttons, text = "Select All", command = pushbutton_select_all_callback, style = 'my.TButton')
select_none_button = ttk.Button(subframe_buttons, text = "Select None", command = pushbutton_select_none_callback, style = 'my.TButton')

# Packing onto Template
grab_button.pack(side='left')
select_all_button.pack(side='left')
select_none_button.pack(side='left')

tk.mainloop()