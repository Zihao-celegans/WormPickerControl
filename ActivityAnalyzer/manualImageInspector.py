""" TODO 10/1/2019

    1. Get image size as variable (Solved)
    2. Add LOTS of comments so Anthony can understands things
    3. Delete the path paste widget and replace with a simple label that shows the path (Solved)
    4. Image display has wrong aspect ratio (Solved)
    5. Represent each well and properties of the well (Solved)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             
    6. Remove confirm buttons, use .trace method on the StringVar variable(Solved)
    7. Delete duplicate window when changing day value(Solved)
    8. Writing csvio function: replace a certain header by new data
    9. read the existing grouping data when a plate is selected

"""


# Standard imports
import numpy as np
import matplotlib.pyplot as plt
import matplotlib.image as mpimg                      
import os
import csv
import time
import tkinter as tk
import string
from tkinter import messagebox
import os.path
from PIL import Image
from PIL import ImageTk # pip install pillow
#from scipy.misc import imread, imresize
import cv2

from tkinter import filedialog
from matplotlib import cm
import matplotlib
from matplotlib.pyplot import plot, draw, show, ion
#from pylab import *

# Anthony's imports
import Calculations as calc
from csvio import readArrayFromCsv, printArrayToCsv
import AnalysisPlots as ap
import ErrorNotifier
from verifyDirlist import verifyDirlist


def manualImageInspection (pparent):

    Child_root = tk.Tk()
    Child_root.geometry("350x900")
    
    global PD
    PD = tk.StringVar() # parent directory
    var = tk.StringVar()
    var_2 = tk.StringVar()
    global var_3
    var_3 = tk.StringVar()
    radio_var = tk.StringVar()
    list1 = list()
    list2 = list()
    global plate_number
    plate_number = tk.StringVar()
    input_cate = tk.StringVar()
    global list_Category
    list_Category = list()
    Color_cate = tk.StringVar()
    Color_marker = tk.StringVar() ## read by computer #fb0 orange #f11 red #05f blue #1f1 green
    list_Category.append('No group')
    list_Category.append('Censor')
    mouse_pos_x = []
    mouse_pos_y = []
    global Index_cate
    global Group_well
    Group_well = np.zeros((24,), dtype=int)
    Group_marker_color = ['#ffffff' for x in range(24)]
    global Group_marker_color_array
    Group_marker_color_array = np.asarray(Group_marker_color)
    global print_to_csv
    print_to_csv = list()
    global ratio
    global image_folder
    global list_day
    global plate_folder
    list_day = list()
    list_day.append('<None>')
    ratio = 4


    def Callback_Cfrm_plate(*args):
        global image_folder
        global list_day
        global plate_folder
        global Group_marker_color_array
        global Group_well

        Group_well = np.zeros((24,), dtype=int)

        Group_marker_color = ['#ffffff' for x in range(24)]
        Group_marker_color_array = np.asarray(Group_marker_color)


        droplist_2['menu'].delete(0, 'end')

        plate_number = var.get()
        plate_folder = os.path.join(image_folder,plate_number)

        #print(plate_folder)

        list_plate_day = verifyDirlist(plate_folder,True,"","")
        list_day = list_plate_day[1]

        for day in list_day:
            droplist_2['menu'].add_command(label = day, command = tk._setit(var_2, day))

        
        interm_filename = os.path.join(Parent_Directory,"_Analysis",plate_number)

        file_to_read = verifyDirlist(interm_filename, False, ".csv", "")
        csv_to_read = file_to_read[0]

        Grouping_data = readArrayFromCsv(csv_to_read[0], '<Group name>')

        print(Grouping_data)


    def Callback_Cfrm_day(*args):

        global mouse_pos_x
        global mouse_pos_y
        global ratio
        global plate_folder
        global Group_marker_color_array

        day_folder = var_2.get()

        print(day_folder)

        plate_day = os.path.join(plate_folder,day_folder)
        list_plate_day = verifyDirlist(plate_day,False,"","")
        day_list = list_plate_day[1]
        first_image = day_list[0]
        first_image_path = os.path.join(plate_day,first_image)
        #print(first_image_path)

        raw_image = cv2.imread(first_image_path)
        shape_img = np.shape(raw_image)
        x = round(shape_img[0]/ratio)
        y = round(shape_img[1]/ratio)
        resized_imge = cv2.resize(raw_image, (y, x))


        plt.close('all')

        fig = plt.figure()
        fig.set_size_inches(12, 8)
        image_plot = plt.imshow(resized_imge)
        
        show(block = False)

        ## Synthesize the path of corresponding ROI data

        Parent_Directory = PD.get()
        plate_number = var.get()
        day_number = var_2.get()
        intm_filename = os.path.join(Parent_Directory,"_Analysis",plate_number,"Session data")
    
        ROI_csv_path = verifyDirlist(intm_filename,False,day_number,"")
        fullfilename_list = ROI_csv_path[0]
        fullfilename = fullfilename_list[0]
        #print(fullfilename)
        header = '<ROI Coordinates>'

        ## read ROI data from csv file
        ROI = readArrayFromCsv(fullfilename,header)
        x_position = ROI[0]
        y_position = ROI[1]
        Inn_rad = ROI[2]
        Out_rad = ROI[3]
        x_pstn_array = np.true_divide(x_position, ratio)
        y_pstn_array = np.true_divide(y_position, ratio)
        rad_array = np.true_divide(Out_rad, ratio)

        # draw the circle marker
        




        if sum(Group_well) != 0:
             for iter_marker in range(len(Group_well)):

                 center_x_iter_mark = x_pstn_array[iter_marker]
                 center_y_iter_mark = y_pstn_array[iter_marker]
                 radius_iter_mark = rad_array[iter_marker]
                 
                 if Group_marker_color_array[iter_marker] == '#ffffff':
                     LINE = 4
                 else:
                     LINE = 4

                 circle_marker = plt.Circle((center_x_iter_mark, center_y_iter_mark), radius_iter_mark, color = Group_marker_color_array[iter_marker], linewidth = LINE, fill = False)
                 ax = fig.gca()
                 ax.add_artist(circle_marker)
                 draw()


        def mouse_click (event):

            global Color_marker
            global mouse_pos_x
            global mouse_pos_y
            global Group_well
            global Group_marker_color_array

            print(event.xdata);
            print(event.ydata);
           
            mouse_pos_x = event.xdata
            mouse_pos_y = event.ydata

            in_cir_post = isInside_multi_circle(x_pstn_array, y_pstn_array, rad_array, mouse_pos_x, mouse_pos_y)
            print(in_cir_post)


            outline_color = Color_marker

            #print(Index_cate)

            center_x = x_pstn_array[in_cir_post]
            center_y = y_pstn_array[in_cir_post]

            center_x_value = center_x[0]
            center_y_value = center_y[0]


            radius = rad_array[in_cir_post]
            radius_value = radius[0]

            
            if Index_cate > 1:
                color_marker_local = outline_color
            else:
                if Index_cate == 0:
                    color_marker_local = '#ffffff'
                else:
                    if Index_cate == 1:
                        color_marker_local =  '#800000'

            if color_marker_local == '#ffffff':
                     LINE_WID = 4
            else:
                     LINE_WID = 4

            circle_marker = plt.Circle((center_x_value, center_y_value), radius_value, color = color_marker_local, linewidth = LINE_WID, fill = False)
            ax = fig.gca()
            ax.add_artist(circle_marker)
            draw()

            Group_well[in_cir_post] = Index_cate
            Group_marker_color_array[in_cir_post] = color_marker_local
        

        cid = fig.canvas.mpl_connect('button_press_event', mouse_click)
        



    def Add_cate():
        global list_Category
        New_cate = input_cate.get()
        len_list_category = len(list_Category)
        list_Category.append(New_cate)
        list_Category = list(dict.fromkeys(list_Category))

        droplist_cate['menu'].delete(0, 'end')
        for cate in list_Category:
            droplist_cate['menu'].add_command(label = cate, command = tk._setit(var_3, cate))
        
    def Dele_Cate():
        Category_to_del = input_cate.get()
        for i in list_Category:
            if(i == Category_to_del):
                list_Category.remove(Category_to_del)
        
        droplist_cate['menu'].delete(0, 'end')
        for cate in list_Category:
            droplist_cate['menu'].add_command(label = cate, command = tk._setit(var_3, cate))
        
    def Dele_all_Cate():
        if len(list_Category) > 2 :
            del list_Category[2:len(list_Category)]
        
        droplist_cate['menu'].delete(0, 'end')
        for cate in list_Category:
            droplist_cate['menu'].add_command(label = cate, command = tk._setit(var_3, cate))

    def Callback_cfrm_cate(*args):
        global Index_cate
        global list_Category
        global Color_marker

        col_map_array = list()

        Div = len(list_Category)-1

        cmap = cm.get_cmap('jet', Div)

        for i in range(cmap.N):
            rgb = cmap(i)[:3] # will return rgba, we take only first 3 so we get rgb
            col_map_array.append(matplotlib.colors.rgb2hex(rgb))
     
        Sltd_cate = var_3.get()
    
        col_map_array_shift = col_map_array[-1:] + col_map_array[:-1]  

        Index_cate = list_Category.index(Sltd_cate)
    
        Color_marker = col_map_array_shift[(Index_cate-1)]

    def isInside(circle_x, circle_y, rad, x, y):
        if((x - circle_x)*(x - circle_x)+(y-circle_y)*(y-circle_y) <= rad*rad):    
            return 1
        else: 
            return 0

    def isInside_multi_circle(x_position_array, y_position_array, rad_array, x, y):
        num_x_pos = len(x_position_array)
        num_y_pos = len(y_position_array)
        num_rad = len(rad_array)
        iswithin = []
        for m in range(num_x_pos):
            Tem_val = isInside(x_position_array[m], y_position_array[m], rad_array[m], x, y)
            iswithin.append(Tem_val)
    
        #print(len(iswithin))
        non_zero_pos = np.nonzero(iswithin)
        #print(non_zero_pos[0])
        return non_zero_pos[0]

    def cfrm_marking():
        global plate_number
        global PD
        global list_Category
        global Group_well
        global print_to_csv
        for k in range(len(Group_well)):
            print_to_csv.append(list_Category[Group_well[k]])

        print_to_csv_array = np.array(print_to_csv).reshape((1,24))

        Parent_Directory = PD.get()
        Plt_num = var.get()
        destination_folder = os.path.join(Parent_Directory,"_Analysis",Plt_num)
        destination_file = verifyDirlist(destination_folder, False, ".csv", "")
        destination_csv = destination_file[0]
        
        fout = os.path.join(destination_csv[0])

        col_offset = 2
        header_offset = 2

        insertDatatoCsv(fout, '<Group name>', '<end>', '<Group name>', print_to_csv_array, col_offset, header_offset)


    lab = tk.Label(Child_root, text = "WWInspector", font = ("arial", 25, "bold"))
    lab.pack()

    label0 = tk.Label(Child_root, text = "Plate data location", relief = "solid", width = 100, font = ("arial", 12))
    label0.place(x = -280, y = 80)

    label1 = tk.Label(Child_root, text = "Parent Directory:", width = 100, font = ("arial", 14, "bold"))
    label1.place(x = -430, y = 120)

    Pare_Dir = tk.Label(Child_root, text = pparent)
    Pare_Dir.place(x = 110, y = 150)

    PD.set(pparent)

    Parent_Directory = PD.get() # Get the parent directory
    list_plate_name = verifyDirlist(Parent_Directory,True,"","_Analysis")
    list1 = list_plate_name[1]
    droplist = tk.OptionMenu(Child_root, var, *list1)
    droplist.config(width = 27)
    droplist.place(x = 35, y = 210)
    plate_number = var.get()
    
    image_folder = os.path.join(Parent_Directory,plate_number)
    var.trace("w", Callback_Cfrm_plate)

    label2 = tk.Label(Child_root, text = "Select Plate", width = 100, font = ("arial", 14, "bold"))
    label2.place(x = -508, y = 180)

    but_Res_Zo = tk.Button(Child_root, text = "Reset Zoom", width = 12, height = 3, font = ("arial", 12, "bold"))
    but_Res_Zo.place(x = 110,y = 260)

    label3 = tk.Label(Child_root, text = "Browse and annote", relief = "solid", width = 100, font = ("arial", 12))
    label3.place(x = -280, y = 370)

    label4 = tk.Label(Child_root, text = "Choose Day", width = 100, font = ("arial", 12))
    label4.place(x = -280, y = 400)


    droplist_2 = tk.OptionMenu(Child_root, var_2, *list_day)
    droplist_2.config(width = 27)
    droplist_2.place(x = 35, y = 430)
    var_2.trace("w", Callback_Cfrm_day)


    label5 = tk.Label(Child_root, text = "Add Categories Here", width = 100, font = ("arial", 12, "bold"))
    label5.place(x = -327, y = 480)

    custom_cate = tk.Entry(Child_root, textvar = input_cate)
    custom_cate.place(x = 110, y = 520)

    but_add_cate = tk.Button(Child_root, text = "Add", width = 8, height = 1, font = ("arial", 10),command = Add_cate)
    but_add_cate.place(x = 250,y = 515)

    but_dele_cate = tk.Button(Child_root, text = "Delete Category", width = 17, height = 1, font = ("arial", 10),command = Dele_Cate)
    but_dele_cate.place(x = 180,y = 555)

    but_dele_all_cate = tk.Button(Child_root, text = "Delete ALL Categories", width = 17, height = 1, font = ("arial", 10),command = Dele_all_Cate)
    but_dele_all_cate.place(x = 20,y = 555)

    label6 = tk.Label(Child_root, text = "Categories Added", width = 100, font = ("arial", 12))
    label6.place(x = -270, y = 596)


    droplist_cate = tk.OptionMenu(Child_root, var_3, *list_Category)
    droplist_cate.config(width = 27)
    droplist_cate.place(x = 35, y = 620)
    var_3.trace("w",Callback_cfrm_cate)

    label7 = tk.Label(Child_root, text = "Write to CSV file", width = 100, font = ("arial", 12))
    label7.place(x = -270, y = 650)


    but_cfrm_marking = tk.Button(Child_root, text = "Confirm", width = 8, height = 1, font = ("arial", 10),command = cfrm_marking)
    but_cfrm_marking.place(x = 130,y = 700)

    labelx = tk.Label(Child_root, text = "Fang-Yen Laboratory, University of Pennsylvania", width = 100, font = ("arial", 8))
    labelx.place(x = -140, y = 850)

                                                                                                                                                                                                                                                                                                                                                                                                                                                
    def on_closing():
        if messagebox.askokcancel("Quit", "Do you want to quit?"):
            Child_root.destroy()

    Child_root.protocol("WM_DELETE_WINDOW", on_closing)

    Child_root.mainloop()

if __name__ == "__main__" :
    manualImageInspection(r"F:\test_WI")