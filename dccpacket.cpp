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
	p.loadBaselineIdlePacket();
	return p;
}

DCCPacket DCCPacket::makeBaselineSpeedDirPacket(int pinA, int pinB, unsigned address, unsigned direction, unsigned  speed, bool headlight)
{
	DCCPacket p(pinA, pinB);
	p.loadBaselineSpeedDirPacket(address, direction, speed, headlight);
	return p;
}

DCCPacket DCCPacket::makeBaselineResetPacket(int pinA, int pinB)
{
	DCCPacket p(pinA, pinB);
	p.loadBaselineResetPacket();
	return p;
}

DCCPacket DCCPacket::makeBaselineBroadcastStopPacket(int pinA, int pinB, BASE_STOP stopcommand)
{
	DCCPacket p(pinA, pinB);
	p.loadBaselineBroadcastStopPacket(stopcommand);
	return p;
}


//Service Mode packet makers:

DCCPacket DCCPacket::makeServiceModeDirectPacket(int pinA, int pinB, int CV, char value)
{
	DCCPacket p(pinA, pinB);
	p.loadServiceModeDirectPacket(CV, value);
	return p;
}
	


void DCCPacket::loadBaselineIdlePacket()
{
	//preamble:
	for (unsigned i=1; i<=10; i++) addOne();
	
	//packet start bit:
	addZero();
	
	//idle address:
	for (unsigned i=1; i<=8; i++) addOne();
	
	//address end bit:
	addZero();
	 
	//no data:
	for (unsigned i=1; i<=8; i++) addZero();

	//data end bit:
	addZero();
        
	//idle packet checksum:
	for (unsigned i=1; i<=8; i++) addOne();

	//packet end bit:
	addOne();
 	
}

void DCCPacket::loadBaselineSpeedDirPacket(unsigned address, unsigned direction, unsigned  speed, bool headlight)
{
	// S9.2, Table 2 (unused until 28-step mode is implemented):
	//const char steps[][5] = {{0,0,0,0,0},{0,0,0,1,0},{1,0,0,1,0},{0,0,0,1,1},{1,0,0,1,1},{0,0,1,0,0},{1,0,1,0,0},{0,0,1,0,1},{1,0,1,0,1},{0,0,1,1,0},{1,0,1,1,0},{0,0,1,1,1},{0,1,0,0,0},{1,1,0,0,0},{0,1,0,0,1},{1,1,0,0,1},{0,1,0,1,0},{1,1,0,1,0},{0,1,0,1,1},{1,1,0,1,1},{0,1,1,0,0},{1,1,1,0,0},{0,1,1,0,1},{1,1,1,0,1},{0,1,1,1,0},{1,1,1,1,0},{0,1,1,1,1},{1,1,1,1,1}};
	char ck = 0;

	if (speed > 27) speed = 27;
	if (direction > 1) direction = 1;
	
	//preamble:
	for (unsigned i=1; i<=10; i++) addOne();
	
	//packet start bit:
	addZero();
	
	//address (7 bits):
	bt = 0; //start check byte collection
	addZero();
	if ((address & 0b01000000) >> 6) addOne(); else addZero();
	if ((address & 0b00100000) >> 5) addOne(); else addZero();
	if ((address & 0b00010000) >> 4) addOne(); else addZero();
	if ((address & 0b00001000) >> 3) addOne(); else addZero();
	if ((address & 0b00000100) >> 2) addOne(); else addZero();
	if ((address & 0b00000010) >> 1) addOne(); else addZero();
	if ((address & 0b00000001)) addOne(); else addZero();
	ck = bt ^ ck;  //collect the checksum
	 
	//address and headlight end bit:
	addZero();
	 
	//data:
	bt = 0;
	addZero();
	addOne();
	if (direction) addOne(); else addZero();

	//the following 'hard-codes' 14-step speed mode, with headlight:
	if (headlight) addOne(); else addZero(); //only for 14-step mode
	if (speed > 0) speed++; //gets around stop value, 000
	if ((speed & 0b00001000) >> 3) addOne(); else addZero();
	if ((speed & 0b00000100) >> 2) addOne(); else addZero();
	if ((speed & 0b00000010) >> 1) addOne(); else addZero();
	if ((speed & 0b00000001)) addOne(); else addZero();
	
	/* defer until 28-step mode is implemented
	for (int i=0; i<5; i++) {
		if (steps[speed][i] == 1) {
			addOne(); 
		}
		else { 
			addZero();
		}
	}
	*/
	
	ck = bt ^ ck;  //collect the checksum
	
	//data end bit:
	addZero();
        
	//packet checksum:
	if ((ck & 0b10000000) >> 7) addOne(); else addZero();
	if ((ck & 0b01000000) >> 6) addOne(); else addZero();
	if ((ck & 0b00100000) >> 5) addOne(); else addZero();
	if ((ck & 0b00010000) >> 4) addOne(); else addZero();
	if ((ck & 0b00001000) >> 3) addOne(); else addZero();
	if ((ck & 0b00000100) >> 2) addOne(); else addZero();
	if ((ck & 0b00000010) >> 1) addOne(); else addZero();
	if ((ck & 0b00000001)) addOne(); else addZero();

	//packet  end bit:
	addOne();
 	
}

void DCCPacket::loadBaselineResetPacket()
{
        //preamble:
        for (unsigned i=1; i<=10; i++) addOne();

        //packet start bit:
        addZero();
        
        //address:
        for (unsigned i=1; i<=8; i++) addZero();
        addZero();
        
        //data:
        for (unsigned i=1; i<=8; i++) addZero();
        addZero();

        //checksum:
        for (unsigned i=1; i<=8; i++) addZero();
        addOne();

}

void DCCPacket::loadBaselineBroadcastStopPacket(unsigned stopcommand)
{
	const char stops[][5] = {{1,0,0,0,0}, {0,0,0,0,1}, {1,0,0,0,1}};
	char ck = 0;

	//preamble:
	for (unsigned i=1; i<=10; i++) addOne();

	//packet start bit:
	addZero();

	//address:
	for (unsigned i=1; i<=8; i++) addZero();
	//no need for checksum collection
	addZero();
        
	//data:
	ck = 0;
	addZero();
	addOne();
	addOne(); //direction, don't care for stop commands
	for (int i=0; i<5; i++)
		if (stops[stopcommand][i] == '1') addOne(); else addZero();
        ck = bt ^ ck;  //collect the checksum	
	addZero();

	//checksum:
	if ((ck & 0b10000000) >> 7) addOne(); else addZero();
	if ((ck & 0b01000000) >> 6) addOne(); else addZero();
	if ((ck & 0b00100000) >> 5) addOne(); else addZero();
	if ((ck & 0b00010000) >> 4) addOne(); else addZero();
	if ((ck & 0b00001000) >> 3) addOne(); else addZero();
	if ((ck & 0b00000100) >> 2) addOne(); else addZero();
	if ((ck & 0b00000010) >> 1) addOne(); else addZero();
	if ((ck & 0b00000001)) addOne(); else addZero();

	addOne();

}

//Long-preamble(>=20 bits) 0 0111CCAA 0 AAAAAAAA 0 DDDDDDDD 0 EEEEEEEE 1
void DCCPacket::loadServiceModeDirectPacket(int CV, char value)
{
	char ck = 0;
	
	//long preamble:
	for (unsigned i=1; i<=20; i++) addOne();

	//byte start bit:
	addZero();
	
	//command, first two bits of CV:
	ck = 0;
	addZero();
	addOne();
	addOne();
	addOne();
	addOne(); //11 - Write byte
	addOne();
	if ((CV & 0b1000000000) >> 9) addOne(); else addZero();
	if ((CV & 0b0100000000) >> 8) addOne(); else addZero();
	ck = bt ^ ck;
	
	//byte start bit:
	addZero();
	
	//last 8 bits of CV:
	ck = 0;
	if ((CV & 0b0010000000) >> 7) addOne(); else addZero();
	if ((CV & 0b0001000000) >> 6) addOne(); else addZero();
	if ((CV & 0b0000100000) >> 5) addOne(); else addZero();
	if ((CV & 0b0000010000) >> 4) addOne(); else addZero();
	if ((CV & 0b0000001000) >> 3) addOne(); else addZero();
	if ((CV & 0b0000000100) >> 2) addOne(); else addZero();
	if ((CV & 0b0000000010) >> 1) addOne(); else addZero();
	if ((CV & 0b0000000001)) addOne(); else addZero();
	ck = bt ^ ck;
	
	//CV value: 
	ck = 0;
	if ((value & 0b10000000) >> 7) addOne(); else addZero();
	if ((value & 0b01000000) >> 6) addOne(); else addZero();
	if ((value & 0b00100000) >> 5) addOne(); else addZero();
	if ((value & 0b00010000) >> 4) addOne(); else addZero();
	if ((value & 0b00001000) >> 3) addOne(); else addZero();
	if ((value & 0b00000100) >> 2) addOne(); else addZero();
	if ((value & 0b00000010) >> 1) addOne(); else addZero();
	if ((value & 0b00000001)) addOne(); else addZero();
	ck = bt ^ ck;  //collect the checksum

	//byte start bit:
	addZero();

	//checksum:
	if ((ck & 0b10000000) >> 7) addOne(); else addZero();
	if ((ck & 0b01000000) >> 6) addOne(); else addZero();
	if ((ck & 0b00100000) >> 5) addOne(); else addZero();
	if ((ck & 0b00010000) >> 4) addOne(); else addZero();
	if ((ck & 0b00001000) >> 3) addOne(); else addZero();
	if ((ck & 0b00000100) >> 2) addOne(); else addZero();
	if ((ck & 0b00000010) >> 1) addOne(); else addZero();
	if ((ck & 0b00000001)) addOne(); else addZero();

	//packet end bit:
	addOne();
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
