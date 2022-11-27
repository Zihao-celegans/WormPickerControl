"""

    WWShell.py
    Tkinter-based shell for running WormWatcher

"""


from subprocess import Popen, PIPE
import sys
from tkinter import *
from tkinter import ttk
import threading
import time

"""     Global variables        """

running = False
data = []
pipe = []

"""     THREAD FUNCTIONS for WW Communication       """

def readFromWW():
    
    global pipe
    
    for line in iter(pipe.stdout.readline,b''):
            print(line.decode())

       

    return


def writeToWW():

    global pipe

    while True:
        if(len(data)!=0):
            out_data = pipe.stdin.write(data[0].encode())
            pipe.stdin.flush() 
            data.clear()

        time.sleep(0.05)
    
    return


"""     Callback and helper functions      """

def startWW():

    global pipe
    pipe = Popen(r"C:\WormWatcher\WormWatcher.exe",stdout=PIPE, stdin=PIPE)



def sendData(to_send):
    global data
    data.append('27\n')
    return
    

"""     Threads     """


# Thread for reading from WW
rt = threading.Thread(target=readFromWW, args=())
rt.start()

# Thread for writing to WW
wt = threading.Thread(target=writeToWW, args=())
wt.start()


"""     Main tab structure     """

r = Tk()
r.title("WormWatcher")

tabControl = ttk.Notebook(r)#,width=500,height=600)
tabControl.pack()

inpt = ttk.Label(r,text="WormWatcher")
inpt.pack()
btn_start = ttk.Button(r,text="Start WormWatcher",command=startProcess)
btn_start.pack()
btn_stop = ttk.Button(r,text="Stop WormWatcher",command=stop_callback)
btn_stop.pack()

r.mainloop()
