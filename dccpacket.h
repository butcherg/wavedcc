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
	static DCCPacket makeBaselineIdlePacket(int pinA, int pinB);
	static DCCPacket makeBaselineSpeedDirPacket(int pinA, int pinB, unsigned address, unsigned direction, unsigned  speed, bool headlight, SPEED_STEPS steps);
	static DCCPacket makeBaselineResetPacket(int pinA, int pinB);
	static DCCPacket makeBaselineBroadcastStopPacket(int pinA, int pinB, BASE_STOP stopcommand);
	static DCCPacket makeServiceModeDirectPacket(int pinA, int pinB, int CV, char value);
	static DCCPacket makeWriteCVToAddressPacket(int pinA, int pinB, int address, int CV, char value);
	
	//S9.2.3 Service Mode Packets:
	void loadServiceModeDirectPacket(int CV, char value);

	void addOne();  //adds a DCC one-pulse to the pulsetrain
	void addZero(); //adds a DCC zero-pulse to the pulsetrain
	
	void resetCK();  //call this at the beginning of a packet assembly
	void resetBT();  //call this at the beginning of a byte assembly
	void accumulateCK(); //call this at the end of a byte assembly


	//pulsetrain helpers
	void addPreamble(); //adds a regular preamble
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
