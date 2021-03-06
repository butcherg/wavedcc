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


#ifdef USE_PIGPIOD_IF
#include <pigpiod_if2.h> 
#else
#include <pigpio.h>
#endif
//#include "pigpio_errors.h"

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include <sys/time.h>

#include <string>
#include <iostream> 
#include <fstream> 
#include <sstream>
#include <vector>
#include <map>
#include <deque>
#include <thread>
#include <mutex>
#include <algorithm>

#include "dccpacket.h"
#include "DatagramSocket.h"
#include "ina219.h"

#define MILLISEC_INTERVAL 500.0 //.01 second interval between voltage/current updates; this is in addition to the apx 1.4ms needed to read voltage,current

bool logging = false;
DatagramSocket *slog;

/*
long timestamp()
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	//return tv.tv_usec / 1000000 + tv.tv_sec;
	return tv.tv_sec * 1000000 + tv.tv_usec;
}
*/

uint64_t timestamp() {
    struct timeval tv;
    gettimeofday(&tv,NULL);
    return tv.tv_sec*(uint64_t)1000000+tv.tv_usec;
}

void loginit()
{
	slog = new DatagramSocket(9035, (char *) "127.0.0.1", true, true);
}

void logclose()
{
	if (slog) slog->~DatagramSocket();
}

void log(std::string msg)
{
	char m[256];
	struct timeval tv;
	gettimeofday(&tv, NULL);
	int n = snprintf ( m, sizeof(m), "%ld_%06ld: %s", tv.tv_sec, tv.tv_usec, msg.c_str() );
	if ((n >0) & (n<256) && (slog) ) slog->send(m, n); 
}

void logcurrent(float current, float voltage)
{
	char m[256];
	struct timeval tv;
	gettimeofday(&tv, NULL);
	int n = snprintf ( m, sizeof(m), "%ld_%06ld: current=%04.2f,voltage=%04.2f", tv.tv_sec, tv.tv_usec, current, voltage );
	if ((n >0) & (n<256) && (slog)) slog->send(m, n); 
}


void pigpio_err(int error)
{
	err_rec r = pigpioError(error);
	std::cout << r.name << " " << r.description << std::endl;
	exit(1);
}

void set_thread_name(std::thread* thread, const char* threadName)
{
   auto handle = thread->native_handle();
   pthread_setname_np(handle,threadName);
}


class CommandQueue
{
public:
	void addCommand(DCCPacket p)
	{
		m.lock();
		cq.push_front(p);
		m.unlock();
	}
	
	DCCPacket getCommand()
	{
		m.lock();
		DCCPacket p = cq.back();
		cq.pop_back();
		m.unlock();
		return p;
	}
	
	bool empty()
	{
		if (cq.size() > 0) return false;
		return true;
	}

private:
	std::deque<DCCPacket> cq;
	std::mutex m; 
};



struct roster_item {
	unsigned address;
	unsigned speed;
	unsigned direction;
	unsigned headlight;
	unsigned fgroup1, fgroup2, fgroup3;
	long tstamp;	// uptime calculation
	int uptime;	// uptime accumulator
};

class Roster
{
public:
	Roster() 
	{
		next = rr.begin();
		//fgroup1 = 128;
		//fgroup2 = 176;
		//fgroup3 = 160;
	}
	
	roster_item get(unsigned address)
	{
		if (rr.find(address) == rr.end()) rr[address] = roster_item{ address, 0, 0, 0, 128, 176, 160, 0, 0}; 
		return rr[address];
	}
	
	void set(unsigned address, roster_item r)
	{
		m.lock();
		rr[address] = r;
		m.unlock();
	}
	
	void setGroup(unsigned address, unsigned group, unsigned val)
	{
		m.lock();
		if (group == 1) rr[address].fgroup1 = val;
		else if (group == 2) rr[address].fgroup2 = val;
		else if (group == 3) rr[address].fgroup3 = val;
		m.unlock();
	}
	
	void update(unsigned address, unsigned speed, unsigned direction, unsigned headlight)
	{
		long tstamp = timestamp();
		m.lock();
		if (rr.find(address) == rr.end()) rr[address] = roster_item{ address, 0, 0, 0, 128, 176, 160, tstamp, 0}; 
		if (rr[address].speed == 0 & speed > 0) { // starting up, just record the timestamp
			rr[address].tstamp = tstamp;
		}
		else if (rr[address].speed > 0 & speed > 0) { // in motion, update uptime and timestamp
			rr[address].uptime += tstamp - rr[address].tstamp;
			rr[address].tstamp = tstamp;
		}
		else if (rr[address].speed > 0 & speed == 0) { // stopping, just update the uptime
			rr[address].uptime += tstamp - rr[address].tstamp;
		}
		rr[address].speed = speed;
		rr[address].direction = direction;
		rr[address].headlight = headlight;
		m.unlock();
	}
	
	roster_item getNext()
	{
		if (rr.size() == 0) return roster_item{ 0, 0, 0, 0, 128, 176, 160}; 
		roster_item i;
		m.lock();
		i = next->second;
		if (++next == rr.end()) next = rr.begin();
		m.unlock();
		return i;
	}
	
	bool forget(unsigned address)
	{
		int result;
		m.lock();
		result = rr.erase(address);
		if (++next == rr.end()) next = rr.begin();
		m.unlock();
		if (result == 1) return true;
		return false;
	}
	
	void forgetall()
	{
		m.lock();
		rr.clear();
		next = rr.begin();
		m.unlock();
	}
	
	std::string list()
	{
		std::stringstream l;
		l << "roster: " << std::endl;
		for (std::map<unsigned, roster_item>::iterator it = rr.begin(); it != rr.end(); ++it)
			l << it->first << ": " << (it->second).speed << " " << (it->second).direction << std::endl;
		if (rr.size() == 0)
			l <<  "No entries." << std::endl;
		return l.str();
	}
	
	std::string uptimes()
	{
		std::stringstream l;
		l << "uptimes (sec): " << std::endl;
		for (std::map<unsigned, roster_item>::iterator it = rr.begin(); it != rr.end(); ++it)
			//l << it->first << ": " << (it->second).uptime << std::endl;
			l << it->first << ":" << ((it->second).uptime / 1000000) << std::endl;
		if (rr.size() == 0)
			l <<  "No entries." << std::endl;
		return l.str();
	}
	
	void writeAndResetUptimes(std::string filename)
	{
		std::ofstream uptimefile;
		uptimefile.open(filename);
		for (std::map<unsigned, roster_item>::iterator it = rr.begin(); it != rr.end(); ++it) {
			uptimefile << it->first << ":" << ((it->second).uptime / 1000000) << std::endl;
			it->second.uptime = 0;
		}
		uptimefile.close();
	}

private:
	std::map<unsigned, roster_item> rr;
	std::mutex m;
//	unsigned next;
	std::map<unsigned, roster_item>::iterator next;
};

//used for command line, configuration file parsing:
std::vector<std::string> split(std::string s, std::string delim)
{
	std::vector<std::string> v;
	if (s.find(delim) == std::string::npos) {
		v.push_back(s);
		return v;
	}
	size_t pos=0;
	size_t start;
	while (pos < s.length()) {
		start = pos;
		pos = s.find(delim,pos);
		if (pos == std::string::npos) {
			v.push_back(s.substr(start,s.length()-start));
			return v;
		}
		v.push_back(s.substr(start, pos-start));
		pos += delim.length();
	}
	return v;
}

inline bool fileExists (const std::string& name) 
{
  struct stat buffer;   
  return (stat (name.c_str(), &buffer) == 0); 
}

std::map<std::string, std::string> getConfig(std::string filename)
{
	std::map<std::string, std::string> config;
	std::ifstream infile(filename.c_str());
	std::string line;
	while (std::getline(infile, line)) {
		if (line[0] == '#') continue; //the whole line is a comment
		std::vector<std::string> c = split(line, "#");
		if (c[0].find("=") == std::string::npos) continue; //there's no name=value pair
		std::vector<std::string> nv = split(c[0], "=");
		if (nv.size() == 2) config[nv[0]] = nv[1];
	}
	infile.close();
	return config;
}


//global declaration of the queue to be used between the command processor and the 
//dccRun thread:
CommandQueue commandqueue;

//global declaration of the roster used to refresh speed/dir packets
Roster roster;

//flag to control runDCC()
bool running = false;

//flag to control runDCCCurrent()
bool currenting = false;

//flag to control programming:
bool programming = false;

//flag to control speed step mode:
bool steps28 = true;

//current and voltage variables, populated by runDCCCurrent:
float voltage;
float current;

//millisecond value to sleep the runDCCCurrent thread:
float millisec;

std::mutex vc;  //use this to guard voltage/current/highwater access
INA219 ina; //The class for interface with the INA219 through I2C

//GPIO ports to use for DCC output, set in the first part of main()
int MAIN1, MAIN2, MAINENABLE;
int PROG1, PROG2, PROGENABLE;

//variables to control CV reading behavior:
int sample_count = 10; //number of samples from the tail of the current measurment vector to use in determining quiescent current
float ack_limit = 60.0; //milliamps over quiescent to determine an ack, per S-9.2.3 60ma. Changeable with 'acklimit' property in wavedcc.conf
int ack_min = 5; // number of current measurements > quiescent + ack_limit to count in determining an ack, per S-9.2.5, 6ms +/- 1ms, so count >=5.  Changeable with 'ackmin' property in wavedcc.conf

//overload threshold in milliamps:
float overload_threshold = 3000.0;
bool overload_trip = false;

//for runDCC() engine and runDCCCurrent() current monitoring threads:
std::thread *t = NULL;
std::thread *c = NULL;

//file path in which to store uptime files:
std::string uptimefilepath = "./";

//boolean to be set by the property 'uptimelogging':
bool uptimelogging = false;

#ifdef USE_PIGPIOD_IF
//pigpiod_id hold the identifier returned at initialization, needed by all pigpiod function calls
int pigpio_id;
#endif

//Thsi routine is to be run as a thread.  It should be started shortly after initialization and
//left to run for the duration of the execution.  It basically just loops forever, sampling the 
//voltage and current every 1ms and posting it to global variables.  There is also a "high-water
//mark" variable, where the maximum current read to-date is posted; this variable can be zeroed
//out for CV reading/verifying.
//
void runDCCCurrent()
{
	char buf[256];
	struct timeval tv1, tv2;
	int dutycycle;
	int overload_count = 0;
	while (currenting) {
		gettimeofday(&tv1, NULL);
		vc.lock();
		voltage = ina.get_voltage();
		current = ina.get_current();
		vc.unlock();
		if (!overload_trip) {
			if (current > overload_threshold) {
				overload_count++;
				if (overload_count >=3) {
#ifdef USE_PIGPIOD_IF
					gpio_write(pigpio_id, MAINENABLE, 0);
					gpio_write(pigpio_id, PROGENABLE, 0);
#else
					gpioWrite(MAINENABLE, 0);
					gpioWrite(PROGENABLE, 0);
#endif
					overload_trip = true;
					programming = false;
					running = false;
					char m[256];
					int n = snprintf(m, 256, "CURRENT OVERLOAD: %04.2f", current);
					if (logging) log(m); 
				}
			}
			else overload_count = 0;
		}
		//if (logging) logcurrent(current, voltage);
		gettimeofday(&tv2, NULL);
		dutycycle = ((tv2.tv_sec - tv1.tv_sec) * 1000000) + (tv2.tv_usec - tv1.tv_usec);
		snprintf ( buf, 256, "current=%04.2f,voltage=%04.2f,duty_cycle=%dus", current, voltage, dutycycle );
		if (logging) log(buf); 
		
		if (millisec > 2) // use the duty cycle to calculate a proper interval
			usleep((int) ((1000 * millisec) - dutycycle));
		else	// just sleep the millisec interval above the duty cycle, 
			//usleep((int) (1000 * millisec));
			usleep((int) (1000 * 2));
	}
} 

//uses the example specified at http://abyz.me.uk/rpi/pigpio/cif.html#gpioWaveCreatePad.
//
//This routine is to be run as a thread.  It basically starts the DCC pulse train
//with an idle packet and retrieves either a command packet from the commandqueue or an idle packet 
//if no command is available, and starts the run loop.  
//
//In the loop, the retrieved packet gets queued up in the wave pulse train, then busy-waits 
//until the current pulse train is done and then deletes it. 
//
//the routine has no termination logic; this has to be provided externally as thread
//control
//
void runDCC()
{	
	struct timespec d;
	d.tv_sec = 0;
	d.tv_nsec = 1000000;  //yields 4-5 iterations in direct, 5-6 iterations in pigpiod

	int wid, nextWid;
	DCCPacket commandPacket(MAIN1, MAIN2);
	
	std::string packet;

	DCCPacket idlePacket = DCCPacket::makeBaselineIdlePacket(MAIN1, MAIN2);
#ifdef USE_PIGPIOD_IF
	wave_add_generic(pigpio_id, idlePacket.getPulseTrain().size(), idlePacket.getPulseTrain().data());
	wid =  wave_create_and_pad(pigpio_id, 50);
	wave_send_using_mode(pigpio_id, wid, PI_WAVE_MODE_ONE_SHOT);
#else
	gpioWaveAddGeneric(idlePacket.getPulseTrain().size(), idlePacket.getPulseTrain().data());
	wid = gpioWaveCreatePad(50, 50, 0);
	gpioWaveTxSend(wid, PI_WAVE_MODE_ONE_SHOT);
#endif

	if (!commandqueue.empty()) {
		commandPacket = commandqueue.getCommand();
	}
	else {
		roster_item i = roster.getNext();
		if (i.address != 0) 
			commandPacket = DCCPacket::makeBaselineSpeedDirPacket(MAIN1, MAIN2, i.address, i.direction, i.speed, i.headlight);
		else
			commandPacket = idlePacket;
	}

	while (running) {
#ifdef USE_PIGPIOD_IF
		wave_add_generic(pigpio_id, commandPacket.getPulseTrain().size(), commandPacket.getPulseTrain().data());
		nextWid =  wave_create_and_pad(pigpio_id, 50);
		wave_send_using_mode(pigpio_id, nextWid, PI_WAVE_MODE_ONE_SHOT_SYNC);

		int i = 0;
		while (wave_tx_at(pigpio_id) == wid) {  nanosleep(&d, &d); i++; }
		//printf("iterations: %d\n", i);
			
		wave_delete(pigpio_id,  wid);
#else
		gpioWaveAddGeneric(commandPacket.getPulseTrain().size(), commandPacket.getPulseTrain().data());
		nextWid = gpioWaveCreatePad(50, 50, 0);
		gpioWaveTxSend(nextWid, PI_WAVE_MODE_ONE_SHOT_SYNC);

		int i = 0;
		while (gpioWaveTxAt() == wid) {  nanosleep(&d, &d); i++; } 
		//printf("iterations: %d\n", i);

		gpioWaveDelete(wid);
#endif
		wid = nextWid;

		// get nextWaveChunk (for this routine, just reuse nextIdlePacket...)
		if (!commandqueue.empty()) {
			commandPacket = commandqueue.getCommand();
		}
		else {
			roster_item i = roster.getNext();
			if (i.address != 0) 
				commandPacket = DCCPacket::makeBaselineSpeedDirPacket(MAIN1, MAIN2, i.address, i.direction, i.speed, i.headlight);
			else
				commandPacket = idlePacket;
		}
	}

#ifdef USE_PIGPIOD_IF
	wave_tx_stop(pigpio_id);
	wave_clear(pigpio_id);
#else
	gpioWaveTxStop();
	gpioWaveClear();
#endif
}

void signal_handler(int signum) {
	std::cout << std::endl << "exiting (signal " << signum << ")..." << std::endl;
#ifdef USE_PIGPIOD_IF
	gpio_write(pigpio_id, MAINENABLE, 0);
	gpio_write(pigpio_id, PROGENABLE, 0);
#else
	gpioWrite(MAINENABLE, 0);
	gpioWrite(PROGENABLE, 0);
#endif
	if (logging) logclose();
	logging=false;
	currenting = false;
	running = false;
	if (c && c->joinable()) {
		c->join();
		c->~thread();
		c = NULL;
	}
	if (t && t->joinable()) {
		t->join();
		t->~thread();
		t = NULL;
	}
	ina.deconfigure();
#ifdef USE_PIGPIOD_IF
	pigpio_stop(pigpio_id);
#else
	gpioTerminate();
#endif
	exit(1);
}


//uptimefilepath
//uptimelogging
std::string dccInit()
{
	MAIN1=17;
	MAIN2=27;
	MAINENABLE=22;
	
	//temporary, for testing:
	PROG1=17;
	PROG2=27;
	PROGENABLE=22;

	//read configuration from wavedcc.conf
	std::map<std::string, std::string> config;
	std::string home(std::getenv("HOME"));
	home += "/.wavedcc/wavedcc.conf";
	if (fileExists("wavedcc.conf")) {
		config = getConfig("wavedcc.conf");
		std::cout << "Configuration from ./wavedcc.conf" << std::endl;
	}
	else if (fileExists(home)) {
		getConfig(home);
		std::cout << "Configuration from " << home << std::endl;
	}
	else std::cout << "No configuration file found." << std::endl;

	//for configuration debug:
	//for(std::map<std::string, std::string>::iterator it = config.begin(); it!= config.end(); ++it)
	//	std::cout << it->first << " = " << it->second << std::endl;
	
	if (config.find("main1") != config.end()) MAIN1 = atoi(config["main1"].c_str());
	if (config.find("main2") != config.end()) MAIN2 = atoi(config["main2"].c_str());
	if (config.find("mainenable") != config.end()) MAINENABLE = atoi(config["mainenable"].c_str());
	if (config.find("prog1") != config.end()) MAIN1 = atoi(config["prog1"].c_str());
	if (config.find("prog2") != config.end()) MAIN2 = atoi(config["prog2"].c_str());
	if (config.find("progenable") != config.end()) MAINENABLE = atoi(config["progenable"].c_str());

	if (config.find("logging") != config.end()) {
		if (config["logging"] == "1") {
		loginit();
		logging = true;
		}
	}
	
	if (config.find("uptimelogging") != config.end())
		if (config["uptimelogging"] == "1")
			uptimelogging = true;
	if (config.find("uptimefilepath") != config.end())
		uptimefilepath = config["uptimefilepath"];

	if (config.find("samplecount") != config.end()) sample_count = atoi(config["samplecount"].c_str());
	if (config.find("acklimit") != config.end()) ack_limit = atof(config["acklimit"].c_str());
	if (config.find("ackmin") != config.end()) ack_min = atoi(config["ackmin"].c_str());
	
	if (config.find("overloadthreshold") != config.end()) overload_threshold = atof(config["overloadthreshold"].c_str());

#ifdef USE_PIGPIOD_IF
	std::string host = "localhost";
	std::string port = "8888";
	if (config.find("host") != config.end()) host = config["host"];
	if (config.find("port") != config.end()) host = config["port"];
	pigpio_id = pigpio_start((char *) host.c_str(), (char *) port.c_str());
	if (pigpio_id < 0) pigpio_err(pigpio_id);
	set_mode(pigpio_id, MAIN1, PI_OUTPUT);
	set_mode(pigpio_id, MAIN2, PI_OUTPUT);
	set_mode(pigpio_id, MAINENABLE, PI_OUTPUT);
	set_mode(pigpio_id, PROG1, PI_OUTPUT);
	set_mode(pigpio_id, PROG2, PI_OUTPUT);
	set_mode(pigpio_id, PROGENABLE, PI_OUTPUT);
	gpio_write(pigpio_id, MAINENABLE, 0);
	gpio_write(pigpio_id, PROGENABLE, 0);
	wave_clear(pigpio_id);
	std::string wavelet_mode = "remote (" + host + ")";
	signal(SIGINT, signal_handler);
	ina.configure(pigpio_id);	
	//ina.configure((const char *) host.c_str(), (const char *) port.c_str());
#else
	int result;
	result = gpioInitialise();
	if (result < 0) pigpio_err(result);
	gpioSetMode(MAIN1, PI_OUTPUT);
	gpioSetMode(MAIN2, PI_OUTPUT);
	gpioSetMode(MAINENABLE, PI_OUTPUT);
	gpioSetMode(PROG1, PI_OUTPUT);
	gpioSetMode(PROG2, PI_OUTPUT);
	gpioSetMode(PROGENABLE, PI_OUTPUT);
	gpioWrite(MAINENABLE, 0);
	gpioWrite(PROGENABLE, 0);
	gpioWaveClear();
	std::string wavelet_mode = "native";
	gpioSetSignalFunc(SIGINT, signal_handler);
	ina.configure();
#endif

	millisec = MILLISEC_INTERVAL; //no need to lock before thread start
	currenting = true;
	c = new std::thread(&runDCCCurrent);
	set_thread_name(c, "current");

	std::stringstream resultstr;
	resultstr << "outgpios: " << MAIN1 << "|" << MAIN2 << std::endl << "mode: " << wavelet_mode << std::endl;
	return resultstr.str();
}

//depends on the prior creation of rwave by the caller...
bool verifyBit(char rwave, float quiescent, unsigned cv, unsigned char bitpos, unsigned char val)
{
	char msg[256];
	std::vector<float> currents;

	DCCPacket p = DCCPacket::makeServiceModeDirectVerifyBitPacket(PROG1, PROG2, cv, bitpos, val);

#ifdef USE_PIGPIOD_IF
	wave_add_generic(pigpio_id, p.getPulseTrain().size(), p.getPulseTrain().data());	
	char pwave = wave_create(pigpio_id);
#else
	gpioWaveAddGeneric(p.getPulseTrain().size(), p.getPulseTrain().data());
	char pwave = gpioWaveCreate();
#endif

	std::vector<char> pchain = {
		//S-9.2.3: 3 resets:
		rwave, rwave, rwave,
		//S-9.2.3: 5 writes:
		pwave, pwave, pwave, pwave, pwave,
		//S-9.2.3: 1 or more resets to cover ack period, if present:
		rwave, rwave, rwave, rwave, rwave, rwave
	};

	float max_current = 0.0;
	int pwrcount = 0;
	snprintf(msg, 256, "Verify CV%d bit %d = %d", cv, bitpos, val);
	if (logging) log(msg);
#ifdef USE_PIGPIOD_IF	
	gpio_write(pigpio_id, PROGENABLE, 1); 
	wave_chain(pigpio_id, pchain.data(), pchain.size());
	while (wave_tx_busy(pigpio_id)) { currents.push_back(current); usleep(1000); }
	gpio_write(pigpio_id, PROGENABLE, 0);
#else
	gpioWrite(PROGENABLE, 1);
	gpioWaveChain(pchain.data(), pchain.size());
	while (gpioWaveTxBusy()) { currents.push_back(current); usleep(1000); }
	gpioWrite(PROGENABLE, 0);
#endif

	float maxack = 0.0;

	//count back from the end of sampling sample_count samples, find current measurements > quiescent + 60ma
	for (int i=currents.size()-sample_count; i<currents.size(); i++) {
		if (currents[i] > quiescent + ack_limit) {
			pwrcount++; //S-9.2.3 60.0ma
			maxack = currents[i];
		}
	}
	
	//count back from the end of sampling sample_count samples, find current measurements > quiescent + 60ma
	//for (int i=0; i<currents.size(); i++) {
	//	if (currents[i] > quiescent + ack_limit) {
	//		pwrcount++; //S-9.2.3 60.0ma
	//		maxack = currents[i];
	//	}
	//}

	if (pwrcount >= ack_min) { //S-9.2.3 6ms +/- 1ms
		snprintf(msg, 256, "CV%d found %d in bit position %d (max=%04.2f, pc=%d)", cv, val, bitpos, maxack, pwrcount);
		if (logging) log(msg);
		val = 0;
		return true;
	}

	snprintf(msg, 256, "CV%d did not find %d in bit position %d (max=%04.2f, pc=%d)", cv, val, bitpos, maxack, pwrcount);
	if (logging) log(msg);
	return false;

}

//depends on the prior creation of rwave by the caller...
bool verifyByte(char rwave, float quiescent, unsigned cv, unsigned char val)
{
	char msg[256];
	std::vector<float> currents;

	DCCPacket p = DCCPacket::makeServiceModeDirectVerifyBytePacket(PROG1, PROG2, cv, val);

#ifdef USE_PIGPIOD_IF
	wave_add_generic(pigpio_id, p.getPulseTrain().size(), p.getPulseTrain().data());	
	char pwave = wave_create(pigpio_id);
#else
	gpioWaveAddGeneric(p.getPulseTrain().size(), p.getPulseTrain().data());
	char pwave = gpioWaveCreate();
#endif

	std::vector<char> pchain = {
		//S-9.2.3: 3 resets:
		rwave, rwave, rwave,
		//S-9.2.3: 5 writes:
		pwave, pwave, pwave, pwave, pwave,
		//S-9.2.3: 1 or more resets to cover ack period, if present:
		rwave, rwave, rwave, rwave, rwave, rwave
	};

	float max_current = 0.0;
	int pwrcount = 0;
	snprintf(msg, 256, "Verify CV%d value %d", cv, val);
	if (logging) log(msg);
#ifdef USE_PIGPIOD_IF	
	gpio_write(pigpio_id, PROGENABLE, 1); 
	wave_chain(pigpio_id, pchain.data(), pchain.size());
	while (wave_tx_busy(pigpio_id)) { currents.push_back(current); usleep(1000); }
	gpio_write(pigpio_id, PROGENABLE, 0);
#else
	gpioWrite(PROGENABLE, 1);
	gpioWaveChain(pchain.data(), pchain.size());
	while (gpioWaveTxBusy()) { currents.push_back(current); usleep(1000); }
	gpioWrite(PROGENABLE, 0);
#endif

	float maxack = 0.0;

	//count back from the end of sampling sample_count samples, find current measurements > quiescent + 60ma
	for (int i=currents.size()-sample_count; i<currents.size(); i++) {
		if (currents[i] > quiescent + ack_limit) {
			pwrcount++; //S-9.2.3 60.0ma
			maxack = currents[i];
		}
	}
	
	
	//count back from the end of sampling sample_count samples, find current measurements > quiescent + 60ma
	//for (int i=0; i<currents.size(); i++) {
	//	if (currents[i] > quiescent + ack_limit) {
	//		pwrcount++; //S-9.2.3 60.0ma
	//		maxack = currents[i];
	//	}
	//}

	if (pwrcount >= ack_min) { //S-9.2.3 6ms +/- 1ms
		snprintf(msg, 256, "CV%d = %d (max=%04.2f, pc=%d)", cv, val, maxack, pwrcount);
		if (logging) log(msg);
		val = 0;
		return true;
	}

	snprintf(msg, 256, "CV%d != %d (max=%04.2f, pc=%d)", cv, val, maxack, pwrcount);
	if (logging) log(msg);
	return false;

}


//global, used to run a single engine with adr/+/-:
//int address=0, speed=0, direction=1;
bool headlight=true;

std::string dccCommand(std::string cmd)
{
	cmd.erase(cmd.find_last_not_of(" \n\r\t")+1);
	cmd.erase(std::remove(cmd.begin(), cmd.end(), '<'), cmd.end());
	cmd.erase(std::remove(cmd.begin(), cmd.end(), '>'), cmd.end());
	std::vector<std::string> cmdstring = split(cmd, " ");

	std::stringstream response;

	//<1{ MAIN|PROG]> - turn on all|main|prog track(s), returns <p1[ MAIN|PROG]>
	if (cmdstring[0] == "1") {
		if (cmdstring.size() >= 2) {
			if (cmdstring[1] == "MAIN") {
				if (programming) {
					response << "<Error: programming mode active.>";
				}
				else {
					if (t == NULL) {
#ifdef USE_PIGPIOD_IF
						wave_clear(pigpio_id);
#else
						gpioWaveClear();
#endif
						running = true;
						vc.lock();
						millisec = 1;
						vc.unlock();
						usleep(1000*MILLISEC_INTERVAL); //insure current monitoring before enabling power
						t = new std::thread(&runDCC);
						set_thread_name(t, "pulsetrain");
						
#ifdef USE_PIGPIOD_IF
						gpio_write(pigpio_id, PROGENABLE, 0);
						gpio_write(pigpio_id, MAINENABLE, 1);
#else
						gpioWrite(PROGENABLE, 0);
						gpioWrite(MAINENABLE, 1);
#endif
						response << "<p1 MAIN>";
					}
					else response << "<Error: DCC pulsetrain already started.";
				}
			}
			else if (cmdstring[1] == "PROG") {
				if (running) {
					response << "<Error: run mode active";
				}
				else {
					programming = true;
					
#ifdef USE_PIGPIOD_IF
					wave_clear(pigpio_id);
					gpio_write(pigpio_id, MAINENABLE, 0);
#else
					gpioWaveClear();
					gpioWrite(MAINENABLE, 0);
#endif
					response << "<p1 PROG>";
					
				}
			}
			else response << "<Error: invalid mode.>";
		}
		else if (cmdstring.size() == 1) {  // <1> just enables MAIN
			if (programming) {
				response << "<Error: programming mode active.>";
			}
			else {
				if (t == NULL) {
#ifdef USE_PIGPIOD_IF
					wave_clear(pigpio_id);
#else
					gpioWaveClear();
#endif
					running = true;
					vc.lock();
					millisec = 1;
					vc.unlock();
					usleep(1000*MILLISEC_INTERVAL);
					t = new std::thread(&runDCC);
					set_thread_name(t, "pulsetrain");
					vc.lock();
					vc.unlock();
#ifdef USE_PIGPIOD_IF
					gpio_write(pigpio_id, PROGENABLE, 0);
					gpio_write(pigpio_id, MAINENABLE, 1);
#else

					gpioWrite(PROGENABLE, 0);
					gpioWrite(MAINENABLE, 1);
#endif
					response << "<p1 MAIN>";
				}
				else response << "<Error: DCC pulsetrain already started.";
			}
		}
		else response << "<Error: wavedcc only supports one mode at a time.>";
		
	}
	
	//<0[ MAIN|PROG]> - turn off all|main|prog track(s), returns <p0[ MAIN|PROG]>
	else if (cmdstring[0] == "0") {
		if (cmdstring.size() >= 2) {
			if (cmdstring[1] == "MAIN") {
				if (programming) {
					response << "<Error: programming mode active.>";
				}
				else {
					running = false;
#ifdef USE_PIGPIOD_IF
					gpio_write(pigpio_id, MAINENABLE, 0);
#else
					gpioWrite(MAINENABLE, 0);
#endif
					
					if (t && t->joinable()) {
						t->join();
						t->~thread();
						t = NULL;
					}
					vc.lock();
					millisec = MILLISEC_INTERVAL;
					vc.unlock();
					response <<  "<p0 MAIN>\n";
					
					if (uptimelogging) {
						char fname[256];
						time_t rawtime;
						struct tm *ftime;
						time( &rawtime );
						ftime = localtime( &rawtime );
						strftime(fname,256,"%Y-%m-%d_%H:%M:%S.txt", ftime);
						roster.writeAndResetUptimes(uptimefilepath+std::string(fname));
					}
					
				}
			}
			else if (cmdstring[1] == "PROG") {
				if (running) {
					response << "<Error: run mode active";
				}
				else {
					programming = false;
#ifdef USE_PIGPIOD_IF
					gpio_write(pigpio_id, PROGENABLE, 0);
#else
					gpioWrite(PROGENABLE, 0);
#endif
					response << "<p0 PROG>\n";
					
				}
			}
			else response << "<Error: invalid mode.>";

		}
		else { // turn off both/either
			if (running) {
				running = false;
#ifdef USE_PIGPIOD_IF
				gpio_write(pigpio_id, MAINENABLE, 0);
#else
				gpioWrite(MAINENABLE, 0);
#endif
				if (t && t->joinable()) {
					t->join();
					t->~thread();
					t = NULL;
				}
				response <<  "<p0>\n";

				if (uptimelogging) {
					char fname[256];
					time_t rawtime;
					struct tm *ftime;
					time( &rawtime );
					ftime = localtime( &rawtime );
					strftime(fname,256,"%Y-%m-%d_%H:%M:%S.txt", ftime);
					roster.writeAndResetUptimes(uptimefilepath+std::string(fname));
				}
				
			}
			else if (programming) {
				programming = false;
#ifdef USE_PIGPIOD_IF
				gpio_write(pigpio_id, PROGENABLE, 0);
#else
				gpioWrite(PROGENABLE, 0);
#endif
				response << "<p0>\n";
			}
			else response << "<p0>\n";
		}
	}

	//<D CABS> - returns the roster list
	//<D SPEED28|SPEED128> - changes the step mode for <t> commands
	else if (cmdstring[0] == "D") {
		if (cmdstring[1] == "CABS") return roster.list();
		else if (cmdstring[1] == "SPEED28") steps28 = true;
		else if (cmdstring[1] == "SPEED128") steps28 = false;
	}
	
	//<-[ (int address)]> - forget address, or forget all addresses, if none is specified. returns NONE
	else if (cmdstring[0] == "-") {
		if (cmdstring.size() >= 2) {
			roster.forget(atoi(cmdstring[1].c_str()));
		}
		else {
			roster.forgetall();
		}
	}

	// throttle command: t addr spd dir
	//<t [1] (int address) (int speed) (0|1 direction)> - throttle comand, returns <T 1 (int speed) (1|0 direction)>
	else if (cmdstring[0] == "t") {
		int address, speed;
		bool direction;
		
		if (running) {
		
			if (cmdstring.size() == 5) {
				address = atoi(cmdstring[2].c_str());
				speed = atoi(cmdstring[3].c_str());
				direction = atoi(cmdstring[4].c_str());
				response << "<T 1 " << speed << " " << direction << ">";
			}
			else if (cmdstring.size() == 4) {
				address = atoi(cmdstring[1].c_str());
				speed = atoi(cmdstring[2].c_str());
				direction = atoi(cmdstring[3].c_str());
				response << "<T 1 " << speed << " " << direction << ">";
			}
			else {
				response << "<Error: malformed command.>";
			}
		
			DCCPacket p;
			
			if (steps28)
				p = DCCPacket::makeBaselineSpeedDirPacket(MAIN1, MAIN2, address, direction, speed, headlight);
			else
				p = DCCPacket::makeAdvancedSpeedDirPacket(MAIN1, MAIN2, address, direction, speed, headlight);
				
			//printf("%s\n", p.getPulseString().c_str());

			commandqueue.addCommand(p);
			roster.update(address, speed, direction, headlight);
		} 
		else response << "<Error: can't run in programming mode.>";
	}
	
	//<f address byte [byte]> sets the functions F1-F12 using the constructed byte
	else if (cmdstring[0] == "f") {
		int address, byte;
		DCCPacket p;
		if (cmdstring.size() == 3) {
			address = atoi(cmdstring[1].c_str());
			byte = atoi(cmdstring[2].c_str());
			p = DCCPacket::makeAdvancedFunctionGroupPacket(MAIN1, MAIN2, address, byte);
			commandqueue.addCommand(p);
		}
		else {
			response << "<Error: malformed command.>";
		}
		
	}

	//<F address function 1|0> command turns engine decoder functions ON and OFF
	else if (cmdstring[0] == "F") {
		int address, func;
		bool state;
		DCCPacket p;
		if (cmdstring.size() == 4) {
			address = atoi(cmdstring[1].c_str());
			func = atoi(cmdstring[2].c_str());
			if (cmdstring[3] == "0")
				state = false;
			else
				state = true;
			
			roster_item r = roster.get(address);
			
			if ((func >=0) & (func <= 4)) {
				if (func == 0) func = 4; else func -= 1;
				if (state) r.fgroup1 |= 1 << func; else r.fgroup1 &= ~(1 << func);
				p = DCCPacket::makeAdvancedFunctionGroupPacket(MAIN1, MAIN2, address, r.fgroup1);
				roster.setGroup(address, 1, r.fgroup1);
				commandqueue.addCommand(p);
			}
			else if ((func >=5) & (func <= 8)) {
				func -= 5;
				if (state) r.fgroup2 |= 1 << func; else r.fgroup2 &= ~(1 << func);
				p = DCCPacket::makeAdvancedFunctionGroupPacket(MAIN1, MAIN2, address, r.fgroup2);
				roster.setGroup(address, 2, r.fgroup2);
				commandqueue.addCommand(p);
			}
			else if ((func >=9) & (func <= 12)) {
				func -= 9;
				if (state) r.fgroup3 |= 1 << func; else r.fgroup3 &= ~(1 << func);
				p = DCCPacket::makeAdvancedFunctionGroupPacket(MAIN1, MAIN2, address, r.fgroup3);
				roster.setGroup(address, 3, r.fgroup3);
				commandqueue.addCommand(p);
			}
		}
		else {
			response << "<Error: malformed command.>";
		}
	}

	
	//<w (int address) (int cv) (int value) - write value to cv of address on the main track
	//	
	else if (cmdstring[0] == "w") {
		int address, cv, value;
		
		if (running) {
		
			if (cmdstring.size() == 4) {
				address = atoi(cmdstring[1].c_str());
				cv = atoi(cmdstring[2].c_str());
				value = atoi(cmdstring[3].c_str());
				
				DCCPacket p = DCCPacket::makeWriteCVToAddressPacket(MAIN1, MAIN2, address, cv, value);
				commandqueue.addCommand(p);
				commandqueue.addCommand(p);
				commandqueue.addCommand(p);
				commandqueue.addCommand(p);
				
				response << "<W " << address << " " << cv << " " << value <<">";
			}
			else {
				response << "<Error: malformed command.>";
			}
		}
		else response << "<Error: can't run in programming mode.>";

	}
	
	//<W cab>
	//<W cv value>
	else if (cmdstring[0] == "W") {
		if (programming) {
			int address, cv, value;
			
			DCCPacket r = DCCPacket::makeBaselineResetPacket(PROG1, PROG2);
			DCCPacket p;
		
			if (cmdstring.size() == 2) {
				address = atoi(cmdstring[1].c_str());
				p =  DCCPacket::makeServiceModeDirectWriteBytePacket(PROG1, PROG2, 1, address);
				response << "<W " << address  <<">";
			}
			else if (cmdstring.size() == 3) {
				cv = atoi(cmdstring[1].c_str());
				value = atoi(cmdstring[2].c_str());
				p =  DCCPacket::makeServiceModeDirectWriteBytePacket(PROG1, PROG2, cv, value);
				response << "<W " << cv  << " " << value << ">";
			}
			else return "Error: malformed command.";
			
			vc.lock();
			millisec = 1;  //throttle up the current monitor to support the ack resolution
			vc.unlock();
			
				
#ifdef USE_PIGPIOD_IF
			wave_clear(pigpio_id);
			wave_add_generic(pigpio_id, r.getPulseTrain().size(), r.getPulseTrain().data());
			char rwave = wave_create(pigpio_id);
			wave_add_generic(pigpio_id, p.getPulseTrain().size(), p.getPulseTrain().data());
			char pwave = wave_create(pigpio_id);
			char pchain[14] = {
				//S-9.2.3: 3 resets:
				rwave, rwave, rwave,
				//S-9.2.3: 5 writes:
				pwave, pwave, pwave, pwave, pwave,
				//S-9.2.3: 6 resets:
				rwave, rwave, rwave, rwave, rwave, rwave
			};
			gpio_write(pigpio_id, PROGENABLE, 1);
			wave_chain(pigpio_id, pchain , 14);
			while (wave_tx_busy(pigpio_id)) usleep(1000);
			gpio_write(pigpio_id, PROGENABLE, 0);
			wave_delete(pigpio_id, rwave);
			wave_delete(pigpio_id, pwave);
				
#else
			gpioWaveClear();
			gpioWaveAddGeneric(r.getPulseTrain().size(), r.getPulseTrain().data());
			char rwave = gpioWaveCreate();
			gpioWaveAddGeneric(p.getPulseTrain().size(), p.getPulseTrain().data());
			char pwave = gpioWaveCreate();
			char pchain[14] = {
				//S-9.2.3: 3 resets:
				rwave, rwave, rwave,
				//S-9.2.3: 5 writes:
				pwave, pwave, pwave, pwave, pwave,
				//S-9.2.3: 6 resets:
				rwave, rwave, rwave, rwave, rwave, rwave
			};
			gpioWrite(PROGENABLE, 1);
			gpioWaveChain(pchain, 14);
			while (gpioWaveTxBusy()) usleep(1000);
			gpioWrite(PROGENABLE, 0);
			gpioWaveDelete(rwave);
			gpioWaveDelete(pwave);
#endif

			vc.lock();
			millisec = MILLISEC_INTERVAL; 
			vc.unlock();				
				
			
		}
		else response << "<Error: can't program in ops mode.>";

	}
	
	//read CV:
	//<R CV CALLBACKNUM CALLBACKSUB> e.g., <R 32 0 0>
	//short version - callback fields can be omitted
	//if short version and appended wtih "log", e.g., "R 29 log", various information will be printed to stdout
	else if (cmdstring[0] == "R") {
		if (programming) {
			int cv, cb, cbsub;
			char msg[256];
			std::vector<float> currents; //collect current measurements during power-up sequence

			if (cmdstring.size() == 4) {
				cv = atoi(cmdstring[1].c_str());
				cb = atoi(cmdstring[2].c_str());
				cbsub = atoi(cmdstring[3].c_str());
			}
			else if (cmdstring.size() == 2) {
				cv = atoi(cmdstring[1].c_str());
			}
			else return "<Error: malformed command.>";

			DCCPacket r = DCCPacket::makeBaselineResetPacket(PROG1, PROG2);
			DCCPacket p;

			vc.lock();
			millisec = 1;  //throttle up the current monitor to support the ack resolution
			vc.unlock();
			usleep(1000*MILLISEC_INTERVAL);

			float quiescent = 800.0; //this will be modified in a few lines with a calculated value...

#ifdef USE_PIGPIOD_IF
			wave_clear(pigpio_id);
			wave_add_generic(pigpio_id, r.getPulseTrain().size(), r.getPulseTrain().data());
			char rwave = wave_create(pigpio_id);

			//S-9.2.3 power-up sequence, 20 valid packets to stabilize the decoder:
			std::vector<char> schain = {
				rwave, rwave, rwave, rwave, rwave, 
				rwave, rwave, rwave, rwave, rwave,
				rwave, rwave, rwave, rwave, rwave,
				rwave, rwave, rwave, rwave, rwave
			};

			if (logging) log("read CV: start 20 power up resets");
			gpio_write(pigpio_id, PROGENABLE, 1);
			wave_chain(pigpio_id, schain.data(), schain.size());
			while (wave_tx_busy(pigpio_id)) { currents.push_back(current); usleep(1000); }
			gpio_write(pigpio_id, PROGENABLE, 0);
			
			
#else
			gpioWaveClear();
			gpioWaveAddGeneric(r.getPulseTrain().size(), r.getPulseTrain().data());
			char rwave = gpioWaveCreate();

			//S-9.2.3 power-up sequence, 20 valid packets to stabilize the decoder:
			std::vector<char> schain = {
				rwave, rwave, rwave, rwave, rwave, 
				rwave, rwave, rwave, rwave, rwave,
				rwave, rwave, rwave, rwave, rwave,
				rwave, rwave, rwave, rwave, rwave
			};

			if (logging) log("read CV: start 20 power up resets");
			gpioWrite(PROGENABLE, 1);
			gpioWaveChain(schain.data(), schain.size());
			while (gpioWaveTxBusy()) { currents.push_back(current); usleep(1000); }
			gpioWrite(PROGENABLE, 0);
#endif

			if (logging) log("read CV: 20 power up resets complete");
			

			//calculate quiescent from the last 10 power-on current measurements:
			float q = 0.0;
			for (int i=currents.size()-sample_count; i<currents.size(); i++) {
				if (currents[i] > q) q = currents[i];
			}
			quiescent = q;
			
			snprintf(msg, 256, "read CV%d: quiescent=%04.2fma, acklimit=%04.4fma, ackmin=%d", cv, quiescent, ack_limit, ack_min);
			if (logging) log(msg);


			//Using bit-verify to walk the bits of the CV, collecting the 1s and 0s
			//First, start with bit 0, and do a verify on both 0 and 1.  If no ack
			//is returned for both, then there's no locomotive on the programming track,
			//or the connection is bad.  If '1' verify succeeds, set byte accumulator
			//to 1, else if 0 succeeds, set byte accumulator to 0, then do the rest of 
			//the bits.

			unsigned val;


			//walk only 1-bits, verify byte; try up to four times:
			int i;
			for (i=1; i<=3; i++) {
				//verify bit 0 by checking both for 1 and 0:
				if (verifyBit(rwave, quiescent, cv, 0, 1)) {
					val = 1;
				}
				else if (verifyBit(rwave, quiescent, cv, 0, 0)) {
					val = 0;
				}
				else {
					val = -1;
					continue;
				}

				//the rest of the bits:
				for (unsigned char i = 1; i < 8; i++) {
					if (verifyBit(rwave, quiescent, cv, i, 1)) {
						val = val | 1<<i; //if a 1 is found, else leave the bit alone (0)
					}
				}
				if (verifyByte(rwave, quiescent, cv, val)) break;
			}
			if (i == 1)
				snprintf(msg, 256, "read CV%d: %d attempt.", cv, i);
			else
				snprintf(msg, 256, "read CV%d: %d attempts.", cv, i);
			if (logging) log(msg);


/*			//walk both 1- and 0-bits:
			for (unsigned char i = 0; i < 8; i++) {
				bool foundone = verifyBit(rwave, quiescent, cv, i, 1);
				bool foundzed = verifyBit(rwave, quiescent, cv, i, 0);
				if (foundone & (!foundzed)) { 
					val = val | 1<<i; // bit at pos is 1, set it in val 
				} 
				else if (foundone & foundzed) { 
					val = -2; // strangeness... acklimit too low?
					break; 
				}
				else if ((!foundone) & (!foundzed)) {
					val = -1; // likely no locomotive or lost connection, maybe acklimit too high?
					break; 
				}
				// else bit at pos is 0 (!foundone & foundzed), leave it alone in val, which was initialized to all 0...
			}
*/
			
			snprintf(msg, 256, "Result: CV%d = %d", cv, val);
			if (logging) log(msg);			

			vc.lock();
			millisec = MILLISEC_INTERVAL;  //put the current monitor interval back to normal
			vc.unlock();
#ifdef USE_PIGPIOD_IF
			wave_clear(pigpio_id);
#else
			gpioWaveClear();
#endif

			//printf("CV%d = %d\n",cv, val); //debug

			if (cmdstring.size() == 4) 
				response << "<r " << cb << "|" << cbsub << "|" << (int) cv << " " << (int) val << ">";
			else if (cmdstring.size() == 2 | cmdstring.size() == 3)
				 response << "<r CV" << cv << "=" << (int) val << ">";

		}
		else response << "<Error: can't program in ops mode.>";
	}


	//RETURNS: Track power status, Version, Microcontroller type, Motor Shield type, build number, and then any defined turnouts, outputs, or sensors.
	//Example: <iDCC-EX V-3.0.4 / MEGA / STANDARD_MOTOR_SHIELD G-75ab2ab><H 1 0><H 2 0><H 3 0><H 4 0><Y 52 0><q 53><q 50>
	else if (cmdstring[0] == "s") {
		if (running)
			response << "<p1 MAIN><p0 PROG>";
		else if (programming)
			response << "<p1 PROG><p0 MAIN>";
		else
			response << "<p0>";
		//response << "<iwavedcc dev / RPi 3 / L298n>\n";

		//until JMRI gets a wavedcc status regex:
		response << "<iDCC-EX V-0.0.0 / MEGA / STANDARD_MOTOR_SHIELD G-75ab2ab>\n";

	}
	
	//wavedcc-unique, just sends power status.
	else if (cmdstring[0] == "sp") {
		if (running)
			response << "<p1 MAIN><p0 PROG>";
		else if (programming)
			response << "<p1 PROG><p0 MAIN>";
		else
			response << "<p0 MAIN><p0 PROG>";
		

	}
	
	//RETURNS: <c "CurrentMAIN" CURRENT C "Milli" "0" MAX_MA "1" TRIP_MA >
	else if (cmdstring[0] == "c") {
		vc.lock();
		float c = current;
		vc.unlock();
		if (overload_trip)
			response << "<c \"CurrentMAIN " << c << " C Milli 0 2000 1 1800 2 OVERLOAD >";
		else
			response << "<c \"CurrentMAIN " << c << " C Milli 0 2000 1 1800 >";
	}
	
	//appease JMRI... No turnouts, no output pins, no sensors. 
	else if ((cmdstring[0] == "T") | (cmdstring[0] == "Z") | (cmdstring[0] == "S") | (cmdstring[0] == "#")) {
		response << "<X>\n";
	}
	
	//Appease JMRI, <#> request max slots, not documented in DCC++ EX.  Reply is just a large number.
	else if (cmdstring[0] == "#") {
		response << "<# 1000d>\n";
	}
	
	else if (cmdstring[0] == "ws") {
#ifdef USE_PIGPIOD_IF
		response << "Remote hardware version: " << get_hardware_revision(pigpio_id) << "\n";
		response << "Remote pigpio(if) version: " << get_pigpio_version(pigpio_id) << "(" << pigpiod_if_version() << ")" << "\n";
		response << "Remote pigpiod DCBs: " << wave_get_max_cbs(pigpio_id) << "\n";
#else
		response << "Local pigpiod DCBs: " << gpioWaveGetMaxCbs() << "\n";
#endif
		if (steps28)
			response << "Speed step mode: 28\n";
		else
			response << "Speed step mode: 128\n";
		if (running) 
			response << "DCC pulsetrain running" << "\n";
		else
			response << "DCC pulsetrain stopped" << "\n";
	}
	
	else if (cmdstring[0] == "l") {
		response << roster.list();
	}
	
	else if (cmdstring[0] == "test") {
		if (!running) {
			DCCPacket testpacket = DCCPacket::makeBaselineSpeedDirPacket(MAIN1, MAIN2, 3,1,1,true);
#ifdef USE_PIGPIOD_IF
			wave_add_generic(pigpio_id, testpacket.getPulseTrain().size(), testpacket.getPulseTrain().data());
			int wid =  wave_create(pigpio_id);
			wave_send_using_mode(pigpio_id, wid, PI_WAVE_MODE_ONE_SHOT);
			while (wave_tx_at(pigpio_id) == wid) usleep(1000);
			wave_delete(pigpio_id,  wid);
#else
			gpioWaveAddGeneric(testpacket.getPulseTrain().size(), testpacket.getPulseTrain().data());
			int wid = gpioWaveCreate();
			gpioWaveTxSend(wid, PI_WAVE_MODE_ONE_SHOT);
			while(gpioWaveTxAt() == wid) usleep(1000);
			gpioWaveDelete(wid);
#endif	
			response << "Test packet sent: " << testpacket.getPulseString() << "  ones: " << testpacket.getOnes() << "  zeros: " << testpacket.getZeros();
		}
		else response << "Error: can't send a test packet while the dcc pulse train is running.";
	}
	
	//temporary, for debugging
	else if (cmdstring[0] == "at") {
		int address, speed, direction;
		if (cmdstring.size() == 4) {
			address = atoi(cmdstring[1].c_str());
			speed = atoi(cmdstring[2].c_str());
			direction = atoi(cmdstring[3].c_str());
			response << "<AT 1 " << speed << " " << direction << ">";
		}
		else {
			response << "<Error: malformed command.>";
		}
		
		DCCPacket p = DCCPacket::makeBaselineSpeedDirPacket(MAIN1, MAIN2, address, direction, speed, headlight);
		response << p.getPulseString();
	}
	
	else response << "Error: unrecognized command: " << cmdstring[0];

	return response.str();



	//to-do:
	

	//<!> - emergency stop all trains, leave track power on, returns NONE
	//		+ BaselineBroadcastStopPacket

	//<F (int address) (int function) (1|0 on/off)> cab function: lights, horn, bell, etc. (this will require a dccwave-maintained roster), returns NONE

	//<W (int address)> - write locomotive address to the programming track
	//		-+Service Mode Direct Address, CV#19, to the programming track (Long-preamble(>=20 bits) 0 0111CCAA 0 AAAAAAAA 0 DDDDDDDD 0 EEEEEEEE 1)

	//Notes:
	// For initial implementation, service and ops modes will be mutually exclusive, guarded by the command logic, so they can exclusively use the waveform generator.
	// ToDo: guard all baseline data boundaries, like addresses...

	//Research:
	//Function control (headlight, sound, etc)
	//Consisting
	//CVs on the main (ops mode programming)


}

void dccFinish()
{
	running = false;
	if (t && t->joinable()) {
		t->join();
		t->~thread();
		t = NULL;
	}
#ifdef USE_PIGPIOD_IF
	pigpio_stop(pigpio_id);
#else
	gpioTerminate();
#endif
}

