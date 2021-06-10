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
	

class DCCPacket
{
public:
	DCCPacket() { };
	DCCPacket(int pinA, int pinB);
	DCCPacket(const DCCPacket &o); 
	
	//Packet factories
	static DCCPacket makeBaselineIdlePacket(int pinA, int pinB);
	static DCCPacket makeBaselineSpeedDirPacket(int pinA, int pinB, unsigned address, unsigned direction, unsigned  speed, bool headlight);
	static DCCPacket makeBaselineResetPacket(int pinA, int pinB);
	static DCCPacket makeBaselineBroadcastStopPacket(int pinA, int pinB, BASE_STOP stopcommand);
	static DCCPacket makeServiceModeDirectPacket(int pinA, int pinB, int CV, char value);
	

	//S9.2 Baseline Packets:
	void loadBaselineIdlePacket();
	void loadBaselineSpeedDirPacket(unsigned address, unsigned direction, unsigned  speed, bool headlight);
	void loadBaselineResetPacket();
	void loadBaselineBroadcastStopPacket(unsigned stopcommand);
	
	//S9.2.1 Extended Packets:
	//(to-do...)
	
	//S9.2.3 Service Mode Packets:
	void loadServiceModeDirectPacket(int CV, char value);

	void addOne();  //adds a DCC one-pulse to the pulsetrain
	void addZero(); //adds a DCC zero-pulse to the pulsetrain
		
	std::vector<gpioPulse_t>& getPulseTrain();  //use std::vector data() method to get reference to pulse train
	std::string getPulseString();
	int getMicros();
	int getOnes();
	int getZeros();

private:
	char bt; //used by addOne and addZero to collect the bits for checksum calculation

	int out1, out2; //GPOI pins
	int us; //time accumulator
	std::vector<gpioPulse_t> pulsetrain; //use as input to gpioWaveAddGeneric() function
	std::string pulsestring; //printable bit string
	int ones, zeros; //one and zero counts 
};

#endif
