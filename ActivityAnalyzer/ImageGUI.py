"""
Hongjing Sun
Created in Nov 2019

"""
import tkinter as tk
from tkinter import ttk
from tkinter import filedialog
from tkinter import messagebox
from PIL import Image, ImageTk, ImageChops

from ReformatParentDirectory import verifyValidParentDirectory
from verifyDirlist import verifyDirlist
import os
import numpy as np
from csvio import readArrayFromCsv


# Normalize image to enhace contrast
def normalize(f):
    lmin = float(f.min())
    lmax = float(f.max())
    return np.floor((f-lmin)/(lmax-lmin)*255.)

""" Callback Function """
# Select the parent folder
def pushbutton_path_callback(*args):
    global pList
    
    pList = list()   # The plates' name

    pushbutton_path.configure(text = "Validating...",style='my.TButton')

    # Ask the user to select a folder
    tempdir = filedialog.askdirectory(parent=root, initialdir=pparent.get(), title='Please select the PARENT directory')
    if len(tempdir) > 0:
        print("You chose %s",tempdir)

    # Enforce a valid parent directory
    if verifyValidParentDirectory(tempdir) :
        pparent.set(tempdir)
        s.configure('path.TButton',background = "gray")
    else:
        s.configure('path.TButton',background = "red")
    
    box_plate.delete(0,"end")
    pList = verifyDirlist(pparent.get(),True,"","_Analysis")[1]
    # Update the list in the box
    for p in pList:
        box_plate.insert("end", p)

    pushbutton_path.configure(text = "Change path")
    return

# Select the plate
def OnDouble(event):
    global plate_path, plate_path_ana
    widget = event.widget
    selection = widget.curselection()
    value = widget.get(selection[0])
    plate_path_ana = os.path.join(pparent.get(),"_Analysis",value,"Session data") # The plate folders in the _Analysis folder
    plate_path = os.path.join(pparent.get(), value) # The full path name of the selected plate
    sList = verifyDirlist(plate_path, True)[1]   # Sessions contained in the selected plate folder
    box_session.delete(0,"end")
    for s in sList:
        box_session.insert("end", s)
    return

# Functions to manage group names
def pushbutton_add_callback(*args):
    text = group_entry.get()
    box_group.insert("end",text)
    return

def pushbutton_delete_callback(*args):
    selection = box_group.curselection()
    for i in reversed(selection):
        print (box_group.get(i))
        box_group.delete(i)
    return

def pushbutton_change_callback(*args):
    return


# Create the canvas for displaying images; import and swap
class images():
    
    def __init__(self, main):
        self.canvas = tk.Canvas(main, width=500, height=359) # Canvas for displaying the plate image
        self.canvas.grid(row=1, column=1, columnspan = 3, padx = 10, pady = 10)
        self.button_swap = ttk.Button(f1,text="Swap frame", command = self.pushbutton_swap_callback)
        self.button_swap.grid(row=2, column=3, sticky = 'nw')
        self.button_zout = ttk.Button(f1,text="Zoom Out", command = self.pushbutton_zout_callback)
        self.button_zout.grid(row=0, column=3, sticky='e', padx = 25, pady = 10)
        self.fr_val = 0                     # Initial frame
        self.fr_jump = 12
        self.bttn_clicks = 0
        self.ratio = 7.68                   # the size of the original image/the size of the image putted on GUI

        # Zoomer type one
        self.zoomcycle = 0
        self.zimg_id = None
        root.bind("<MouseWheel>",self.zoomer)
        self.canvas.bind("<Motion>",self.crop)

        # Zoomer type two
        self.canvas.bind("<ButtonPress-1>", self.on_button_press)
        self.canvas.bind("<B1-Motion>", self.on_move_press)
        self.canvas.bind("<ButtonRelease-1>", self.on_button_release)
        self.rect = None
        self.start_x = None
        self.start_y = None

    # Called when double click in session box
    def ImportImage(self,event):
        self.zoom = 0             # 0 for normal (zoom out) status, 1 for zoom in status
        
        self.bttn_clicks = 0       # Renew the bottun-click number when new session chosen
        self.fr_val = 0
        self.fr_jump = 12

        input_frame.set(self.fr_val)
        input_fr_jump.set(self.fr_jump)
        
        widget = event.widget
        selection = widget.curselection()
        value = widget.get(selection[0])                        # The session selected
        session_path = os.path.join(plate_path, value)
        
        images.imgList = verifyDirlist(session_path, False, ".png")[0]  # All images' full path name in the selected session folder
        tot_frame.set(str(len(images.imgList)))
        path1 = images.imgList[self.fr_val]                     # The full path name of the current image
        self.image_open = Image.open(path1).resize((500, 359),Image.ANTIALIAS)
        self.img = ImageTk.PhotoImage(self.image_open)
        self.image_on_canvas = self.canvas.create_image(0, 0, image=image1.img, anchor= 'nw')
        
        image2.delete("all")                                    # Delete previous difference image when a new session chosen

        path_default = images.imgList[self.fr_val+self.fr_jump]
        self.image_open_jump = Image.open(path_default).resize((500, 359),Image.ANTIALIAS)
        diff = self.XYdiff()                                    # Default difference image with 12 frames-difference


        return

    # If the initial frame is changed manually
    def change_fr(self, *args):
        self.bttn_clicks = 0
        self.fr_val = int(input_frame.get())
        fr_jump = int(input_fr_jump.get())
        path = images.imgList[self.fr_val]
        self.image_open = Image.open(path).resize((500, 359),Image.ANTIALIAS)
        path_default = images.imgList[self.fr_val+fr_jump]
        self.image_open_jump = Image.open(path_default).resize((500, 359),Image.ANTIALIAS)
        diff = self.XYdiff()  
        if self.zoom == 0:
            self.image_new = ImageTk.PhotoImage(self.image_open)
            self.canvas.itemconfig(self.image_on_canvas, image = self.image_new)
            
        else:
            self.image_new_crop = self.image_open.crop((self.start_x, self.start_y, self.curX, self.start_y+0.718*(self.curX-self.start_x)))
            self.image_new = ImageTk.PhotoImage(self.image_new_crop.resize((500, 359),Image.ANTIALIAS))
            self.canvas.itemconfig(self.image_on_canvas, image = self.image_new)
            rect_Diffimg = ImageTk.PhotoImage(self.new_im.crop((self.start_x, self.start_y, self.curX, self.start_y+0.718*(self.curX-self.start_x))))
            show_Diffimg = image2.itemconfig(self.imgDiff_on_canvas, image = rect_Diffimg)

    def pushbutton_swap_callback(self):
         
        # When the frames to jump doesn't change
        if self.fr_jump == int(input_fr_jump.get()):
            self.bttn_clicks += 1                     # Count clicks
        
        else:               # Make the current frame as the first one
            self.bttn_clicks = 1
            image2.delete("all")
            path = images.imgList[self.fr_val]
            self.image_open = Image.open(path).resize((500, 359),Image.ANTIALIAS)
        
        self.fr_jump = int(input_fr_jump.get())       # Number of frames to jump

        if self.bttn_clicks % 2 == 0:                 # Even; desplay the initial one
            path2 = images.imgList[self.fr_val-self.fr_jump]
            self.fr_val = self.fr_val-self.fr_jump                  # current frame 
            self.image_open_jump = Image.open(path2).resize((500, 359),Image.ANTIALIAS)
        else:                                         # Odd
            path2 = images.imgList[self.fr_val+self.fr_jump]
            self.fr_val = self.fr_val+self.fr_jump
            self.image_open_jump = Image.open(path2).resize((500, 359),Image.ANTIALIAS)
            diff = self.XYdiff()
        
        self.img_jump_orig = ImageTk.PhotoImage(self.image_open_jump)
        input_frame.set(self.fr_val)
        if self.zoom == 1: # 1st time zoom in
            size = 500, 359
            
            self.rect_img_jump = self.image_open_jump.crop((self.start_x, self.start_y, self.curX, self.start_y+0.718*(self.curX-self.start_x)))
            self.crop_img_jump = ImageTk.PhotoImage(self.rect_img_jump.resize(size))
            self.show_img_jump = self.canvas.itemconfig(self.image_on_canvas, image = self.crop_img_jump)
            self.show_Diffimg = image2.itemconfig(self.imgDiff_on_canvas, image = self.crop_Diffimg)
        
        elif self.zoom == 0: # On original scale
            self.img_jump = ImageTk.PhotoImage(self.image_open_jump)
            self.canvas.itemconfig(self.image_on_canvas, image = self.img_jump)
            
        else:
            messagebox.showerror("Error","Swap only allowed for the 1st zooming in")

        return

    def XYdiff(self):
        
        if self.bttn_clicks == 1 or self.bttn_clicks == 0:      # Generate the difference image only for the first click
            difference = ImageChops.difference(self.image_open, self.image_open_jump)
            np_im = np.array(difference)                        # Convert to numpy array to normalized
            np_im = normalize(np_im)
            new_im = Image.fromarray(np_im)                     # Convert back to image
            self.new_im = new_im.convert("L")
        
        self.imgDiff = ImageTk.PhotoImage(self.new_im)
        self.imgDiff_on_canvas = image2.create_image(0, 0, image=self.imgDiff, anchor= 'nw')
        return 

    """Zoom mode 1"""
    def zoomer(self,event):
        if (event.delta > 0):
            if self.zoomcycle != 4: self.zoomcycle += 1
        elif (event.delta < 0):
            if self.zoomcycle != 0: self.zoomcycle -= 1
        self.crop(event)

    def crop(self,event):
        if self.zimg_id: self.canvas.delete(self.zimg_id)
        if (self.zoomcycle) != 0:
            x,y = event.x, event.y
            if self.zoomcycle == 1:
                tmp = self.image_open.crop((x-55,y-40,x+55,y+40))
                tmp2 = self.new_im.crop((x-55,y-40,x+55,y+40))
            elif self.zoomcycle == 2:
                tmp = self.image_open.crop((x-40,y-30,x+40,y+30))
                tmp2 = self.new_im.crop((x-40,y-30,x+40,y+30))
            elif self.zoomcycle == 3:
                tmp = self.image_open.crop((x-15,y-10,x+15,y+10))
                tmp2 = self.new_im.crop((x-15,y-10,x+15,y+10))
            elif self.zoomcycle == 4:
                tmp = self.image_open.crop((x-6,y-4,x+6,y+4))
                tmp2 = self.new_im.crop((x-6,y-4,x+6,y+4))
            size = 300,200
            self.zimg = ImageTk.PhotoImage(tmp.resize(size))
            self.zimg2 = ImageTk.PhotoImage(tmp2.resize(size))
            self.zimg_id = self.canvas.create_image(event.x,event.y,image=self.zimg)
            self.zimg_id2 = image2.create_image(event.x,event.y,image=self.zimg2)
    
    """Zoom mode 2"""
    def on_button_press(self, event):
        # save mouse drag start position
        self.start_x = event.x
        self.start_y = event.y

        # create rectangle 
        self.rect = self.canvas.create_rectangle(self.start_x, self.start_y, self.start_x+0.1, self.start_y+0.1, fill="")
        
    def on_move_press(self, event):
        self.curX, self.curY = (event.x, event.y)

        # expand rectangle as dragging the mouse
        self.canvas.coords(self.rect, self.start_x, self.start_y, self.curX, self.start_y+0.718*(self.curX-self.start_x))
        if self.zoom >= 1:                          # If further zoom in needed
            im111 = Image.open(save_path)
            im222 = Image.open(save_path_D)
            self.rect_img = im111.crop((self.start_x, self.start_y, self.curX, self.start_y+0.718*(self.curX-self.start_x)))
            self.rect_Diffimg = im222.crop((self.start_x, self.start_y, self.curX, self.start_y+0.718*(self.curX-self.start_x)))
        else:                                       # First time zooming in
            if self.bttn_clicks == 0:               # Zoom in before any swapping
                self.rect_img = self.image_open.crop((self.start_x, self.start_y, self.curX, self.start_y+0.718*(self.curX-self.start_x)))
            else:
                self.rect_img = self.image_open_jump.crop((self.start_x, self.start_y, self.curX, self.start_y+0.718*(self.curX-self.start_x)))
            self.rect_Diffimg = self.new_im.crop((self.start_x, self.start_y, self.curX, self.start_y+0.718*(self.curX-self.start_x)))

    def on_button_release(self, event):
        global save_path, save_path_D

        self.zoom += 1
            
        size = 500, 359
        
        # Save the cropped image for further zooming in
        save_path = os.path.join(pparent.get(),"image111.png")
        save_path_D = os.path.join(pparent.get(),"image222.png")
        self.rect_img = self.rect_img.resize(size)
        self.rect_Diffimg = self.rect_Diffimg.resize(size)
        self.rect_img.save(save_path)
        self.rect_Diffimg.save(save_path_D)
        
        self.crop_img = ImageTk.PhotoImage(self.rect_img)
        self.crop_Diffimg = ImageTk.PhotoImage(self.rect_Diffimg)
        self.show_img = self.canvas.itemconfig(self.image_on_canvas, image = self.crop_img)
        self.show_Diffimg = image2.itemconfig(self.imgDiff_on_canvas, image = self.crop_Diffimg)

        self.canvas.delete(self.rect)

    def pushbutton_zout_callback(self):
        self.zoom = 0
        if self.bttn_clicks >= 1:
            self.show_img = self.canvas.itemconfig(self.image_on_canvas, image = self.img_jump_orig)
        else:
            self.show_img = self.canvas.itemconfig(self.image_on_canvas, image = self.img)
        self.show_Diffimg = image2.itemconfig(self.imgDiff_on_canvas, image = self.imgDiff)
        return

""" Layout """
# Main script
root = tk.Tk()
root.geometry("1500x660")
root.title("ImageGUI")
root.resizable(False, False)
bgcolor = (241,241,245)
root.columnconfigure(0, weight=1)
root.columnconfigure(1, weight=1)
root.rowconfigure(0, weight=1)

""" Left Frame (configuration panel) """

s=ttk.Style()
#s.theme_use('vista')
#print(s.theme_names())
s.configure('M.TFrame', borderwidth=1, background='lightgrey')
s.configure('A.TFrame', borderwidth=1, background='white')

frame1 = ttk.Frame(root, width=500,height=660, style = 'M.TFrame')
frame1.grid(row = 0, column = 0, rowspan = 13, columnspan = 5, sticky = 'nsw')
frame1.columnconfigure(0, weight=1) 

L_label1 = ttk.Label(frame1, text = "Parent Folder", font = "Helvetica 16 bold", justify=tk.CENTER, background='lightgrey')
L_label1.grid(row = 0, column = 1, columnspan = 5, sticky = 'n')
pparent = tk.StringVar(root)
pparent.set(r"E:\WI_Project")
L_label2 = ttk.Label(frame1, textvariable = pparent, font='Helvetica 12',wraplength=800, justify=tk.LEFT, background='lightgrey') # Fullpathname of parent folder
L_label2.grid(row = 1, column = 1, columnspan = 5, sticky = 'n')

L_label_emp0 = ttk.Label(frame1, text = " ", background='lightgrey') 
L_label_emp0.grid(row = 3, column = 1, columnspan = 5, sticky = 'n')
L_label_emp1 = ttk.Label(frame1, text = "   ", background='lightgrey') 
L_label_emp1.grid(row = 0, rowspan = 13, column = 0, sticky = 'w')
L_label_emp1 = ttk.Label(frame1, text = "   ", background='lightgrey') 
L_label_emp1.grid(row = 0, rowspan = 13, column = 5, sticky = 'e')

# Botton to change folder
s.configure('my.TButton', font=('Helvetica', 12))
pushbutton_path = ttk.Button(frame1,text="Change path",command = pushbutton_path_callback, style = 'my.TButton')
pushbutton_path.grid(row = 2, column = 1, columnspan = 5, sticky = 'n')

# Text input to add group
input_text = tk.StringVar()   
group_entry = ttk.Entry(frame1, textvariable = input_text, justify = tk.CENTER, width = 15) 
group_entry.grid(row = 8, column = 3, padx = 10, sticky = 'n')

# Botton to change/delete/add group
s.configure('C.TButton', font=('Helvetica', 12))
pushbutton_add = ttk.Button(frame1,text="Add group",command = pushbutton_add_callback, style = 'C.TButton')
pushbutton_add.grid(row = 9, column = 3, sticky = 'n')
s.configure('A.TButton', font=('Helvetica', 12))
pushbutton_change = ttk.Button(frame1,text="Change group",command = pushbutton_change_callback, style = 'A.TButton')
pushbutton_change.grid(row = 11, column = 3, sticky = 'n')
s.configure('B.TButton', font=('Helvetica', 12))
pushbutton_delete = ttk.Button(frame1,text="Delete group",command = pushbutton_delete_callback, style = 'B.TButton')
pushbutton_delete.grid(row = 10, column = 3, sticky = 'n')

# List boxes
box_plate = tk.Listbox(frame1)
plate_label = ttk.Label(frame1, text = "Plate", font='Helvetica 12 bold', background='lightgrey')
plate_label.grid(row = 5, column = 1)
scroll_p = ttk.Scrollbar(frame1, orient=tk.VERTICAL)
box_plate.config(yscrollcommand=scroll_p.set)
scroll_p.configure(command=box_plate.yview)
box_plate.grid(row = 6, column = 1, stick='e')
scroll_p.grid(row=6,column=2,sticky='nsw')
box_plate.bind("<Double-Button-1>", OnDouble)

box_session = tk.Listbox(frame1)
session_label = ttk.Label(frame1, text = "Session", font='Helvetica 12 bold', background='lightgrey')
session_label.grid(row = 5, column = 3)
scroll_s = ttk.Scrollbar(frame1, orient=tk.VERTICAL)
box_session.config(yscrollcommand=scroll_s.set)
scroll_s.configure(command=box_session.yview)
scroll_s.grid(row=6,column=4,sticky='nsw')
box_session.grid(row = 6, column = 3)


box_group = tk.Listbox(frame1, selectmode='multiple')
group_label = ttk.Label(frame1, text = "Group", font='Helvetica 12 bold', background='lightgrey')
group_label.grid(row = 7, column = 1)
scroll_g = ttk.Scrollbar(frame1, orient=tk.VERTICAL)
box_group.config(yscrollcommand=scroll_g.set)
scroll_g.configure(command=box_group.yview)
scroll_g.grid(row=8, rowspan = 4, column=2,sticky='nswe')
box_group.grid(row = 8, rowspan = 4, column = 1)
box_group.insert(0, "censor")


""" Right Frame (actually a notebook) """

n = ttk.Notebook(root, height = 500, width = 866)
f1 = ttk.Frame(n, style="M.TFrame")   # first page, which would get widgets gridded into it
f2 = ttk.Frame(n, style="M.TFrame")   # second page
n.add(f1, text='Hybrid View')
n.add(f2, text='Activity View')
n.grid(row = 0, column = 1, rowspan = 15, columnspan = 5, sticky = 'nswe')


""" PANE 1: Hybrid view """

label1 = ttk.Label(f1, text = "Frame ", font = "Helvetica 12", justify=tk.CENTER, background='lightgrey')
label1.grid(row=0, column=1, pady = 10, sticky='e')
input_frame = tk.StringVar(value=0)
curr_frm = ttk.Entry(f1, textvariable = input_frame, justify = tk.CENTER, width = 3) 

curr_frm.grid(row = 0, column = 2, sticky='w', padx = 25, pady = 10)
label1_1 = ttk.Label(f1, text = "of ", font = "Helvetica 12", justify=tk.LEFT, background='lightgrey')
label1_1.grid(row=0, column=2, pady = 10)
tot_frame = tk.StringVar(value='#')
label1_2 = ttk.Label(f1, textvariable = tot_frame, font='Helvetica 12', justify=tk.LEFT, background='lightgrey')
label1_2.grid(row=0, column=2, sticky='e', padx = 25, pady = 10)

label2 = ttk.Label(f1, text = "Frame Y - Frame X", font = "Helvetica 12", justify=tk.CENTER, background='lightgrey')
label2.grid(row=0, column=4, columnspan = 3, pady = 10, padx = 10)

P1_label_emp0 = ttk.Label(f1, text = "    ", width = 12, background='lightgrey') 
P1_label_emp0.grid(row = 0, column = 0, rowspan = 6, sticky = 'w')
P1_label_emp0 = ttk.Label(f1, text = "    ", width = 12, background='lightgrey') 
P1_label_emp0.grid(row = 0, column = 7, rowspan = 6, sticky = 'e')


# Canvas where image displayed
image1 = images(f1)
box_session.bind("<Double-Button>", image1.ImportImage) # import the image when double clicking one session
curr_frm.bind("<Return>", image1.change_fr) # When the frame change, the image displayed will be changed


image2 = tk.Canvas(f1, width=500, height=359)
image2.grid(row=1, column=4, columnspan = 3, padx = 10, pady = 10)

# Frames selection
label3 = ttk.Label(f1, text = "Jump = ", font = "Helvetica 12", justify=tk.RIGHT, background='lightgrey')
label3.grid(row = 2, column = 1, sticky = "ne")
input_fr_jump = tk.StringVar(value=12)   
fr_entry = ttk.Entry(f1, textvariable = input_fr_jump, justify = tk.CENTER, width = 6) 
fr_entry.grid(row = 2, column = 2, sticky = 'nw')
label4 = ttk.Label(f1, text = "Frames (max xx)", font = "Helvetica 12", justify=tk.LEFT, background='lightgrey')
label4.grid(row = 2, column = 2, sticky = "n")

image3 = tk.Canvas(f1, width=900, height=150)
image3.grid(row=3, column=1, columnspan = 6, padx = 5, pady = 10)

""" PANE 2: Activity View """
label1 = ttk.Label(f2, text = "Activity", font = "Helvetica 12", justify=tk.CENTER, background='white')
label1.grid(row=0, column=0, pady = 10, padx = 5, sticky = 'w')
image4 = tk.Canvas(f2, width=900, height=150)
image4.grid(row=1, column=0, columnspan = 3, padx = 20, pady = 5)
image4.create_line(0, 0, 900, 150)
image4.create_line(0, 150, 900, 0, fill="red", dash=(4, 4))

label2 = ttk.Label(f2, text = "Simulated Activity", font = "Helvetica 12", justify=tk.CENTER, background='white')
label2.grid(row=2, column=0, pady = 10, padx = 5, sticky = 'w')
image5 = tk.Canvas(f2, width=900, height=150)
image5.grid(row=3, column=0, columnspan = 3, padx = 20, pady = 5)
image5.create_line(0, 0, 900, 150)
image5.create_line(0, 150, 900, 0, fill="red", dash=(4, 4))

label3 = ttk.Label(f2, text = "Unsimulated Activity", font = "Helvetica 12", justify=tk.CENTER, background='white')
label3.grid(row=4, column=0, pady = 10, padx = 5, sticky = 'w')
image6 = tk.Canvas(f2, width=900, height=150)
image6.grid(row=5, column=0, columnspan = 3, padx = 20, pady = 5)
image6.create_line(0, 0, 900, 150)
image6.create_line(0, 150, 900, 0, fill="red", dash=(4, 4))

P2_label_emp0 = ttk.Label(f2, text = "    ", background='lightgrey') 
P2_label_emp0.grid(row = 6, column = 0, columnspan = 3, pady = 10, sticky = 's')



def on_closing():
    global save_path, save_path_D
    if messagebox.askokcancel("Quit", "Do you want to quit?"):
        try:
            os.remove(save_path)
            os.remove(save_path_D)
        except:
            pass
        root.destroy()

root.protocol("WM_DELETE_WINDOW", on_closing)

root.mainloop()
