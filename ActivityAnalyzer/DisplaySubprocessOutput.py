"""
- read output from a subprocess in a background thread
- show the output in the GUI
https://stackoverflow.com/questions/665566/redirect-command-line-results-to-a-tkinter-gui
"""

import sys
from itertools import islice
from subprocess import Popen, PIPE
from textwrap import dedent
from threading import Thread
import tkinter as tk # Python 3
from queue import Queue, Empty # Python 3

def iter_except(function, exception):
    """Works like builtin 2-argument `iter()`, but stops on `exception`."""
    try:
        while True:
            yield function()
    except exception:
        return

class DisplaySubprocessOutput:
    def __init__(self, _root, _scrolltext, procargs):
        self.root = _root

        # start subprocess to generate some output
        self.process = Popen(procargs, stdout=PIPE, text=True)

        # launch thread to read the subprocess output
        #   (put the subprocess output into the queue in a background thread,
        #    get output from the queue in the GUI thread.
        #    Output chain: process.readline -> queue -> label)
        q = Queue(maxsize=2048)  # limit output buffering (may stall subprocess)
        t = Thread(target=self.reader_thread, args=[q])
        t.daemon = True # close pipe if GUI process exits
        t.start()

        # show subprocess' stdout in GUI
        self.lognum=0
        self.scrolltext = _scrolltext
        self.update(q) # start update loop

    def reader_thread(self, q):
        """Read subprocess output and put it into the queue."""
        try:
            with self.process.stdout as pipe:
                for line in iter(pipe.readline, b''):
                    q.put(line)
        finally:
            q.put(None)

    def update(self, q):
        """Update GUI with items from the queue."""
        for line in iter_except(q.get_nowait, Empty): # display all content
            if line is None:
                self.quit()
                return
            else:
                self.lognum += 1
                print('Printed: ', line)
                self.scrolltext.insert(tk.INSERT,"\n" + str(line),'tag' + str(self.lognum))

                if "Error" in line:
                    self.scrolltext.tag_config('tag' + str(self.lognum), foreground='red')  

                break # display no more than one line per 40 milliseconds
        self.root.after(40, self.update, q) # schedule next update

    def quit(self):
        self.process.kill() # exit subprocess if GUI is closed (zombie!)
