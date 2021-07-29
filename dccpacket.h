/*
    This file is part of wavedcc,
    Copyright (C) 2021 Glenn Butcher.
    
    wavedcc is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.
    
    wavedcc is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
    
    You should have received a copy of the GNU General Public License
    along with wavedcc.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef __DCCPACKET_H__
#define __DCCPACKET_H__

#include <pigpio.h>

#include <string>
#include <vector>

//s9.2-B
enum BASE_STOP {
	BASE_STOP_STOPI = 1,
	BASE_STOP_ESTOP,
	BASE_STOP_ESTOPI
};

enum SPEED_STEPS {
	STEP_14,
	STEP_28,
	STEP_128
};
	

class DCCPacket
{
public:
	DCCPacket() { };
	DCCPacket(int pinA, int pinB);
	DCCPacket(const DCCPacket &o); 
	
	//Packet factories
	
	//Baseline packets:
	static DCCPacket makeBaselineIdlePacket(int pinA, int pinB);
	static DCCPacket makeBaselineSpeedDirPacket(int pinA, int pinB, unsigned address, unsigned direction, unsigned  speed, bool headlight);
	static DCCPacket makeBaselineResetPacket(int pinA, int pinB);
	static DCCPacket makeBaselineBroadcastStopPacket(int pinA, int pinB, BASE_STOP stopcommand);
	
	//Extended packets:
	static DCCPacket makeAdvancedSpeedDirPacket(int pinA, int pinB, unsigned address, unsigned direction, unsigned  speed, bool headlight);
	static DCCPacket makeAdvancedFunctionGroupOnePacket(int pinA, int pinB, unsigned address, unsigned value);
	static DCCPacket makeAdvancedFunctionGroupTwoPacket(int pinA, int pinB, unsigned address, unsigned value);
	static DCCPacket makeAdvancedFunctionGroupPacket(int pinA, int pinB, unsigned address, unsigned value);

	
	//Service mode packets:
	static DCCPacket makeServiceModeDirectWriteBytePacket(int pinA, int pinB, int CV, char value);
	static DCCPacket makeServiceModeDirectVerifyBytePacket(int pinA, int pinB, int CV, char value);
	static DCCPacket makeWriteCVToAddressPacket(int pinA, int pinB, int address, int CV, char value);
	

	void addOne();  //adds a DCC one-pulse to the pulsetrain
	void addZero(); //adds a DCC zero-pulse to the pulsetrain
	
	void resetCK();  //call this at the beginning of a packet assembly
	void resetBT();  //call this at the beginning of a byte assembly
	void accumulateCK(); //call this at the end of a byte assembly


	//pulsetrain helpers
	void addPreamble(); //adds a regular preamble
	bool addAddress(int address);  //adds an extended address, or a baseline address if 1 <= address <= 127
	void addCK(); //call this at the end of the packet, before adding the packet end bit
	void addDelimiter(int val); //adds a byte delimiter to the pulse train, val=0|1
		
	std::vector<gpioPulse_t>& getPulseTrain();  //use std::vector's data() method to get reference to pulse train
	std::string getPulseString();
	int getMicros();
	int getOnes();
	int getZeros();

private:
	char bt; //used by addOne and addZero to collect the bits for checksum calculation
	char ck; //checksum accumulator;

	int out1, out2; //GPOI pins
	int us; //time accumulator
	std::vector<gpioPulse_t> pulsetrain; //use as input to gpioWaveAddGeneric() function
	std::string pulsestring; //printable bit string
	int ones, zeros; //one and zero counts 
};

#endif
