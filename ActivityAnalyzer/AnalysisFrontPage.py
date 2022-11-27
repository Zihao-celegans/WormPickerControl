#GUI Imports
from tkinter import *
from tkinter import filedialog
from tkinter import messagebox
from tkinter import ttk
from tkinter.ttk import *
from ReformatParentDirectory import reformatParentDirectory
from ReformatParentDirectory import verifyValidParentDirectory
#from manualImageInspector import manualImageInspection
from ActivityCurveGenerator import activityCurveGenerator
from plotGroupAverages import plotGroupAverages

#General Imports
import os
import subprocess


""" Callbacks """ 
def dropdown_callback(*args): 

    # Print to console which option was selected
    print("Selected: ", dd_var.get())

    # Figure out which help label to add
    idx = 0
    idx = atypes.index(dd_var.get())
    help_var.set(helps[idx])

    # Figure out which step label to display
    step_var.set(stypes[idx])

def dropdown_dt_callback(*args): 

    # Print to console which data type was selected
    print("Selected: ", dt_var.get())

def pushbutton_path_callback(*args):

    pushbutton_path.configure(background = "orange",text = "Validating...")

    # Ask the user to select a folder
    tempdir = filedialog.askdirectory(parent=root, initialdir=pparent.get(), title='Please select the PARENT directory')
    if len(tempdir) > 0:
        print("You chose %s",tempdir)

    # Enforce a valid parent directory

    if verifyValidParentDirectory(tempdir) :
        pparent.set(tempdir)
        pushbutton_path.configure(background = "gray", text = "Change path")
    else:
        pushbutton_path.configure(background = "red", text = "Change path")



def pushbutton_execute_callback(*args):

    print("Execute: ", dd_var.get())

    # Verify that the parent folder is valid
    if not verifyValidParentDirectory(pparent.get()): 
        return


    if "Step 1" in dd_var.get(): 

        # First, ensure the data is in the correct format
        reformatParentDirectory(pparent.get())

        # Second, call the analyzer program
        progname = r'C:\WormWatcher\ActivityCalc.exe'
        subprocess.run([progname,pparent.get()], shell=True)

        print("Completed analysis")

    elif "Step 2" in dd_var.get():
        activityCurveGenerator(pparent.get())

    elif "Step 3" in dd_var.get():
        try:
            plotGroupAverages(pparent.get(), float(user_input_num.get()), dt_var.get()) # .get() gives a string, need to convert to float to use
        except:
            messagebox.showerror("Error",user_input_num.get()+" is an invalid parameter value")
    
    elif "Manual image inspection" in dd_var.get():
        manualImageInspection(pparent.get())
        B=1
    else:
        print("INVALID COMMAND!!")

    root.deiconify()

#Buttons for Tree 
def pushbutton_select_all_callback(*args):
    print(tree.selection())
    return
def pushbutton_select_none_callback(*args):
    print(tree.selection())
    return
def pushbutton_refresh_callback(*args):
    print(tree.selection())
    return



""" Main script """

# Main window and size
root = Tk()
root.geometry("280x700") #Width x Height
root.title("WWAnalyzer 2.0")
#root.iconbitmap(r'C:\\Dropbox\\Autogans Supporting Files\\Icons\\WormWatcher Logo V01.ico')

# Frames for organizing buttons

frame_top_space = Frame(root,width=280,height=12,relief='flat',borderwidth=0)

frame_top = Frame(root,width=280,height=125,relief='flat',borderwidth=0)

frame_middle = Frame(root,width=280,height=200,relief='flat',borderwidth=0)

frame_bottom_space = Frame(root,width=280,height=12,relief='flat',borderwidth=0)

# Stackable frames for custom options depending on what analysis is selected
frame_step1 = Frame(root,width=280,height=275,relief='flat',borderwidth=1)

#Tree frame
frame_tree = Frame(root, width = 280, height = 200, relief = 'flat', borderwidth = 0)

# Pack all frames

for frame in [frame_top_space, frame_top, frame_middle, frame_step1, frame_tree]:
    frame.pack(expand=True, fill='both')
    frame.pack_propagate(0)

# pushbutton_execute to run the selected analysis, below everything tlese
pushbutton_execute = ttk.Button(root,text="Execute",command = pushbutton_execute_callback, font='Helvetica 12')
pushbutton_execute.pack()
frame_bottom_space.pack()

# Set analysis types
global atypes
atypes = [
            "Step 1: Analyze each session",
            "Step 2: Consolidate all sessions",
            "Step 3: Custom analysis",
            "Manual image inspection",
          ]

global helps
helps = [
        "Description:\nFind all imaging sessions in the selected parent folder and run the pixel difference computation for them",
        "Description:\nCondense the activity of each session into a single point and plot activity over time",
        "Description:\nSelect a custom analysis to run, such as comparing two groups on a plate",
        "Description:\nInteractively inspect activity in individual wells, specify the groupings of wells on a plate, or censor wells"
        ]

""" TOP FRAME - SETTING THE PATH """ 
pparent = StringVar(root)
pparent.set(r"C:\Users\Hongjing Sun\Desktop\Worm\SampleParentFolder")
labelpathheader=Label(frame_top, text="Parent directory",font='Helvetica 16 bold',wraplength=260,justify=CENTER).pack()
labelcurrpath=Label(frame_top,textvariable=pparent, text="Select directory",font='Helvetica 12',wraplength=260,justify=LEFT).pack()
pushbutton_path = ttk.Button(frame_top,text="Change path",command = pushbutton_path_callback, font='Helvetica 12')
pushbutton_path.pack()

""" MIDDLE FRAME - SELECTING THE ANALYSIS """

# Create special string variable to hold current option
dd_var = StringVar(root)
dd_var.set(atypes[0]) # default value
dd_var.trace("w",dropdown_callback)

# Create option menu
labeltop=Label(frame_middle, text="Analysis type",font='Helvetica 16 bold').pack()
dropDownMenu = OptionMenu(frame_middle,dd_var,*atypes).pack()

# Display helper text for each
help_var = StringVar(root)
help_var.set(helps[0])
labelhelp = Label(frame_middle,textvariable=help_var,wraplength=260,justify=LEFT, font='Helvetica 12').pack();

# Display the step chosen
global stypes
stypes = [
            "Step 1 options",
            "Step 2 options",
            "Step 3 options",
            "Manual image inspection",
          ]
step_var = StringVar(root)
step_var.set(stypes[0])
labelstep=Label(frame_middle, textvariable=step_var,font='Helvetica 16 bold').pack()


# User input to select parameter
# Step 3 option
# Set data types
global datatypes
datatypes = [
            "<ROI cumulative NFS activity>",
            "<Tx>",
            "<ROI stimulated activity>"
          ]

# Create special string variable to hold current option for data type selection
dt_var = StringVar(root)
dt_var.set(datatypes[0]) # default value
dt_var.trace("w", dropdown_dt_callback)

label_data_type = Label(frame_step1, text="Enter datatype",font='Helvetica 10').pack()
dropDownMenu_data = OptionMenu(frame_step1, dt_var, *datatypes)
dropDownMenu_data.pack()

label_parameter = Label(frame_step1, text="Enter parameter",font='Helvetica 10').pack()
user_input_num = Entry(frame_step1)
user_input_num.pack()

""" MIDDLE FRAME - Displaying file in directory and selecting which to be analyzed """

#Modern Aesthetic, Bold Header, No Borders
tree_style = ttk.Style()
tree_style.configure("modern.Treeview", highlightthickness=0, bd=0, font=('Helvetica', 11)) # Modify the font of the body
tree_style.configure("modern.Treeview.Heading", font=('Helvetica', 14,'bold')) # Modify the font of the headings
tree_style.layout("modern.Treeview", [('modern.Treeview.treearea', {'sticky': 'nswe'})]) # Remove the borders

#Creating Tree
tree=ttk.Treeview(frame_tree, style="modern.Treeview")

# Create its scrollbar
vsb = ttk.Scrollbar(frame, orient="vertical", command=tree.yview)
tree.configure(yscrollcommand=vsb.set)

#Columns
tree["columns"]=("Plate","N Image sessiosn","N Analyzed Sessions")
tree.column("Plate", width=100, minwidth=50)# stretch=Tk.NO)
tree.column("N Image Sessions", width=100, minwidth=50)#, stretch=tk.NO)
tree.column("N Analyzed Sessions", width=100, minwidth=50)#, stretch=tk.NO)

#Tree Headings
tree.heading("#0",text="Plate",anchor=tk.W)
tree.heading("one", text="N image sessions",anchor=tk.W)
tree.heading("two", text="N analyzed sessions",anchor=tk.W)

#Populate Tree
folders = list()
for plate, N_img, N_ana  in zip(plate_ids, N_image_sessions, N_analyzed_sessions):
    folders.append(tree.insert("", 'end', "", text=str(plate), values = (N_img, N_ana)))

# Packing data
tree.pack(side=tk.RIGHT,fill = tk.BOTH)
vsb.pack(side=tk.RIGHT,fill = tk.Y)
frame_tree.pack(side=tk.TOP)

# Select All, Select None, Refreh Buttons
#Button Style
s = ttk.Style()
s.configure('my.TButton',font = ('Helvetica',12),height = 16,width = 40)

#Select All Button
b = ttk.Button(root, text = "Grab", command = pushbutton_grab_callback, style = 'my.TButton')
b.pack(side=tk.BOTTOM)
tree.tag_configure('ttk', background='yellow')

#Select None Button
b = ttk.Button(root, text = "Grab", command = pushbutton_grab_callback, style = 'my.TButton')
b.pack(side=tk.BOTTOM)
tree.tag_configure('ttk', background='yellow')

#Refresh Button
b = ttk.Button(root, text = "Grab", command = pushbutton_grab_callback, style = 'my.TButton')
b.pack(side=tk.BOTTOM)
tree.tag_configure('ttk', background='yellow')

""" Useful Functions """
#selection_add(*items)
#selection_toggle(*items)

mainloop()








