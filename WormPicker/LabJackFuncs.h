/*
 *		LabJackFuncs.h
 *		Anthony Fouad
 *		Fang-Yen Group, 8/2018
 *
 *		Declares functions for the LabJack
 */

#pragma once
#ifndef LABJACKFUNCS_H_
#define LABJACKFUNCS_H_

#include "c:\program files (x86)\labjack\drivers\LabJackUD.h" 
#include <string>

// Start up the labjack
int LabJack_Bootup(LJ_HANDLE *lngHandle);

// Set a DAC pin to a certain value
std::string LabJack_SetDacN(LJ_HANDLE lngHandle, int DacNum, float voltage);

// Set a FIO pin to true (1) or false (0) - automatically configures as DO if needed
std::string LabJack_SetFioN(LJ_HANDLE lngHandle, int N, int value);

// Read the value of a FIO pin - automatically configures as DI if needed
std::string LabJack_ReadFioN(LJ_HANDLE lngHandle, int N, long &state);

// Read the value of an AIN pin
double LabJack_ReadAINN(LJ_HANDLE lngHandle, int AIN_num);
double LabJack_ReadThermistorC(LJ_HANDLE lngHandle, int AIN_num); // Wrapper specifically for temperature

// Prints any labjack errors to console and returns a string-format
std::string LabJack_Print_Error(LJ_ERROR error_code);

// Set requested DAC pin to V1 for requested seconds and milliseconds using LOW LEVEL hardware timing delays.
// After the total delay time, DAC pin is set to V2.
// NOTE: m_pGoOne() DOES NOT RETURN UNTIL THE ENTIRE DELAY TIME HAS ELAPSED. RECOMMEND PUTTING THIS FUNCTION 
// IN ITS OWN THREAD.
void LabJack_Pulse_DAC(LJ_HANDLE lngHandle, int DacNum, int N_secs, int N_msecs, std::string& lj_error_string, double V1 = 5, double V2 = 0);
void LabJack_PWM_DAC(LJ_HANDLE lngHandle, int DacNum, double run_secs, double f, double duty, std::string& lj_error_string);


// Test functions
void LabJack_testAIN0();


// NOTE: To read or write from pin FIOx use m_peDI or m_peDO, respectively (see link in cpp file)

#endif
