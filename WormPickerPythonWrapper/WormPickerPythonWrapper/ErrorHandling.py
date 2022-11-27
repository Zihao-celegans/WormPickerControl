'''
    ErrorHandling.py

    Handling errors while running the python script for WormPicker execution

    Zihao Li
    Fang-Yen Lab
    Jan 2022
'''

import enum

class ErrorType (enum.Enum):
    SCRIPT_ACTION_SUCCESS = 1
    INCORRECT_INPUT_ARGUMENT_TYPE = -1
    FAILURE_FEEDBACK_FROM_WORMPICKER = -2


class ErrorStruct():
    def __init__(self, _code, _msg, _handle):

        self.code = _code
        self.message = _msg
        self.handle = _handle


class ErrorStrcutMaker():
    def __init__(self):
        # Successful completion (000)
        self.success = ErrorStruct(000, "", ErrorType.SCRIPT_ACTION_SUCCESS)

        # Invalid script (100)
        self.incorrect_input_argument_type = ErrorStruct(100, "Incorrect input argument type!", ErrorType.INCORRECT_INPUT_ARGUMENT_TYPE)

        # Failure feedback sent by the WormPicker (200)
        self.Failure_sent_by_WormPicker = ErrorStruct(200, "Execution failure feedback received from WormPicker!", ErrorType.FAILURE_FEEDBACK_FROM_WORMPICKER)



###### Utilities for notifying error to screen or/and log files ######

def Notify(func, error_struct):

    if error_struct.handle is ErrorType.SCRIPT_ACTION_SUCCESS:
        print("Successful execution:    " + func.__name__)

    else:
        print("ERROR in " + func.__name__ +":   (error code "+ str(error_struct.code) + ") " + error_struct.message)


###### Utilities for checking error ######

def Check_Input_Argument_Type(Input_args_received, Expected_types):

    type_is_correct = False

    for arg, ty in zip(Input_args_received, Expected_types):
        type_is_correct = (type(arg).__name__ == ty)
        if type_is_correct is False: break

    return type_is_correct



###### Utilities for handling error ######
