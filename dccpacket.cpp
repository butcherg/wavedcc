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


#include "dccpacket.h"

DCCPacket::DCCPacket(int pinA, int pinB)
{
	out1 = pinA;
	out2 = pinB;
	us = ones = zeros =0;
}

//copy constructor:
DCCPacket::DCCPacket(const DCCPacket &o)
{
	bt = o.bt; 
	out1 = o.out1;
	out2 = o.out2; 
	us = o.us; 
	pulsetrain = o.pulsetrain; 
	pulsestring = o.pulsestring; 
	ones = o.ones;
	zeros = o.zeros;
}


//Baseline packet makers:

DCCPacket DCCPacket::makeBaselineIdlePacket(int pinA, int pinB)
{
	DCCPacket p(pinA, pinB);
	
	//preamble:
	p.addPreamble();
	
	//packet start bit:
	p.addDelimiter(0);
	
	//idle address:
	for (unsigned i=1; i<=8; i++) p.addOne();
	
	//address end bit:
	p.addDelimiter(0);
	 
	//no data:
	for (unsigned i=1; i<=8; i++) p.addZero();

	//data end bit:
	p.addDelimiter(0);
        
	//idle packet checksum:
	for (unsigned i=1; i<=8; i++) p.addOne();

	//packet end bit:
	p.addDelimiter(1);

//	printf("%s\n", p.getPulseString().c_str());
	return p;
}


DCCPacket DCCPacket::makeBaselineSpeedDirPacket(int pinA, int pinB, unsigned address, unsigned direction, unsigned  speed, bool headlight)
{
	DCCPacket p(pinA, pinB);

	if (speed > 28) speed = 28;
	if (speed < 0)  speed =  0;
	if (direction > 1) direction = 1;
	
	//reset checksum accumulator
	p.resetCK();
	
	//preamble:
	p.addPreamble();

	//address start bit:
	p.addDelimiter(0);
	p.addAddress(address); //If address is >127, the address is a two-byte encoding per S9.2.1, so this is a bit more than a baseline packet

	//data start bit:
	p.addDelimiter(0);

	//data:
	p.resetBT();
	p.addZero();
	p.addOne();
	if (direction) p.addOne(); else p.addZero();
	if (speed > 0) speed+=3; //gets around stop values, 10000 - 10001
	if ((speed & 0b00000001)) p.addOne(); else p.addZero();
	if ((speed & 0b00010000) >> 4) p.addOne(); else p.addZero();
	if ((speed & 0b00001000) >> 3) p.addOne(); else p.addZero();
	if ((speed & 0b00000100) >> 2) p.addOne(); else p.addZero();
	if ((speed & 0b00000010) >> 1) p.addOne(); else p.addZero();
	p.accumulateCK();

	//checksum start bit:
	p.addDelimiter(0);

	//checksum:
	p.addCK(); 

	//packet end bit:
	p.addDelimiter(1);
	
//	printf("%d: %s\n", speed, p.getPulseString().c_str());
	return p;
}


DCCPacket DCCPacket::makeBaselineResetPacket(int pinA, int pinB)
{
	DCCPacket p(pinA, pinB);
	
	//preamble:
	p.addPreamble();

	//packet start bit:
	p.addDelimiter(0);
        
	//address:
	for (unsigned i=1; i<=8; i++) p.addZero();
	p.addDelimiter(0);
        
	//data:
	for (unsigned i=1; i<=8; i++) p.addZero();
	p.addDelimiter(0);

	//checksum:
	for (unsigned i=1; i<=8; i++) p.addZero();
	p.addDelimiter(1);

//	printf("%s\n", p.getPulseString().c_str());
	return p;
}

DCCPacket DCCPacket::makeBaselineBroadcastStopPacket(int pinA, int pinB, BASE_STOP stopcommand)
{
	DCCPacket p(pinA, pinB);
	
	const char stops[][5] = {{1,0,0,0,0}, {0,0,0,0,1}, {1,0,0,0,1}};
	p.resetCK();

	//preamble:
	p.addPreamble();

	//packet start bit:
	p.addDelimiter(0);

	//address:
	for (unsigned i=1; i<=8; i++) p.addZero();
	//no need for checksum collection
	p.addDelimiter(0);
        
	//data:
	p.resetBT();
	p.addZero();
	p.addOne();
	p.addOne(); //direction, don't care for stop commands
	for (int i=0; i<5; i++)
		if (stops[stopcommand][i] == '1') p.addOne(); else p.addZero();
	p.accumulateCK();  //collect the checksum	

	p.addDelimiter(0);        

	//checksum:
	p.addCK(); 

	p.addDelimiter(1);

//	printf("%s\n", p.getPulseString().c_str());
	return p;
}



//Extended packet makers:

DCCPacket DCCPacket::makeAdvancedSpeedDirPacket(int pinA, int pinB, unsigned address, unsigned direction, unsigned  speed, bool headlight)
{
	DCCPacket p(pinA, pinB);

	if (speed > 128) speed = 128;
	if (speed < 0)  speed =  0;
	if (direction > 1) direction = 1;
	
	//reset checksum accumulator
	p.resetCK();
	
	//preamble:
	p.addPreamble();

	//address start bit:
	p.addDelimiter(0);
	p.addAddress(address); //If address is >127, the address is a two-byte encoding per S9.2.1, so this is a bit more than a baseline packet

	//data start bit:
	p.addDelimiter(0);
	
	p.resetBT();
	//001 - advanced operating instruction
	p.addZero();
	p.addZero();
	p.addOne();
	//11111 - 128 speed step instruction
	p.addOne();
	p.addOne();
	p.addOne();
	p.addOne();
	p.addOne();
	p.accumulateCK();
	
	//data start bit:
	p.addDelimiter(0);
	
	p.resetBT();
	if (direction) p.addOne(); else p.addZero();
	if ((speed & 0b01000000) >> 6) p.addOne(); else p.addZero();
	if ((speed & 0b00100000) >> 5) p.addOne(); else p.addZero();
	if ((speed & 0b00010000) >> 4) p.addOne(); else p.addZero();
	if ((speed & 0b00001000) >> 3) p.addOne(); else p.addZero();
	if ((speed & 0b00000100) >> 2) p.addOne(); else p.addZero();
	if ((speed & 0b00000010) >> 1) p.addOne(); else p.addZero();
	if ((speed & 0b00000001)) p.addOne(); else p.addZero();
	p.accumulateCK();
	
	p.addDelimiter(0);        

	//checksum:
	p.addCK(); 

	p.addDelimiter(1);
	
	return p;
}

//this function assumes the value is completely constructed by the caller:
DCCPacket DCCPacket::makeAdvancedFunctionGroupPacket(int pinA, int pinB, unsigned address, unsigned value)
{
	DCCPacket p(pinA, pinB);
	
	p.resetCK();
	
	p.addPreamble();

	p.addDelimiter(0);
	p.addAddress(address); //If address is >127, the address is a two-byte encoding per S9.2.1, so this is a bit more than a baseline packet

	p.addDelimiter(0);
	p.resetBT();
	if ((value & 0b10000000) >> 7) p.addOne(); else p.addZero();
	if ((value & 0b01000000) >> 6) p.addOne(); else p.addZero();
	if ((value & 0b00100000) >> 5) p.addOne(); else p.addZero();
	if ((value & 0b00010000) >> 4) p.addOne(); else p.addZero(); 
	if ((value & 0b00001000) >> 3) p.addOne(); else p.addZero();
	if ((value & 0b00000100) >> 2) p.addOne(); else p.addZero();
	if ((value & 0b00000010) >> 1) p.addOne(); else p.addZero();
	if ((value & 0b00000001)) p.addOne(); else p.addZero();
	p.accumulateCK();
	
	p.addDelimiter(0);
	p.addCK(); 

	p.addDelimiter(1);
	
	//printf("%s\n", p.getPulseString().c_str());
	
	return p;
	
}

DCCPacket DCCPacket::makeAdvancedFunctionGroupOnePacket(int pinA, int pinB, unsigned address, unsigned value)
{
	DCCPacket p(pinA, pinB);
	
	p.resetCK();
	
	p.addPreamble();

	p.addDelimiter(0);
	p.addAddress(address); //If address is >127, the address is a two-byte encoding per S9.2.1, so this is a bit more than a baseline packet

	p.addDelimiter(0);
	p.resetBT();
	//100 - function group 1
	p.addOne();
	p.addZero();
	p.addZero();

	if ((value & 0b00010000) >> 4) p.addOne(); else p.addZero(); //only defined for 14-step mode (CV# 29 bit 1 = 1), then it's FL.  Otherwise, it's undefined...
	if ((value & 0b00001000) >> 3) p.addOne(); else p.addZero();
	if ((value & 0b00000100) >> 2) p.addOne(); else p.addZero();
	if ((value & 0b00000010) >> 1) p.addOne(); else p.addZero();
	if ((value & 0b00000001)) p.addOne(); else p.addZero();
	p.accumulateCK();
	
	p.addDelimiter(0);
	p.addCK(); 

	p.addDelimiter(1);
	
	return p;
}

DCCPacket DCCPacket::makeAdvancedFunctionGroupTwoPacket(int pinA, int pinB, unsigned address, unsigned value)
{
	DCCPacket p(pinA, pinB);
	
	p.resetCK();
	
	p.addPreamble();

	p.addDelimiter(0);
	p.addAddress(address); //If address is >127, the address is a two-byte encoding per S9.2.1, so this is a bit more than a baseline packet

	p.addDelimiter(0);
	p.resetBT();
	//101 - function group 2
	p.addOne();
	p.addZero();
	p.addOne();
	if ((value & 0b00010000) >> 4) p.addOne(); else p.addZero();  //assumes the calling function set this properly for F5-F8 (1) vs F9-F12 (0)
	if ((value & 0b00001000) >> 3) p.addOne(); else p.addZero();
	if ((value & 0b00000100) >> 2) p.addOne(); else p.addZero();
	if ((value & 0b00000010) >> 1) p.addOne(); else p.addZero();
	if ((value & 0b00000001)) p.addOne(); else p.addZero();
	p.accumulateCK();
	
	p.addDelimiter(0);
	p.addCK(); 

	p.addDelimiter(1);
	
	return p;
}

DCCPacket DCCPacket::makeWriteCVToAddressPacket(int pinA, int pinB, int address, int CV, char value)
{
	DCCPacket p(pinA, pinB);
	
	//reset checksum accumulator
	p.resetCK();

	//preamble:
	p.addPreamble();

	////////////////////////// Address: 1-2 bytes //////////////////////////////////
	//packet start bit:
	p.addDelimiter(0);
	p.addAddress(address);
	
	////////////////////////// Instruction byte //////////////////////////////////
	//instruction start bit:
	p.addDelimiter(0);
	//start check byte collection
	p.resetBT();
	//1110 - Long-form CV access
	p.addOne();
	p.addOne();
	p.addOne();
	p.addZero();
	//11 - write CV
	p.addOne();
	p.addOne(); 
	//CV address = CV# - 1:
	CV--;
	//CV high bits 9 and 10
	if ((CV & 0b1000000000) >> 9) p.addOne(); else p.addZero();
	if ((CV & 0b0100000000) >> 8) p.addOne(); else p.addZero();
	p.accumulateCK();
	
	////////////////////////// CV low byte //////////////////////////////////
	//CV# start bit:
	p.addDelimiter(0);
	//start check byte collection
	p.resetBT();
	if ((CV & 0b0010000000) >> 7) p.addOne(); else p.addZero();
	if ((CV & 0b0001000000) >> 6) p.addOne(); else p.addZero();
	if ((CV & 0b0000100000) >> 5) p.addOne(); else p.addZero();
	if ((CV & 0b0000010000) >> 4) p.addOne(); else p.addZero();
	if ((CV & 0b0000001000) >> 3) p.addOne(); else p.addZero();
	if ((CV & 0b0000000100) >> 2) p.addOne(); else p.addZero();
	if ((CV & 0b0000000010) >> 1) p.addOne(); else p.addZero();
	if ((CV & 0b0000000001)) p.addOne(); else p.addZero();
	p.accumulateCK();
	
	////////////////////////// CV value //////////////////////////////////
	//CV value start bit:
	p.addDelimiter(0);
	p.resetBT(); //start check byte collection
	if ((value & 0b10000000) >> 7) p.addOne(); else p.addZero();
	if ((value & 0b01000000) >> 6) p.addOne(); else p.addZero();
	if ((value & 0b00100000) >> 5) p.addOne(); else p.addZero();
	if ((value & 0b00010000) >> 4) p.addOne(); else p.addZero();
	if ((value & 0b00001000) >> 3) p.addOne(); else p.addZero();
	if ((value & 0b00000100) >> 2) p.addOne(); else p.addZero();
	if ((value & 0b00000010) >> 1) p.addOne(); else p.addZero();
	if ((value & 0b00000001)) p.addOne(); else p.addZero();
	p.accumulateCK();
	
	////////////////////////// Checksum //////////////////////////////////
	p.addDelimiter(0);
	p.addCK(); 
	
	//packet end bit:
	p.addDelimiter(1);

//	printf("%s\n", p.getPulseString().c_str());
	return p;
}


//Service Mode packet makers:

DCCPacket DCCPacket::makeServiceModeDirectWriteBytePacket(int pinA, int pinB, int CV, char value)
{
	DCCPacket p(pinA, pinB);
	p.resetCK();
	
	//long preamble:
	for (unsigned i=1; i<=20; i++) p.addOne();

	//command, first two bits of CV:
	p.addDelimiter(0);
	p.resetBT(); 
	//0111 - Service Mode Direct
	p.addZero();
	p.addOne();
	p.addOne();
	p.addOne();
	//11 - Write byte
	p.addOne();
	p.addOne();
	if ((CV & 0b1000000000) >> 9) p.addOne(); else p.addZero();
	if ((CV & 0b0100000000) >> 8) p.addOne(); else p.addZero();
	p.accumulateCK();
	
	//last 8 bits of CV:
	p.addDelimiter(0);
	p.resetBT(); 
	if ((CV & 0b0010000000) >> 7) p.addOne(); else p.addZero();
	if ((CV & 0b0001000000) >> 6) p.addOne(); else p.addZero();
	if ((CV & 0b0000100000) >> 5) p.addOne(); else p.addZero();
	if ((CV & 0b0000010000) >> 4) p.addOne(); else p.addZero();
	if ((CV & 0b0000001000) >> 3) p.addOne(); else p.addZero();
	if ((CV & 0b0000000100) >> 2) p.addOne(); else p.addZero();
	if ((CV & 0b0000000010) >> 1) p.addOne(); else p.addZero();
	if ((CV & 0b0000000001)) p.addOne(); else p.addZero();
	p.accumulateCK();
	
	//CV value:
	p.addDelimiter(0);
	p.resetBT(); 
	if ((value & 0b10000000) >> 7) p.addOne(); else p.addZero();
	if ((value & 0b01000000) >> 6) p.addOne(); else p.addZero();
	if ((value & 0b00100000) >> 5) p.addOne(); else p.addZero();
	if ((value & 0b00010000) >> 4) p.addOne(); else p.addZero();
	if ((value & 0b00001000) >> 3) p.addOne(); else p.addZero();
	if ((value & 0b00000100) >> 2) p.addOne(); else p.addZero();
	if ((value & 0b00000010) >> 1) p.addOne(); else p.addZero();
	if ((value & 0b00000001)) p.addOne(); else p.addZero();
	p.accumulateCK(); 

	p.addDelimiter(0);
	p.addCK(); 

	p.addDelimiter(1);
	return p;
}

DCCPacket DCCPacket::makeServiceModeDirectVerifyBytePacket(int pinA, int pinB, int CV, char value)
{
	DCCPacket p(pinA, pinB);
	p.resetCK();
	
	//long preamble:
	for (unsigned i=1; i<=20; i++) p.addOne();

	//command, first two bits of CV:
	p.addDelimiter(0);
	p.resetBT(); 
	//0111 - Service Mode Direct
	p.addZero();
	p.addOne();
	p.addOne();
	p.addOne();
	//01 - Verify byte
	p.addZero();
	p.addOne();
	if ((CV & 0b1000000000) >> 9) p.addOne(); else p.addZero();
	if ((CV & 0b0100000000) >> 8) p.addOne(); else p.addZero();
	p.accumulateCK();
	
	//last 8 bits of CV:
	p.addDelimiter(0);
	p.resetBT(); 
	if ((CV & 0b0010000000) >> 7) p.addOne(); else p.addZero();
	if ((CV & 0b0001000000) >> 6) p.addOne(); else p.addZero();
	if ((CV & 0b0000100000) >> 5) p.addOne(); else p.addZero();
	if ((CV & 0b0000010000) >> 4) p.addOne(); else p.addZero();
	if ((CV & 0b0000001000) >> 3) p.addOne(); else p.addZero();
	if ((CV & 0b0000000100) >> 2) p.addOne(); else p.addZero();
	if ((CV & 0b0000000010) >> 1) p.addOne(); else p.addZero();
	if ((CV & 0b0000000001)) p.addOne(); else p.addZero();
	p.accumulateCK();
	
	//CV value:
	p.addDelimiter(0);
	p.resetBT(); 
	if ((value & 0b10000000) >> 7) p.addOne(); else p.addZero();
	if ((value & 0b01000000) >> 6) p.addOne(); else p.addZero();
	if ((value & 0b00100000) >> 5) p.addOne(); else p.addZero();
	if ((value & 0b00010000) >> 4) p.addOne(); else p.addZero();
	if ((value & 0b00001000) >> 3) p.addOne(); else p.addZero();
	if ((value & 0b00000100) >> 2) p.addOne(); else p.addZero();
	if ((value & 0b00000010) >> 1) p.addOne(); else p.addZero();
	if ((value & 0b00000001)) p.addOne(); else p.addZero();
	p.accumulateCK(); 

	p.addDelimiter(0);
	p.addCK(); 

	p.addDelimiter(1);
	return p;
}


#define ONE 58
#define ZERO 100


void DCCPacket::addOne()
{
	gpioPulse_t p[2];

	p[0].gpioOn  = (1<<out1);
	p[0].gpioOff = (1<<out2);
	p[0].usDelay = ONE;
	p[1].gpioOn  = (1<<out2);
	p[1].gpioOff = (1<<out1);
	p[1].usDelay = ONE;
	pulsetrain.push_back(p[0]);
	pulsetrain.push_back(p[1]);
	pulsestring.append("1");
	us += 116;
	bt = (bt<<1) | 1;
	ones++;
}


void DCCPacket::addZero()
{
	gpioPulse_t p[2];

	p[0].gpioOn  = (1<<out1);
	p[0].gpioOff = (1<<out2);
	p[0].usDelay = ZERO;
	p[1].gpioOn  = (1<<out2);
	p[1].gpioOff = (1<<out1);
	p[1].usDelay = ZERO;
	pulsetrain.push_back(p[0]);
	pulsetrain.push_back(p[1]);
	pulsestring.append("0");
	us += 200;
	bt = (bt << 1);
	zeros++;
}



//Checksum accumulators and adders;

void DCCPacket::resetCK()
{
	ck = 0;
}

void DCCPacket::resetBT()
{
	bt = 0;
}

void DCCPacket::accumulateCK()
{
	ck = bt ^ ck; 
}


void DCCPacket::addPreamble()
{
	for (unsigned i=1; i<=12; i++) addOne();
}

bool DCCPacket::addAddress(int address)
{
	if ((address >= 1) & (address <= 127)) {
		resetBT(); //start check byte collection
		addZero();
		if ((address & 0b01000000) >> 6) addOne(); else addZero();
		if ((address & 0b00100000) >> 5) addOne(); else addZero();
		if ((address & 0b00010000) >> 4) addOne(); else addZero();
		if ((address & 0b00001000) >> 3) addOne(); else addZero();
		if ((address & 0b00000100) >> 2) addOne(); else addZero();
		if ((address & 0b00000010) >> 1) addOne(); else addZero();
		if ((address & 0b00000001)) addOne(); else addZero();
		accumulateCK();
		return true;
	}
	else if ((address >= 128) & (address <= 10239)) {
		resetBT();
		addOne(); //11: extended address
		addOne();
		if ((address & 0b10000000000000) >> 13) addOne(); else addZero();
		if ((address & 0b01000000000000) >> 12) addOne(); else addZero();
		if ((address & 0b00100000000000) >> 11) addOne(); else addZero();
		if ((address & 0b00010000000000) >> 10) addOne(); else addZero();
		if ((address & 0b00001000000000) >> 9)  addOne(); else addZero();
		if ((address & 0b00000100000000) >> 8)  addOne(); else addZero();
		accumulateCK();
		
		addDelimiter(0);
		
		resetBT();
		if ((address & 0b00000010000000) >> 7) addOne(); else addZero();
		if ((address & 0b00000001000000) >> 6) addOne(); else addZero();
		if ((address & 0b00000000100000) >> 5) addOne(); else addZero();
		if ((address & 0b00000000010000) >> 4) addOne(); else addZero();
		if ((address & 0b00000000001000) >> 3) addOne(); else addZero();
		if ((address & 0b00000000000100) >> 2) addOne(); else addZero();
		if ((address & 0b00000000000010) >> 1) addOne(); else addZero();
		if ((address & 0b00000000000001)) addOne(); else addZero();
		accumulateCK();
		
		return true;
	}
	
	
	
	return false;
	
}

void DCCPacket::addCK()
{
	if ((ck & 0b10000000) >> 7) addOne(); else addZero();
	if ((ck & 0b01000000) >> 6) addOne(); else addZero();
	if ((ck & 0b00100000) >> 5) addOne(); else addZero();
	if ((ck & 0b00010000) >> 4) addOne(); else addZero();
	if ((ck & 0b00001000) >> 3) addOne(); else addZero();
	if ((ck & 0b00000100) >> 2) addOne(); else addZero();
	if ((ck & 0b00000010) >> 1) addOne(); else addZero();
	if ((ck & 0b00000001)) addOne(); else addZero();
}



void DCCPacket::addDelimiter(int val)
{
	if (val == 0) {
		pulsestring.append(" ");
		addZero();
		pulsestring.append(" ");
	}
	else if (val == 1) {
		pulsestring.append(" ");
		addOne();
		pulsestring.append(" ");
	}
}

//Getters:
	
std::vector<gpioPulse_t>& DCCPacket::getPulseTrain()
{
	return pulsetrain;
}
	
std::string DCCPacket::getPulseString()
{
	return pulsestring;
}

int DCCPacket::getMicros()
{
	return us;
}

int DCCPacket::getOnes()
{
	return ones;
}

int DCCPacket::getZeros()
{
	return zeros;
}
