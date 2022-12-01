'''
    WormPicker.py

    WormPicker script action library

    Zihao Li
    Fang-Yen Lab
    Dec 2021
'''

import socket_module as skt
import subprocess
import time
import CommandStringMaker as cms
import PlateIndex as pidx
import ErrorHandling as errh
import Phenotype as phty
import enum
import sys
import json
import os



class ScriptInputMode(enum.Enum):
    INTERACTIVE = 1         # Interactive mode: take script from user in a infinite loop from the command prompt while the robot exe keep running in the background
    NON_INTERACTIVE = 0     # Non-interactive mode: user coded ALL the commands in the WormPickerPythonScript.py before running the entire script,
                            #                       the robot exe will be terminated by the end of python script, and therefore cannot take any more commands from user


class ScriptExecutionMode (enum.Enum):
    CAREFUL = 1     # Careful mode: exit the whole script if any step in the script failed
    CARELESS = 0    # Careless mode: When any step in the script failed, still continue trying executing next step



class WormPicker(object):

    """description of WormPicker class"""
    '''

    WormPicker class constructs objects having access to script action library.
    Objects in the WormPicker class communicate with robots running in C++ through socket, 
    by which objects send script commands to, and receive execution feedback from C++.

    '''

    def __init__(self, f_config = 'C:\WormPicker\config.json'):

        self.launch_socket = 0 # socket for launching WormPicker exe program running in background, and receive launch feedback
        self.command_socket = 0 # socket for sending script commands to WormPicker, and receive script action status
        self.exe_subprocess_obj = False
        self.mask_rcnn_subprocess_obj = False
        self.errstructs = errh.ErrorStrcutMaker() # construct error handling object

        ## Load parameters from config file
        self.f_config = os.path.normpath(f_config)

        self.params = {} # parameters to be loaded from the whole config file
        self.launch_params = {} # parameters for launching exe program
        self.script_execution_params = {} # parameters for script execution

        self.script_input_mode = -1 # paramters for script input mode, see class ScriptInputMode for detail
        self.script_execution_mode = -1 # paramters for execution mode in Python script, see class ScriptExecutionMode for detail

        self.script_is_active = False # A boolean indicator to show the status of the scipt: whether is still active for execution



    ########################################## Utilities for initiating/Pausing WormPicker ##########################################

    def InitializeResources(self):
        # Initialize resources for launching WormPicker

        # Load parameters from config file
        f = open(self.f_config)
        self.params = json.load(f)['API scripting']
        f.close()

        # parameters for launching exe program
        self.launch_params = self.params['program launch']
        # parameters for script execution
        self.script_execution_params = self.params['script_execution']
        # Script input mode
        self.script_input_mode = ScriptInputMode(self.script_execution_params['interactive_input'])
        # Execution mode for Python script
        self.script_execution_mode = ScriptExecutionMode(self.script_execution_params['careful_mode'])


    def launch(self):
        # launch the robot exe. program running in the background
        print("launching...")

        # Initialize resources
        self.InitializeResources()

        # run the mask-rcnn server
        #self.mask_rcnn_subprocess_obj = subprocess.Popen([os.path.normpath(self.launch_params['mask_rcnn_launcher_directory'])])

        self.mask_rcnn_subprocess_obj = subprocess.run(["python", self.launch_params['mask_rcnn_launcher_directory']])
  
        # run the exe program
        self.exe_subprocess_obj = subprocess.Popen([os.path.normpath(self.launch_params['exe_directory'])])

        # Setup an acceptor socket waiting for feedback of successful launch of exe
        # Setup the server socket to listen
        self.launch_socket = skt.socket_module(ip = self.launch_params['raw_ip_address'], 
                                               port = self.launch_params['port_num'],
                                               socket_type = skt.SocketType.SERVER)

        while(1):
            time.sleep(2)
            print("Waiting for launch feedback from robot...")
            feedback = self.launch_socket.receive_data(show_message = True)
            if (feedback == self.launch_params['launch_success_signal']):
                self.launch_socket.send_data(self.launch_params['close_launch_socket_signal'])

                # Add timeout error before if too long to break
                break

        

    def connect(self):
        # Set up the socket module
        self.command_socket = skt.socket_module(ip = self.script_execution_params['raw_ip_address'], 
                                                port = self.script_execution_params['port_num'], 
                                                socket_type = skt.SocketType.CLIENT)

        # By now the scipt is active for execution
        self.script_is_active = True
        # Return the connection status

    def LaunchAndConnect(self):
        # combine self.launch and self.connect together into a single member function
        self.launch()
        self.connect()
        # TODO: Return boolean to indicate the success of launch and connect

    def stop_and_disconnect(self):
        # Terminate the subprocess (robot exe. program and mask rcnn server)
        if (self.exe_subprocess_obj != False): 
            self.exe_subprocess_obj.terminate()
            self.exe_subprocess_obj = False
        if (self.mask_rcnn_subprocess_obj != False): 
            self.mask_rcnn_subprocess_obj.terminate()
            self.mask_rcnn_subprocess_obj = False

        # By now the script is inactive since the subprocess have been terminated
        self.script_is_active = False

     ########################################## Utilities for communication with WormPicker  ##########################################

    def SendCommand(self, command_string):
        # Send command string to WormPicker
        self.command_socket.send_data(command_string)

    def ReceiveFeedback(self, func):
        # Receive script action feedback from WormPicker
        script_feedback = self.command_socket.receive_data()

        if (script_feedback == (self.script_execution_params['execution_result_signal']['success'] + func.__name__)):
            errh.Notify(func, self.errstructs.success)
            return True
        elif (script_feedback == (self.script_execution_params['execution_result_signal']['fail'] + func.__name__)):
            errh.Notify(func, self.errstructs.Failure_sent_by_WormPicker)
            return False
        else:
            return False


        ########################################## Utilities for Script management  ##########################################

    def controlledExit(self, message = "Script stopped!"):
        # Exit the whole Python script
        self.script_is_active = False
        sys.exit(message)

    def carefulExit(self):
        # If in CAREFUL mode, Exit the whole Python script if any script action failed during the execution
        if (self.script_execution_mode == ScriptExecutionMode.CAREFUL):
            self.Exit(message = "Script exit under CAREFUL execution mode!")

    def ExitCurrentScript(self, func):
        # Waiting for feedback from WormPicker
        # and Exit the current script depending on the script action status
        if self.ReceiveFeedback(func) is False:
            self.carefulExit()
            return False
        else:
            return True

    def Exit(self, message = "Exit script..."):
        # Combine self.stop_and_disconnect() and self.controlledExit() into one single function
        self.stop_and_disconnect()
        self.controlledExit(message)



    ########################################## SCRIPT ACTION LIBRARY STARTS HERE #################################
    '''

    IMPORTANT NOTE:
    Please arrange the library in an alphabetical ascending order of function names, i.e. A-Z!

    '''
    ########################################## Low level script actions ##########################################

    def AutoFocus(self):
        '''
        Perform autofocusing on plate (assuming the gantry is right above the spot the user want to focus onto)

        Name of argument:       Type:                       Note:
        N.A.                    N.A.                        N.A.

        Return                  bool                        True: successful execution; False: error occured or execution failed

        '''

        ## Send the command to WormPicker
        Command_AutoFocus = cms.MakeCommandStr(this_step_action = self.AutoFocus.__name__)
        self.SendCommand(Command_AutoFocus)

        ## Waiting for script action feedback from WormPicker and end the script
        return self.ExitCurrentScript(self.AutoFocus)

    def CalibOx(self, movement = 2, show_img = False):
        '''
        Measure how many pixel shift in low-mag image coresponding to unit movement in OxCNC.
        The pixel shift is assayed by template matching operation performed to images before and after OxCNC movement.
        Note: This script action assumes there has been a clear definite pattern, e.g. a micrometer calibration slide with clean white background, 
        in the center of camera FOV already.

        Name of argument:       Type:                       Note:
        movement                float                       OxCNC movement (in mm) for measuring pixel shift
        show_img                bool                        True: show intermediate result images while computing the pixel shift
                                                            False: do not show any image

        Return                  bool                        True: successful execution; False: error occured or execution failed

        '''
        ## Return if input type is not correct
        if not errh.Check_Input_Argument_Type(
                [movement, show_img],
                ['float', 'bool']):
            errh.Notify(self.CalibOx, self.errstructs.incorrect_input_argument_type)
            self.carefulExit()
            return False
        
        ## Send the command to WormPicker
        Command_CalibOx = cms.MakeCommandStr(
                                             this_step_action = self.CalibOx.__name__,
                                             idx_non_zero_args = range(2),
                                             non_zero_args = [movement, int(show_img)]
                                             )
        self.SendCommand(Command_CalibOx)

        ## Waiting for script action feedback from WormPicker and end the script
        return self.ExitCurrentScript(self.CalibOx)
        
    def CalibPick(self, start_pos1, start_pos2, start_pos3, range = 30, step_sz = 2):
        '''
        Calibrate the pick position in the FOV of machine vision camera
        Mapping the pick tip position with the coordinates of servo motors, while the pick is raster scanning in the FOV

        Name of argument:       Type:                       Note:
        start_pos1              int                         start position of servo 1, cooresponding to the right upper corner
        start_pos2              int                         start position of servo 2, cooresponding to the right upper corner
        start_pos3              int                         start position of servo 3, cooresponding to the right upper corner
                                                            (start_pos1-3 denote the start position of pick at the right upper corner of FOV)
        range                   int                         range of servo movement for calibration
        step_sz                 int                         step size of incremental movement for pick raster scanning

        Return                  bool                        True: successful execution; False: error occured or execution failed
        '''
        ## Return if input type is not correct
        if not errh.Check_Input_Argument_Type(
                [start_pos1, start_pos2, start_pos3, range, step_sz],
                ['int', 'int', 'int', 'int', 'int']):
            errh.Notify(self.CalibPick, self.errstructs.incorrect_input_argument_type)
            self.carefulExit()
            return False
        
        ## Send the command to WormPicker
        Command_CalibPick = cms.MakeCommandStr(
                                             this_step_action = self.CalibPick.__name__,
                                             idx_non_zero_args = range(5),
                                             non_zero_args = [start_pos1, start_pos2, start_pos3, range, step_sz]
                                             )
        self.SendCommand(Command_CalibPick)

        ## Waiting for script action feedback from WormPicker and end the script
        return self.ExitCurrentScript(self.CalibPick)
        
    def CenterWorm(self, idx_plate, pickable_only=True, set_pick_to_start=False):
        '''
        Attempt to find and center a worm with the desired phenotype.
        Move to a random offset within the plate, find all worms, center the one closest to the high mag, and check if its a matching phenotype.
        Repeat the process until we find a match (or give up after multiple tries).

        Name of argument:       Type:                       Note:
        idx_plate               PlateIndex object           (row and col) of plate on which the user will center the worm
        pickable_only           bool                        True: Only center to worms within wormfinder's pickable region; False: otherwise
        set_pick_to_start       bool                        True: Immediately position the pick to the start position once we've found a worm; False: otherwise

        Return                  bool                        True: successful execution; False: error occured or execution failed
        '''

        ## Return if input type is not correct
        if not errh.Check_Input_Argument_Type(
                [idx_plate, pickable_only, set_pick_to_start],
                ['PlateIndex', 'bool', 'bool']):
            errh.Notify(self.CenterWorm,
                        self.errstructs.incorrect_input_argument_type)
            self.carefulExit()
            return False

        ## Send the command to WormPicker
        Command_CenterWorm = cms.MakeCommandStr(
            this_step_action=self.CenterWorm.__name__,
            row=idx_plate.row,
            col=idx_plate.col,
            idx_non_zero_args=range(2),
            non_zero_args=[int(pickable_only), int(set_pick_to_start)]
        )

        self.SendCommand(Command_CenterWorm)

        ## Wait for script action feedback from WormPicker and end the script
        return self.ExitCurrentScript(self.CenterWorm)

    def FindWorm(self):
        '''
        Find all the worms in the FOV (assuming the camera has been placed above the ROI already)
        Return the number of worms found and their positions in the image

        Name of argument:       Type:                       Note:
        N.A.                    N.A.                        N.A.

        Return:                 Type:                       Note:                  
        num_worm                int                         number of worms found in the FOV
        positions               list of cv.point            pixel positions of all the worms found
        result                  bool                        True: successful execution; False: error occured or execution failed

        '''
        ## Send the command to WormPicker
        Command_FindWorm = cms.MakeCommandStr(this_step_action = self.FindWorm.__name__)
        self.SendCommand(Command_FindWorm)

        # Data struct that holds the returns from robot
        

        return True

    def HeatPick(self, heat_time = 2, cool_time = 0.2):
        '''
        Perform sterilization to pick by sending electric current

        Name of argument:       Type:                       Note:
        heat_time               int                         Heating time (in second)
        cool_time               float                       Waiting time for cooling (in second)

        Return                  bool                        True: successful execution; False: error occured or execution failed
        '''

        ## Send the command to WormPicker
         ## Return if input type is not correct
        if not errh.Check_Input_Argument_Type(
                [heat_time, cool_time],
                ['int', 'float']):
            errh.Notify(self.HeatPick,
                        self.errstructs.incorrect_input_argument_type)
            self.carefulExit()
            return False

        ## Send the command to WormPicker
        Command_HeatPick = cms.MakeCommandStr(this_step_action = self.HeatPick.__name__,
                                              idx_non_zero_args = range(2), 
                                              non_zero_args = [heat_time, cool_time]
                                              )
        self.SendCommand(Command_HeatPick)

        ## Waiting for script action feedback from WormPicker and end the script
        return self.ExitCurrentScript(self.HeatPick)

    def MoveToPlate(self, idx_plate, directly = True, steril = False, rand_offset = False, pick_out_FOV = False, manage_lid = False, focus = False):
        '''
        Move the gantry/camera above a plate at specified row and column

        Name of argument:       Type:                       Note:
        idx_plate               PlateIndex object           (row and col) of plate the user want to move to
        directly                bool                        True: Go directly to plate; False: Go to with raise; 
        steril                  bool                        True: Sterilize during motion; False: move without sterilizing the pick
        rand_offset             bool                        True: Add random offset; False: Do not add random offset
        pick_out_FOV            bool                        True: Move the pick out of view of the camera (we typically only care about this if we're moving to the source plate);
                                                            False: Do not move the pick out of view of the camera
        manage_lid              bool                        True: Manage lids when moving to the plate; False: move to plate without managing lids
        focus                   bool                        True: Perform laser autofocus after moving (Typically used here if moving to destination plate to drop a worm);
                                                            False: Do not perform laser autofocus

        Return                  bool                        True: successful execution; False: error occured or execution failed
        '''


        ## Return if input type is not correct
        if errh.Check_Input_Argument_Type([idx_plate, directly, steril, rand_offset, pick_out_FOV, manage_lid, focus], 
                                          ['PlateIndex', 'bool', 'bool', 'bool', 'bool', 'bool', 'bool']
                                          )is False:

            errh.Notify(self.MoveToPlate,self.errstructs.incorrect_input_argument_type)
            self.carefulExit()
            return False


        ## Send the command to WormPicker
        Command_MoveToPlate = cms.MakeCommandStr(
                                                 this_step_action = self.MoveToPlate.__name__,
                                                 row = idx_plate.row, 
                                                 col = idx_plate.col, 
                                                 idx_non_zero_args = range(6), 
                                                 non_zero_args = [int(directly), int(steril), int(rand_offset), int(pick_out_FOV), int(manage_lid), int(focus)]
                                                 )

        self.SendCommand(Command_MoveToPlate)

        ## Waiting for script action feedback from WormPicker and end the script
        return self.ExitCurrentScript(self.MoveToPlate)

    def Phenotype(self, sex_type=0, gfp_type=0, rfp_type=0, morph_type=0, desired_stage=0, num_gfp_spots=1, num_rfp_spots=1):
        '''
        Attempt to phenotype any worms in the high mag image and check them against the desired phenotype.
        Note that in C++, this function takes a num_worms parameter can be used to know how many worms are in the high mag image and determine whether or not you would
        like to perform an intermediate pick.
        Currently, the num_worms is hard-coded to be always 0 in the C++ end.

        Name of argument:       Type:                       Note:
        sex_type                int                         Type of sex wanted
                                                            - 0: No sex phenotyping i.e. whatever sex is fine
                                                            - 1: Herm. is wanted
                                                            - 2: Male is wanted

        gfp_type                int                         Type of GFP
                                                            - 0: No GFP phenotyping
                                                            - 1: GFP worm is wanted
                                                            - 2: non-GFP worm is wanted

        rfp_type                int                        Type of RFP
                                                            - 0: No RFP phenotyping
                                                            - 1: RFP worm is wanted
                                                            - 2: non-RFP worm is wanted

        morph_type              int                        Type of morphology
                                                            - 0: No morphology phenotyping
                                                            - 1: "Normal" worm wanted
                                                            - 2: Dumpy worm wanted
                                                            - 3, 4, etc...: open to other morphology phenotype

        desired_stage           int                        Desired stage
                                                            - 0: No staging required
                                                            - 1: Adult worms
                                                            - 2: L4 worms
                                                            - 3: L3 worms
                                                            - 4: L2 worms
                                                            - 5: L1 worms

        num_gfp_spots           int                         the number of GFP spots expected on a single worm (only needed if searching for gfp worms)

        num_rfp_spots           int                         the number of RFP spots expected on a single worm (only needed if searching for rfp worms)

        Return                  bool                        True: successful execution; False: error occured or execution failed
        '''

        ## Return if input type is not correct
        if not errh.Check_Input_Argument_Type(
                [sex_type, gfp_type, rfp_type, morph_type, desired_stage,
                 num_gfp_spots, num_rfp_spots],
                ['int', 'int', 'int', 'int', 'int', 'int', 'int']):
            errh.Notify(self.Phenotype,
                        self.errstructs.incorrect_input_argument_type)
            self.carefulExit()
            return False

        ## Send the command to WormPicker
        Command_Phenotype = cms.MakeCommandStr(
            this_step_action=self.Phenotype.__name__,
            idx_non_zero_args=[0, 1, 2, 3, 4, 8, 9],
            non_zero_args=[sex_type, gfp_type, rfp_type, morph_type,
                           desired_stage, num_gfp_spots, num_rfp_spots]
        )

        self.SendCommand(Command_Phenotype)

        ## Wait for script action feedback from WormPicker and end the script
        return self.ExitCurrentScript(self.Phenotype)

    def SpeedPick(self, dig_amount=5, sweep_amount=15, pause_time=0.0, restore_height_after=False, center_during_pick=True, picking_mode=0):
        '''
        Perform a single picking action on the agar surface.

        Name of argument:       Type:                       Note:
        dig_amount              int                         Dig amount, dxl units
        sweep_amount            int                         Sweep amount, dxl units
        pause_time              float                       Pause time (seconds)
        restore_height_after    bool                        Restore height after - i.e. don't learn touch height from this picking operation
        center_during_pick      bool                        Do centering during the pick
        picking_mode            int                         The picking mode
                                                             - 0: AUTO = automatically find worm before picking
                                                             - 1: MANUAL = user clicks on worm before picking

        Return                  bool                        True: successful execution; False: error occured or execution failed
        '''

        ## Return if input type is not correct
        if not errh.Check_Input_Argument_Type(
                [dig_amount, sweep_amount, pause_time, restore_height_after,
                 center_during_pick, picking_mode],
                ['int', 'int', 'float', 'bool', 'bool', 'int']):
            errh.Notify(self.SpeedPick,
                        self.errstructs.incorrect_input_argument_type)
            self.carefulExit()
            return False

        ## Send the command to WormPicker
        Command_SpeedPick = cms.MakeCommandStr(
            this_step_action=self.SpeedPick.__name__,
            idx_non_zero_args=range(6),
            non_zero_args=[dig_amount, sweep_amount, pause_time,
                           int(restore_height_after), int(center_during_pick),
                           picking_mode]
        )

        self.SendCommand(Command_SpeedPick)

        ## Wait for script action feedback from WormPicker and end the script
        return self.ExitCurrentScript(self.SpeedPick)

    def SpeedDrop(self, dig_amount=3, sweep_amount=-18, pause_time=2.0, restore_height_after=False, center_during_pick=False, picking_mode=0):
        '''
        Perform a single drop action on the agar surface.

        Name of argument:       Type:                       Note:
        dig_amount              int                         Dig amount, dxl units
        sweep_amount            int                         Sweep amount, dxl units
        pause_time              float                       Pause time (seconds)
        restore_height_after    bool                        Restore height after - i.e. don't learn touch height from this picking operation
        center_during_pick      bool                        Do centering during the pick
        picking_mode            int                         The picking mode
                                                             - 0: AUTO = automatically find a place to drop
                                                             - 1: MANUAL = user clicks on a spot for dropping


        Return                  bool                        True: successful execution; False: error occured or execution failed
        '''

        ## Return if input type is not correct
        if not errh.Check_Input_Argument_Type(
                [dig_amount, sweep_amount, pause_time, restore_height_after,
                 center_during_pick, picking_mode],
                ['int', 'int', 'float', 'bool', 'bool', 'int']):
            errh.Notify(self.SpeedDrop,
                        self.errstructs.incorrect_input_argument_type)
            self.carefulExit()
            return False

        ## Send the command to WormPicker
        Command_SpeedDrop = cms.MakeCommandStr(
            this_step_action=self.SpeedPick.__name__,  # This method shares a script command with SpeedPick, just with the dropping default params
            idx_non_zero_args=range(6),
            non_zero_args=[dig_amount, sweep_amount, pause_time,
                           int(restore_height_after), int(center_during_pick),
                           picking_mode]
        )

        self.SendCommand(Command_SpeedDrop)

        ## Wait for script action feedback from WormPicker and end the script
        return self.ExitCurrentScript(self.SpeedDrop)

    ########################################## Mid level script actions ##########################################

    def PickNWorms(self, idx_src, idx_dest, idx_inter, num_worm, Phenotype, strain, genNum):
        '''
        Transfer multiple (N) worms matching specified phenotypes from one source plate to one destination plate.

        Name of argument:       Type:                       Note:
        idx_src                 PlateIndex object           (row and col) of source plate
        idx_dest                PlateIndex object           (row and col) of destination plate
        idx_inter               PlateIndex object           (row and col) of intermediate picking plate
        num_worm                int                         number of worms want to transfer from source to destination
        Phenotype               Phenotype object            desired phenotype of worms to be transferred
        strain                  string                      the strain of worms to be transferred
        genNum                  int                         the generation number of worms in destination plate

        Return                  bool                        True: successful execution; False: error occured or execution failed
        '''

        ## Return if input type is not correct
        if errh.Check_Input_Argument_Type([idx_src, idx_dest, idx_inter, num_worm, Phenotype, strain, genNum],
                                          ['PlateIndex', 'PlateIndex', 'PlateIndex', 'int', 'Phenotype', 'str', 'int']) is False:
            errh.Notify(self.PickNWorms, self.errstructs.incorrect_input_argument_type)
            self.carefulExit()
            return False

        ## Send the command to WormPicker
        Command_PickNWorms = cms.MakeCommandStr(
                                                this_step_action = self.PickNWorms.__name__,
                                                row = idx_src.row,
                                                col = idx_src.col,
                                                idx_non_zero_args = range(12),
                                                non_zero_args = [idx_dest.row, idx_dest.col,
                                                                 idx_inter.row, idx_inter.col,
                                                                 num_worm,
                                                                 Phenotype.sex.value,
                                                                 Phenotype.gfp.value,
                                                                 Phenotype.rfp.value,
                                                                 Phenotype.morph.value,
                                                                 Phenotype.stage.value,
                                                                 strain,
                                                                 genNum
                                                                 ]
                                                )

        ## Send the command to WormPicker
        self.SendCommand(Command_PickNWorms)
        ## Waiting for script action feedback from WormPicker and end the script
        return self.ExitCurrentScript(self.PickNWorms)

    def ScreenOnePlate(self, idx_plate, num_worm, want_spec_phenotype = False, 
                       phenotype = phty.Phenotype(), thresh = -1, 
                       check_sex = False, check_GFP = False, check_RFP = False, check_morph = False, check_stage = False):
        '''
        Inspect phenotypes of certain amount of worms over a single plate.

        Name of argument:          Type:                       Note:
        idx_plate                  PlateIndex object           (row and col) of the plate to be screened
        num_worm                   int                         number of worms want to screen
        want_spec_phenotype        bool                        is there any specific phenotype want to screen for
                                                                Ture:	    get percentage for the wanted phenotype out of the total number of worms screened
							                                                can quit when this plate containing too low percentage phenotype you wanted, to save time.
					                                            False:		check phenotypes of interested until reach number of worms user requested
							                                                can get distribution of different phenotypes.
        phenotype                  Phenotype object            phenotype wanted, ONLY applicable when want_spec_phenotype is True
        thresh                     float                       percentage threshold above which the screen can quit, ONLY applicable when want_spec_phenotype is True
        check_sex                  bool                        whether check sex, ONLY applicable when want_spec_phenotype is False
        check_GFP                  bool                        whether check GFP, ONLY applicable when want_spec_phenotype is False
        check_RFP                  bool                        whether check RFP, ONLY applicable when want_spec_phenotype is False
        check_morph                bool                        whether check morphology, ONLY applicable when want_spec_phenotype is False
        check_stage                bool                        whether check developmental stage, ONLY applicable when want_spec_phenotype is False

        Return                     bool                        True: successful execution; False: error occured or execution failed
        '''
        ## Return if input type is not correct
        if errh.Check_Input_Argument_Type([idx_plate, num_worm, want_spec_phenotype, phenotype, thresh, check_sex, check_GFP, check_RFP, check_morph, check_stage],
                                        ['PlateIndex', 'int', 'bool', 'Phenotype', 'float', 'bool', 'bool', 'bool', 'bool', 'bool']) is False:
            errh.Notify(self.ScreenOnePlate, self.errstructs.incorrect_input_argument_type)
            self.carefulExit()
            return False

        ## Send the command to WormPicker

        if want_spec_phenotype is True:
            Command_ScreenOnePlate = cms.MakeCommandStr(
                this_step_action = self.ScreenOnePlate.__name__,
                row = idx_plate.row,
                col = idx_plate.col,
                idx_non_zero_args = range(8),
                non_zero_args = [
                    num_worm,
                    int(want_spec_phenotype),
                    phenotype.sex.value,
                    phenotype.GFP.value,
                    phenotype.RFP.value,
                    phenotype.morph.value,
                    phenotype.stage.value,
                    thresh
                    ]
                )
        else:
            Command_ScreenOnePlate = cms.MakeCommandStr(
                this_step_action = self.ScreenOnePlate.__name__,
                row = idx_plate.row,
                col = idx_plate.col,
                idx_non_zero_args = range(7),
                non_zero_args = [
                    num_worm,
                    int(want_spec_phenotype),
                    int(check_sex),
                    int(check_GFP),
                    int(check_RFP),
                    int(check_morph),
                    int(check_stage)
                    ]
                )

        ## Send the command to WormPicker
        self.SendCommand(Command_ScreenOnePlate)
        ## Waiting for script action feedback from WormPicker and end the script
        return self.ExitCurrentScript(self.ScreenOnePlate)


    ########################################## High level script actions ##########################################

    def CrossStrains(self, idx_strain_1, idx_strain_2, idx_dest, idx_inter):
        '''
        Perform genetic cross between two strains.
        Pick worms matching specified phenotypes from 2 source plates into a destination plate.
        

        Name of argument:       Type:                       Note:
        idx_strain_1:           PlateIndex object           (row and col) of first source plate/strain
        idx_strain_2:           PlateIndex object           (row and col) of second source plate/strain
        idx_dest:               PlateIndex object           (row and col) of destination plate
        idx_inter:              PlateIndex object           (row and col) of intermediate picking plate

        Return                  bool                        True: successful execution; False: error occured or execution failed
        
        '''

        ## Return if input type is not correct
        if errh.Check_Input_Argument_Type([idx_strain_1, idx_strain_2, idx_dest, idx_inter], 
                                          ['PlateIndex', 'PlateIndex', 'PlateIndex', 'PlateIndex']
                                          )is False:

            errh.Notify(self.CrossStrains, self.errstructs.incorrect_input_argument_type)
            self.carefulExit()
            return False

        ## Send the command to WormPicker
        Command_CrossStrains = cms.MakeCommandStr(
                                                  this_step_action = self.CrossStrains.__name__,
                                                  row = idx_strain_1.row, 
                                                  col = idx_strain_1.col, 
                                                  idx_non_zero_args = range(6),
                                                  non_zero_args = [idx_strain_2.row, idx_strain_2.col, idx_dest.row, idx_dest.col, idx_inter.row, idx_inter.col]
                                                  )
        ## Send the command to WormPicker
        self.SendCommand(Command_CrossStrains)
        ## Waiting for script action feedback from WormPicker and end the script
        return self.ExitCurrentScript(self.CrossStrains)

    def ScreenPlates(self, num_plate, num_wpp, rows):
        '''
        Inspect phenotypes of a certain amount of worms over a set of plates.
        You can specify up to 8 rows that contain plates to screen. If you specify a row for screening the script will assume EVERY
        location in that row contains a plate to screen.

        Name of argument:       Type:                       Note:
        num_plate               int                         total number of plates that need to be screened
        num_wpp                 int                         number of worms per plate (wpp) to screen
        rows                    list of int                 rows containing plates to screen.
                                                            The script will start screening at the lowest number row, and work in ascending order.

        Return                  bool                        True: successful execution; False: error occured or execution failed
        '''

        ## Return if input type is not correct
        if (
            errh.Check_Input_Argument_Type([num_plate, num_wpp, rows], 
                                          ['int', 'int', 'list']
                                          )
            or 
            errh.Check_Input_Argument_Type(rows,
                                           ['int'] * len(rows)
                                           )
            ) is False:

            errh.Notify(self.ScreenPlates, self.errstructs.incorrect_input_argument_type)
            self.carefulExit()
            return False

        ## Remove duplicated in rows and sort in ascending
        rows = list(set(rows))
        rows.sort()

        ## Send the command to WormPicker
        Command_ScreenPlates = cms.MakeCommandStr(this_step_action = self.ScreenPlates.__name__,
                                                 idx_non_zero_args = range(2 + len(rows)),
                                                 non_zero_args = [num_plate, num_wpp] + rows
                                                 )
        self.SendCommand(Command_ScreenPlates)

        ## Waiting for script action feedback from WormPicker and end the script
        return self.ExitCurrentScript(self.ScreenPlates)

    def SingleWorms(self, idx_src, idx_inter, num_worm, rows_dest):
        '''
        Picks worms matching a specified phenotype from a single source plate onto individual destination plates. A single worm per plate.
        You can specify up to 7 rows into which you will single worms. If you specify a row for singling the script will assume ALL
        plates in that row (other than the source or intermediate plates) are valid plates to pick worms to.

        Name of argument:       Type:                       Note:
        idx_src                 PlateIndex object           (row and col) of source plate
        idx_inter               PlateIndex object           (row and col) of intermediate picking plate
        num_worm                int                         number of worms to single
        rows_dest               list of int                 rows of plates to single worms into. 
                                                            The script will start singling to the lowest number row, and work in ascending order.

        Return                  bool                        True: successful execution; False: error occured or execution failed
        '''

        ## Return if input type is not correct
        if (
            errh.Check_Input_Argument_Type([idx_src, idx_inter, num_worm, rows_dest], 
                                          ['PlateIndex', 'PlateIndex', 'int', 'list']
                                          ) 
            or 
            errh.Check_Input_Argument_Type (rows_dest,
                                            ['int'] * len(rows_dest)
                                            )
            ) is False:

            errh.Notify(self.SingleWorms, self.errstructs.incorrect_input_argument_type)
            self.carefulExit()
            return False

        ## Remove duplicated in rows_dest and sort in ascending
        rows_dest = list(set(rows_dest))
        rows_dest.sort()


        ## Send the command to WormPicker
        Command_SingleWorms = cms.MakeCommandStr(this_step_action = self.SingleWorms.__name__,
                                                 row = idx_src.row,
                                                 col = idx_src.col,
                                                 idx_non_zero_args = range(3 + len(rows_dest)),
                                                 non_zero_args = [idx_inter.row, idx_inter.col, num_worm] + rows_dest
                                                 )
        self.SendCommand(Command_SingleWorms)

        ## Waiting for script action feedback from WormPicker and end the script
        return self.ExitCurrentScript(self.SingleWorms)

    ########################################## SCRIPT ACTION LIBRARY ENDS HERE #################################
    