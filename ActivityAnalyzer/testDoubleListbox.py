try:
    import tkinter as tk
except ImportError:
    import Tkinter as tk


class App(tk.Tk):
    def __init__(self, *args, **kwargs):
        tk.Tk.__init__(self, *args, **kwargs)
        self.resizable(width=False, height=False)

        # list variables
        self.list_var1 = tk.StringVar()
        self.list_var2 = tk.StringVar()

        # main frame
        self.main_frame = tk.Frame(self)

        self.listbox1 = tk.Listbox(self.main_frame, listvariable=self.list_var1, selectmode='multiple')
        self.listbox2 = tk.Listbox(self.main_frame, listvariable=self.list_var2, selectmode='multiple')

        # little button frame
        self.button_frame = tk.Frame(self.main_frame)

        self.all_to_right_button = tk.Button(self.button_frame, text='>>', command=self.move_to_right)
        self.one_to_right_button = tk.Button(self.button_frame, text='>', command=lambda: self.move_to_right(True))
        self.one_to_left_button = tk.Button(self.button_frame, text='<', command=lambda: self.move_to_left(True))
        self.all_to_left_button = tk.Button(self.button_frame, text='<<', command=self.move_to_left)

        # packing
        self.all_to_right_button.pack()
        self.one_to_right_button.pack()
        self.one_to_left_button.pack()
        self.all_to_left_button.pack()

        self.listbox1.pack(side='left', anchor='w')
        self.button_frame.pack(side='left')
        self.listbox2.pack(side='right', anchor='e')
        self.main_frame.pack()

        # insert default values
        self.init_default_values()

    def init_default_values(self):
        self.list_var1.set(value=[1, 2, 3, 4, 5])
        self.list_var2.set(value=[1, 2, 3])

        # you can test with literals
        # self.list_var1.set(value=['A', 'B', 'C', 'D', 'E'])
        # self.list_var2.set(value=['A', 'B', 'C'])

    def move_to_right(self, only_one_item=False):

        if self.listbox1.curselection() == ():
            return

        # get tuple of selected indices
        if only_one_item:
            selection = (self.listbox1.curselection()[0],)
        else:
            selection = self.listbox1.curselection()

        # left all/selected values
        left_value_list = [line.strip(' \'') for line in self.list_var1.get()[1:-1].split(',')]
        left_selected_list = [left_value_list[index] for index in selection]

        # values from right side
        right_value_list = [line.strip(' \'') for line in self.list_var2.get()[1:-1].split(',')]

        # merge w/o duplicates
        result_list = sorted(list(set(right_value_list + left_selected_list)))

        self.list_var2.set(value=result_list)

    def move_to_left(self, only_one_item=False):

        if self.listbox2.curselection() == ():
            return

        # get tuple of selected indices
        if only_one_item:
            selection = (self.listbox2.curselection()[0],)
        else:
            selection = self.listbox2.curselection()

        # right all/selected values
        right_value_list = [line.strip(' \'') for line in self.list_var2.get()[1:-1].split(',')]
        right_selected_list = [right_value_list[index] for index in selection]

        # values from left side
        left_value_list = [line.strip(' \'') for line in self.list_var1.get()[1:-1].split(',')]

        # merge w/o duplicates
        result_list = sorted(list(set(left_value_list + right_selected_list)))

        self.list_var1.set(value=result_list)

app = App()
app.mainloop()
