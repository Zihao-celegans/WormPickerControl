"""
    CommandStringMaker.py

    Making robot-understandable command strings to be sent to WormPicker through
    socket

    Zihao Li
    Fang-Yen Lab
    Dec 2021
"""

'''
    IMPORTANT NOTE:
    The format of string send to the socket must be the exactly the following:

    "this_step_name" + "\t" + "r" + "\t" + "c" + "" + "this_step_action" + "\t"
    + "a0" + "\t" + "a1" + "\t" + "a2" + "\t" + "a3" + "\t" + "a4" + "\t" + "a5"
    + "\t" + "a6" + "\t" + "a7" + "\t" + "a8" + "\t" + "a9" + "\n"

    
    where "\t" is the delimiter for decoding the script entity at C++ side
    and "\n" is a flag denoting end of the string received from the socket at
    C++ side. this_step_name and this_step_action will be converted into string,
    r and c into int,
    a0-a9 into double, at C++ side

    Example:
    "action	1	1	DummyScript	0	0	0	0	0	0	0	0	0	0" + "\n" 
    ("\n" can be added in Python side before sending to socket)

'''


def MakeCommandStr(this_step_action, row=0, col=0, idx_non_zero_args=None,
                   non_zero_args=None, this_step_name="action"):
    if idx_non_zero_args is None:
        idx_non_zero_args = []
    if non_zero_args is None:
        non_zero_args = []
    non_zero_args_to_write = non_zero_args

    CommandString = this_step_name + "\t"
    CommandString = CommandString + str(row) + "\t" + str(col) + "\t"
    CommandString = CommandString + this_step_action
    idx = 0
    for i in idx_non_zero_args:
        if i != idx:
            for j in range(i - idx):
                CommandString = CommandString + "\t" + "0"
        CommandString = CommandString + "\t" + str(non_zero_args_to_write[0])
        non_zero_args_to_write.pop(0)
        idx = i + 1
    print(CommandString)  # for testing; to delete

    return CommandString
