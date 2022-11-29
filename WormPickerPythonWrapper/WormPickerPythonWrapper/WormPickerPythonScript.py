'''
    WormPickerPythonScript.py
    
    Python script for WormPicker execution

    Zihao Li & Siming He
    Fang-Yen Lab
    Dec 2021
'''

import WormPicker
import PlateIndex as pidx
import Phenotype as phty



# test push change


def main():

    # Launch WormPicker to run in background and setup connection
    WP = WormPicker.WormPicker()
    WP.LaunchAndConnect()

    # Non-interactive script input
    if (WP.script_input_mode == WormPicker.ScriptInputMode.NON_INTERACTIVE):

        ###### SCRIPT ACTIONS START HERE ######

        WP.SingleWorms(idx_src = pidx.PlateIndex(row = 1, col = 1),
                      idx_inter = pidx.PlateIndex(row = 1, col = 0),
                      num_worm = 5,
                      rows_dest = [0]
                      )

        ###### SCRIPT ACTIONS END HERE ######


    # Interactive script input
    elif (WP.script_input_mode == WormPicker.ScriptInputMode.INTERACTIVE):

        while (True):
            try:
                # get user input from command prompt
                command = input("Enter your command:    ")
                exec(command)
            except:
                print("ERROR(s) occur...")
                # Exit the loop if the script has been deactivated
                if WP.script_is_active is False:
                    print("Script has been deactivated...")
                    break


    # Exit the python script
    WP.Exit()

if __name__ == "__main__":
    main()




