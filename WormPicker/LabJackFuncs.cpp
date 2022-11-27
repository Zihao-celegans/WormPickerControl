
//---------------------------------------------------------------------------
//
//
// REFERENCE FOR LabJackUD FUNCTIONS:
// https://labjack.com/support/datasheets/u3/high-level-driver/function-reference
//
//  Simple
//
//  Basic UD U3 example for Dev-C++.  Demonstrates dynamic linking which
//  should work in many Windows compilers, if linking to labjackud.lib
//  (as shown in the VC6 examples) does not work.  Compared to the
//  normal C examples for VC6, the dynamic linking requires the
//  extra header file LJUD_DynamicLinking.h, and the LoadLabJackUD
//  function below.  The other change that must be made when converting
//  a VC6 example to dynamic linking, is to put "m_p" in front of all
//  the UD function names as seen in the code below.
//
//  So, to convert any of the VC6_LJUD examples (of which there are many)
//  to dynamic linking (for Dev-C, Borland, and others), do the following:
//
//  1.  Add an include for LJUD_DynamicLinking.h.
//
//  2.  Copy and paste the LoadLabJackUD function (below).
//
//  3.  Add "m_p" in front of all UD function names.
//
//
//  support@labjack.com
//  Sep 16, 2013
//---------------------------------------------------------------------------
//
//
//
//
//
//
//---------------------------------------------------------------------------
//
// WARNING: #include "LJUD_DynamicLinking.h" should not be included on any files other than this one. 
//
//---------------------------------------------------------------------------

// LabJack includes
#include <stdio.h>
#include "ThreadFuncs.h"
#include <iostream>
#include <stdlib.h>
#include <windows.h>
#include "C:\program files (x86)\labjack\drivers\LabJackUD.h" 
#include "C:\DevC_LJUD\DevC_LJUD\LJUD_DynamicLinking.h" 
#include "LabJackFuncs.h"

// Other includes
#include <string>

// Namespaces
using namespace std;

//Start of LoadLabJackUD function.
//This is the function used to dynamically load the DLL.
void LoadLabJackUD(void)
{
	//Now try and load the DLL.
	if(hDLLInstance = LoadLibrary("labjackud.dll"))
	{
		//If successfully loaded, get the address of the functions.
		m_pListAll = (tListAll)::GetProcAddress(hDLLInstance,"ListAll");
		m_pOpenLabJack = (tOpenLabJack)::GetProcAddress(hDLLInstance,"OpenLabJack");
		m_pAddRequest = (tAddRequest)::GetProcAddress(hDLLInstance,"AddRequest");
		m_pGo = (tGo)::GetProcAddress(hDLLInstance,"Go");
		m_pGoOne = (tGoOne)::GetProcAddress(hDLLInstance,"GoOne");
		m_peGet = (teGet)::GetProcAddress(hDLLInstance,"eGet");
		m_pePut = (tePut)::GetProcAddress(hDLLInstance,"ePut");
		m_pGetResult = (tGetResult)::GetProcAddress(hDLLInstance,"GetResult");
		m_pGetFirstResult = (tGetFirstResult)::GetProcAddress(hDLLInstance,"GetFirstResult");
		m_pGetNextResult = (tGetNextResult)::GetProcAddress(hDLLInstance,"GetNextResult");
		m_peAIN = (teAIN)::GetProcAddress(hDLLInstance,"eAIN");
		m_peDAC = (teDAC)::GetProcAddress(hDLLInstance,"eDAC");
		m_peDI = (teDI)::GetProcAddress(hDLLInstance,"eDI");
		m_peDO = (teDO)::GetProcAddress(hDLLInstance,"eDO");
		m_peAddGoGet = (teAddGoGet)::GetProcAddress(hDLLInstance,"eAddGoGet");
		m_peTCConfig = (teTCConfig)::GetProcAddress(hDLLInstance,"eTCConfig");
		m_peTCValues = (teTCValues)::GetProcAddress(hDLLInstance,"eTCValues");
		m_pResetLabJack = (tResetLabJack)::GetProcAddress(hDLLInstance,"ResetLabJack");
		m_pDoubleToStringAddress = (tDoubleToStringAddress)::GetProcAddress(hDLLInstance,"DoubleToStringAddress");
		m_pStringToDoubleAddress = (tStringToDoubleAddress)::GetProcAddress(hDLLInstance,"StringToDoubleAddress");
		m_pStringToConstant = (tStringToConstant)::GetProcAddress(hDLLInstance,"StringToConstant");
		m_pErrorToString = (tErrorToString)::GetProcAddress(hDLLInstance,"ErrorToString");
		m_pGetDriverVersion = (tGetDriverVersion)::GetProcAddress(hDLLInstance,"GetDriverVersion");
		m_pTCVoltsToTemp = (tTCVoltsToTemp)::GetProcAddress(hDLLInstance,"TCVoltsToTemp");
	}
	else
	{
		printf("\nFailed to load LabJack DLL\n");
		getchar();
		exit(0);
	}
	// m_pOpenLabJack now holds a pointer to the OpenLabJack function.  The compiler
	// automatically recognizes m_pOpenLabJack as a pointer to a function and
	// calls the function with the parameters given.  If we created another
	// variable of type tOpenLabJack and simply put "pNewVar = m_pOpenLabJack",
	// then the compiler might not know to call the function.
}
//End of LoadLabJackUD function.



//This is our simple error handling function that is called after every UD
//function call.  This function displays the errorcode and string description
//of the error.  It also has a line number input that can be used with the
//macro __LINE__ to display the line number in source code that called the
//error handler.  It also has an iteration input is useful when processing
//results in a loop (getfirst/getnext).
//
// Note: ADF edited to stop this from aborting by default
void ErrorHandler(LJ_ERROR lngErrorcode, long lngLineNumber, long lngIteration, bool abort = false)
{
	char err[255];

	if(lngErrorcode != LJE_NOERROR)
	{
		m_pErrorToString(lngErrorcode,err);
		printf("\nLABJACK ERROR:\nError number = %d\n",lngErrorcode);
		printf("Error string = %s\n",err);
		printf("Source line number = %d\n",lngLineNumber);
		printf("Iteration = %d\n\n",lngIteration);
		if(lngErrorcode > LJE_MIN_GROUP_ERROR && abort)
		{
			//Quit if this is a group error.
			getchar();
			exit(0);
		}
	}
}



int LabJack_Bootup(LJ_HANDLE *lngHandle)
{
	LJ_ERROR lngErrorcode;
	double dblDriverVersion;
	long lngIOType = 0, lngChannel = 0;
	double dblValue = 0;
	double Value0 = 9999, Value1 = 9999, Value2 = 9999;
	double ValueDIBit = 9999, ValueDIPort = 9999, ValueCounter = 9999;

	//Load the DLL.
	LoadLabJackUD();

	//Read and display the UD version.
	dblDriverVersion = m_pGetDriverVersion();
	printf("\tBooting LabJack UD...\n");
	printf("\tUD Driver Version = %.3f\n", dblDriverVersion);


	//Open the first found LabJack U3. Abort if failed
	lngErrorcode = m_pOpenLabJack(LJ_dtU3, LJ_ctUSB, "1", 1, lngHandle);
	ErrorHandler(lngErrorcode, __LINE__, 0, true);
	
	//Start by using the pin_configuration_reset IOType so that all
	//pin assignments are in the factory default condition.
	lngErrorcode = m_pePut(*lngHandle, LJ_ioPIN_CONFIGURATION_RESET, 0, 0, 0);
	ErrorHandler(lngErrorcode, __LINE__, 0, true);

	//The following commands will use the add-go-get method to group
	//multiple requests into a single low-level function.

	//Set DAC0 to 0 volts - NO, external code should set appropriate default values on each pin.
	// As it happens though, whenever you connect the labjack it sets everything to 0. 
	//lngErrorcode = m_pAddRequest(*lngHandle, LJ_ioPUT_DAC, 0, 0, 0, 0);
	//ErrorHandler(lngErrorcode, __LINE__, 0, true);

	// Excecute the request
	lngErrorcode = m_pGoOne(*lngHandle);
	ErrorHandler(lngErrorcode, __LINE__, 0, true);

	// Display result and return
	if (lngErrorcode == 0)
		printf("\tSuccessfully booted LabJack UD and set Dac0 to 0.0V\n\n");
	else
		printf("\tFAILED TO BOOT LABJACK!");

	return lngErrorcode;
}

string LabJack_SetDacN(LJ_HANDLE lngHandle, int DacNum, float voltage){

	LJ_ERROR lngErrorcode;
	int max_tries = 3, tries = 1;

	do {
		lngErrorcode = m_pAddRequest(lngHandle, LJ_ioPUT_DAC, DacNum, voltage, 0, 0);
		ErrorHandler(lngErrorcode, __LINE__, 0);
		lngErrorcode = m_pGoOne(lngHandle);
		ErrorHandler(lngErrorcode, __LINE__, 0);

		if (lngErrorcode != 0)
			boost_sleep_ms(100);

	} while (tries++ < max_tries && lngErrorcode != 0);

	return LabJack_Print_Error(lngErrorcode);
}

string LabJack_SetFioN(LJ_HANDLE lngHandle, int N, int value) {
	LJ_ERROR lngErrorcode = m_peDO(lngHandle, N, value);
	return LabJack_Print_Error(lngErrorcode);
}

string LabJack_ReadFioN(LJ_HANDLE lngHandle, int N, long &state) {
	LJ_ERROR lngErrorcode = m_peDI(lngHandle, N, &state);
	return LabJack_Print_Error(lngErrorcode);
}

double LabJack_ReadAINN(LJ_HANDLE lngHandle, int AIN_num) {

	// Setup variables
	long lngIOType = 0, lngChannel = 0;
	double dblValue = 0;

	// Add read request
	long lngErrorcode = m_pAddRequest(lngHandle, LJ_ioGET_AIN, AIN_num, 0, 0, 0);

	// Execute request
	lngErrorcode = m_pGoOne(lngHandle);

	// Get value and return
	lngErrorcode = m_pGetFirstResult(lngHandle, &lngHandle, &lngChannel, &dblValue, 0, 0);
	return dblValue;

}

// Thermistor temperature in degrees celcius form selected AIN port via LJTR
// See C:\Dropbox\Tau Scientific Instruments\Designs\Static Rig\thermistor_calcs.m

double LabJack_ReadThermistorC(LJ_HANDLE lngHandle, int AIN_num) {


	// Coefficients (from row B25/85 = 3977 and a 10K resistor)
	// http://www.vishay.com/docs/29049/ntcle100.pdf
	double A = 3.354016E-03;
	double B = 2.569850E-04;
	double C = 2.620131E-06;
	double D = -6.383091E-08;
	double R25 = 10000;

	// Get current LJTR voltage 
	double V = LabJack_ReadAINN(lngHandle, AIN_num);

	// Convert LJTR voltage to resistance
	// https://labjack.com/support/software/applications/ud-series/ljlogud/ljlogstream-scaling-equations
	double R = ((2.5 - V)*R25 / V);

	// Convert resistance to temperature in K and C
	// (Same reference)
	double TempK = 1 / (A + B * log(R / R25) + C * pow(log(R / R25),2) + D * pow(log(R / R25),3));
	double TempC = TempK - 273.15;


	return TempC;

}

// Prints any labjack errors to console and returns a string-format
string LabJack_Print_Error(LJ_ERROR error_code) {
	
	// Create string to hold error message
	string error_string("");

	// If an error occurred, fill the string with it. Otherwise leave the string blank
	if (error_code != 0) {
		char error_char[300];
		m_pErrorToString(error_code, error_char);
		error_string = string(error_char);
		cout << "LabJack Error detected:\t" << error_string << endl;
	}

	// Return the string for external use
	return error_string;

}

/* 

	Set requested DAC pin to V1 for requested seconds and milliseconds using LOW LEVEL hardware timing delays.
	After the total delay time, DAC pin is set to V2.

	NOTE 1: m_pGoOne() DOES NOT RETURN UNTIL THE ENTIRE DELAY TIME HAS ELAPSED. RECOMMEND PUTTING THIS FUNCTION 
	IN ITS OWN THREAD.

	NOTE 2:  even with the command run in its own thread, the LabJack itself will not accept new commands until
	the existing series has concluded. New commands are queued and run after completion.

*/

void LabJack_Pulse_DAC(LJ_HANDLE lngHandle, int DacNum, int N_secs, int N_msecs, string& lj_error_string, double V1, double V2) {

	// Setup variables
	LJ_ERROR lngErrorcode;
	int max_tries = 3, tries = 1;

	// Prepare requests on DAC try up to 3 times if fails

	do {

		// Request DAC on
		lngErrorcode = m_pAddRequest(lngHandle, LJ_ioPUT_DAC, DacNum, V1, 0, 0);

		// Wait seconds
		for (int i = 0; i < N_secs; i++) {
			lngErrorcode = m_pAddRequest(lngHandle, LJ_ioPUT_WAIT, DacNum, 1000000, 0, 0);
			ErrorHandler(lngErrorcode, __LINE__, 0);
		}

		// Wait additional milliseconds
		lngErrorcode = m_pAddRequest(lngHandle, LJ_ioPUT_WAIT, DacNum, N_msecs * 1000, 0, 0);
		ErrorHandler(lngErrorcode, __LINE__, 0);

		// Request turning off the DAC (V2 volts)
		lngErrorcode = m_pAddRequest(lngHandle, LJ_ioPUT_DAC, DacNum, V2, 0, 0);
		ErrorHandler(lngErrorcode, __LINE__, 0);

		// Wait a bit before trying again
		if (lngErrorcode != 0)
			boost_sleep_ms(100);

	} while (tries++ < max_tries && lngErrorcode != 0);

	// Return error if failed to add requests
	if (lngErrorcode != 0) { lj_error_string = LabJack_Print_Error(lngErrorcode); return; }


	// Send all requests to labjack. No retries are included, but can be done externally
	lngErrorcode = m_pGoOne(lngHandle);
	if (lngErrorcode != 0) { lj_error_string = LabJack_Print_Error(lngErrorcode); return; }
}


/*
	Set DACx to pulse between 5V and 0V at 10Hz with a certain duty fraction. Individual pulses are each hardware timed.
*/

void LabJack_PWM_DAC(LJ_HANDLE lngHandle, int DacNum, double run_secs, double f, double duty, string& lj_error_string) {
	
	// Setup variables
	LJ_ERROR lngErrorcode;
	int max_tries = 3, tries = 1;
	double T = 1000 / f;	/// Seconds (period)
	int N_iter = (int) run_secs * f;
	double N_msecs = duty * T;

	// Send many hardward timed pulse requests to DAC

	for (int i = 0; i < N_iter; i++) {


		// Request DAC on
		lngErrorcode = m_pAddRequest(lngHandle, LJ_ioPUT_DAC, DacNum, 5, 0, 0);

		// Request wait milliseconds
		lngErrorcode = m_pAddRequest(lngHandle, LJ_ioPUT_WAIT, DacNum, N_msecs * 1000, 0, 0);
		ErrorHandler(lngErrorcode, __LINE__, 0);

		//// Request turning off the DAC
		lngErrorcode = m_pAddRequest(lngHandle, LJ_ioPUT_DAC, DacNum, 0, 0, 0);
		ErrorHandler(lngErrorcode, __LINE__, 0);

		// Return error if failed to add requests
		if (lngErrorcode != 0) { lj_error_string = LabJack_Print_Error(lngErrorcode); return; }

		// Send all requests to labjack. No retries are included, but can be done externally
		lngErrorcode = m_pGoOne(lngHandle);
		if (lngErrorcode != 0) { lj_error_string = LabJack_Print_Error(lngErrorcode); return; }

		// Wait to hold port steady
		boost_sleep_ms((1-duty)*T);

		cout << "Sent pulse " << i << " of " << N_iter << endl;
	}

}


void LabJack_testAIN0() {


	// Set up variables

	long lngIOType = 0, lngChannel = 0;
	double dblValue = 0;

	// Boot up labjack
	long labjack_handle = 0;
	LabJack_Bootup(&labjack_handle);

	// Set FIO4-FIO7 to digital out with value 0
	/// eDO automatically configures as DO if needed
	printf("not sure if executing request is required\n");
	for (int i = 4; i<=7; i++)
		m_peDO(labjack_handle, i, 0);

	// Set FIO6 to high to allow power to the sensor
	m_peDO(labjack_handle, 6, 1);

	// Read AIN0 continuously

	for (;;) {

		/// Add read request
		long lngErrorcode = m_pAddRequest(labjack_handle, LJ_ioGET_AIN, 0, 0, 0, 0);

		/// Execute request
		lngErrorcode = m_pGoOne(labjack_handle);

		/// Get value and print
		lngErrorcode = m_pGetFirstResult(labjack_handle, &lngIOType, &lngChannel, &dblValue, 0, 0);
		std::cout << (int)dblValue << std::endl;

		boost_sleep_ms(150);
	}
}


