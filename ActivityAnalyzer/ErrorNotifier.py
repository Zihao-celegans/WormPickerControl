"""
    ErrorNotifier.py
    Anthony Fouad
    August 2019

    Notify the user of critical errors (warning window and exit) or warnings (log in file and show at end)
"""

import os
import ctypes
import sys
from datetime import datetime
from tkinter import messagebox
import tkinter.scrolledtext as tkst
import tkinter as tk
#from file_read_backwards import FileReadBackwards
from verifyDirlist import verifyDirlist
 



class ErrorNotifier :

    # Variables
    ferror = ""

    # Constructor, initilialize the error log file
    def __init__(self,_scrolltext = None):

        # Create an error file for today if it doesn't already exist
        today = datetime.today()
        today_time = datetime.now()
        today_str = today.strftime("%Y-%m-%d")
        today_time = datetime.now()
        today_time_str = today_time.strftime("%Y-%m-%d (%H-%M-%S)")
        perror = "C:\\WormWatcher\\logs\\"
        self.ferror = os.path.join(perror,"AnalysisLog_" + today_str + ".txt")
        self.notify("Log","-------------------------- Analysis Started ------------------------")
        self.notify("Log","Time:\t" + today_time_str + "\n")

        # Assign reference to the scrolling text window instead of printing
        self.scrolltext = _scrolltext
        self.lognum = 0;

    # Route errors to the appropriate error function by type
    def notify(self,err_type,err_msg) :

        if "Abort" in err_type :
            self.errorAbort(err_msg)

        elif "Log" in err_type :
            self.errorLog(err_msg)

        elif "Print" in err_type:
            self.errorLog(err_msg)
            print(err_msg)
            self.lognum += 1

            if self.scrolltext != None:
                self.scrolltext.insert(tk.INSERT,"\n" + err_msg,'tag' + str(self.lognum))

                if "Red" in err_type:
                    self.scrolltext.tag_config('tag' + str(self.lognum), foreground='red')  

                self.scrolltext.see("end")



        elif "Return" in err_type:
            self.errorAbort(err_msg,False)

        else :
            err_msg = "Unknown error type: " + err_msg
            self.errorAbort(err_msg) 

    # Scan through a long message, print any lines with "ERROR" as red
    def notifyLongMsg(self,err_msg):

        # Look for first line
        i0=0
        i1 = err_msg.find("\n")
        if i1 < 0:
            i1 = len(err_msg) - 1

        # Print each line 
        while (i1 < len(err_msg)) and (i0 < len(err_msg)):

            # Print this line
            this_line = err_msg[i0:i1]
            if "ERROR" in this_line.upper():
                self.notify("PrintRed",this_line)
            else:
                self.notify("Print",this_line)

            # Get next line
            i0 = i1+1

            if i0 >= len(err_msg):
                break

            # Find new i1 in this substring to the end
            i1 = err_msg[i0:-1].find("\n")

            if i1 < 0:
                # If at end set to end
                i1 = len(err_msg) - 1
            else:
                # Convert i1 back to absolut units, and break if OOB
                i1 += i0

            if i1 >= len(err_msg):
                break

    # Error type: Abort (with graphical notification)
    def errorAbort(self,err_msg,exit_flag = True) : 
        self.errorLog(err_msg)
        #ctypes.windll.user32.MessageBoxW(0, err_msg, "Analysis error", 0)
        messagebox.showerror("Error",err_msg)
        if exit_flag:
            sys.exit(-1)


    # Update the log file
    def errorLog(self,err_msg) : 

        # Verify that the error file exists and is openable
        outfile = self.validateLogFile(False)

        # Write the error message to the file
        outfile.write(err_msg)
        outfile.write("\n\n")

    # Try to open the log file
    def validateLogFile(self,close_file = True) : 
        try:
            outfile = open(self.ferror, "a+") # or "a+", whatever you need
        except IOError:
            ERROR = "Could not open error log file:\n" + self.ferror + "\nHave you opened it in another program?"
            ctypes.windll.user32.MessageBoxW(0, ERROR, "Analysis aborted", 0) # cannot use errorAbort - circular
            sys.exit(-1)

        if close_file :
            outfile.close()

        return outfile

        
