/*

	LabJackWB.h
	Anthony Fouad
	Fang-Yen Lab
	December 2018

	Object-oriented interface for accessing the labjack and storing info about
	which stuff is in which pin on the labjack.


	Ideal organization:
		LabJack Object with contents from LabJack Funcs
		LabJackWB inherits from LabJack with easier to understand pinnings

		Note: LabJackUD driver is thread safe and this inherited interface might not be needed.



*/

#pragma once
#ifndef LABJACKWB_H
#define LABJACKWB_H

class LabJackWB {

public:
	// Constructor
	LabJackWB() {}

	// Data
	long lab_jack_handle = 0;
	int vac_dac = 0;
	int heat_dac = 1;
	int actuonix_fio = 0;
	int power_touch_fio = 1;
	int read_touch_ainn = 0;
};

#endif