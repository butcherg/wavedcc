#ifdef USE_PIGPIOD_IF
#include <pigpiod_if2.h> 
#else
#include <pigpio.h>
#endif

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>

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
};

class Roster
{
public:
	Roster() 
	{
		next = rr.begin();
	}
	
	void update(unsigned address, unsigned speed, unsigned direction, unsigned headlight)
	{
		m.lock();
		rr[address] = roster_item{ address, speed, direction, headlight}; 
		m.unlock();
	}
	
	roster_item getNext()
	{
		if (rr.size() == 0) return roster_item{ 0, 0, 0, 0}; 
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
	std::ifstream infile(filename);
	if (!infile.is_open()) return config; //empty config
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

//flag to control programming:
bool programming = false;

//flag to control speed step mode:
bool steps28 = true;

//GPIO ports to use for DCC output, set in the first part of main()
int MAIN1, MAIN2, MAINENABLE;
int PROG1, PROG2, PROGENABLE;

//for runDCC() engine thread:
std::thread *t = NULL;


#ifdef USE_PIGPIOD_IF
//pigpiod_id hold the identifier returned at initialization, needed by all pigpiod function calls
int pigpio_id;
#endif

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
		while (wave_tx_at(pigpio_id) == wid) time_sleep(0.001);
		wave_delete(pigpio_id,  wid);
#else
		gpioWaveAddGeneric(commandPacket.getPulseTrain().size(), commandPacket.getPulseTrain().data());
		nextWid = gpioWaveCreatePad(50, 50, 0);
		gpioWaveTxSend(nextWid, PI_WAVE_MODE_ONE_SHOT_SYNC);
		while(gpioWaveTxAt() == wid) time_sleep(0.001);
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
	pigpio_stop(pigpio_id);
#else
	gpioTerminate();
#endif
	exit(1);
}

std::string dccInit()
{
	MAIN1=2;
	MAIN2=3;

	//read configuration from wavedcc.conf
	std::map<std::string, std::string> config;
	if (fileExists("wavedcc.conf")) config = getConfig("wavedcc.conf");

	//for configuration debug:
	//for(std::map<std::string, std::string>::iterator it = config.begin(); it!= config.end(); ++it)
	//	std::cout << it->first << " = " << it->second << std::endl;
	
	if (config.find("main1") != config.end()) MAIN1 = atoi(config["main1"].c_str());
	if (config.find("main2") != config.end()) MAIN2 = atoi(config["main2"].c_str());
	if (config.find("mainenable") != config.end()) MAINENABLE = atoi(config["mainenable"].c_str());
	if (config.find("prog1") != config.end()) MAIN1 = atoi(config["prog1"].c_str());
	if (config.find("prog2") != config.end()) MAIN2 = atoi(config["prog2"].c_str());
	if (config.find("progenable") != config.end()) MAINENABLE = atoi(config["progenable"].c_str());

#ifdef USE_PIGPIOD_IF
	std::string host = "localhost";
	std::string port = "8888";
	if (config.find("host") != config.end()) host = config["host"];
	if (config.find("port") != config.end()) host = config["port"];
	if (pigpio_start((char *) host.c_str(), (char *) port.c_str()) < 0) return "Error: GPIO Initialization failed.";
	set_mode(pigpio_id, MAIN1, PI_OUTPUT);
	set_mode(pigpio_id, MAIN2, PI_OUTPUT);
	set_mode(pigpio_id, MAINENABLE, PI_OUTPUT);
	set_mode(pigpio_id, PROG1, PI_OUTPUT);
	set_mode(pigpio_id, PROG2, PI_OUTPUT);
	set_mode(pigpio_id, PROGENABLE, PI_OUTPUT);
	wave_clear(pigpio_id);
	std::string wavelet_mode = "remote (" + host + ")";
	signal(SIGINT, signal_handler);
#else
	if (gpioInitialise() < 0) return "Error: GPIO Initialization failed.";
	gpioSetMode(MAIN1, PI_OUTPUT);
	gpioSetMode(MAIN2, PI_OUTPUT);
	gpioSetMode(MAINENABLE, PI_OUTPUT);
	gpioSetMode(PROG1, PI_OUTPUT);
	gpioSetMode(PROG2, PI_OUTPUT);
	gpioSetMode(PROGENABLE, PI_OUTPUT);
	gpioWaveClear();
	std::string wavelet_mode = "native";
	gpioSetSignalFunc(SIGINT, signal_handler);
#endif

	std::stringstream result;
	result << "outgpios: " << MAIN1 << "|" << MAIN2 << std::endl << "mode: " << wavelet_mode << std::endl;
	return result.str();
}

//global, used to run a single engine with adr/+/-:
int address=0, speed=0, direction=1;
bool headlight=false;

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
						running = true;
						t = new std::thread(&runDCC);
#ifdef USE_PIGPIOD_IF
						gpio_write(pigpio_id, MAINENABLE, 1);
#else
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
					gpio_write(pigpio_id, PROGENABLE, 1);
#else
					gpioWrite(PROGENABLE, 1);
#endif
					response << "<p1 PROG>";
					
				}
			}
			else response << "<Error: invalid mode.>";
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
					response <<  "<p0 MAIN>";
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
					response << "<p0 PROG>";
					
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
				response <<  "<p0 MAIN>";
			}
			else if (programming) {
				programming = false;
#ifdef USE_PIGPIOD_IF
				gpio_write(pigpio_id, PROGENABLE, 0);
#else
				gpioWrite(PROGENABLE, 0);
#endif
				response << "<p0 PROG>";
			}
		}
	}

	//<D SPEED28|SPEED128> - changes the step mode for <t> commands
	else if (cmdstring[0] == "D") {
		if (cmdstring[1] == "CABS") return roster.list();
		else if (cmdstring[1] == "SPEED28") steps28 = true;
		else if (cmdstring[1] == "SPEED28") steps28 = false;
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
	//		+ BaselineSpeedDirPacket
	//		- ExtendedSpeedDirPacket
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

			commandqueue.addCommand(p);
			roster.update(address, speed, direction, headlight);
		} 
		else response << "<Error: can't run in programming mode.>";
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
				response << "<W " << address << " " << cv << " " << value <<">";
			}
			else {
				response << "<Error: malformed command.>";
			}
		
			DCCPacket p = DCCPacket::makeWriteCVToAddressPacket(MAIN1, MAIN2, address, cv, value);
			commandqueue.addCommand(p);
			//commandqueue.addCommand(p);
			//commandqueue.addCommand(p);
			//commandqueue.addCommand(p);
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
				p =  DCCPacket::makeServiceModeDirectPacket(PROG1, PROG2, 1, address);
				response << "<W " << address  <<">";
			}
			else if (cmdstring.size() == 3) {
				cv = atoi(cmdstring[1].c_str());
				value = atoi(cmdstring[2].c_str());
				p =  DCCPacket::makeServiceModeDirectPacket(PROG1, PROG2, cv, value);
				response << "<W " << cv  << " " << value << ">";
			}
			else return "Error: malformed command.";
			
				
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
				wave_chain(pigpio_id, pchain , 14);
				while (wave_tx_busy(pigpio_id)) time_sleep(0.1);
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
				gpioWaveChain(pchain, 14);
				while (gpioWaveTxBusy()) time_sleep(0.1);
				gpioWaveDelete(rwave);
				gpioWaveDelete(pwave);
#endif
				
				
			
		}
		else response << "<Error: can't program in ops mode.>";

	}

	else if (cmdstring[0] == "s") {
#ifdef USE_PIGPIOD_IF
		response << "Remote hardware version: " << get_hardware_revision(pigpio_id) << "\n";
		response << "Remote pigpio(if) version: " << get_pigpio_version(pigpio_id) << "(" << pigpiod_if_version() << ")" << "\n";
		response << "Remote pigpiod DCBs: " << wave_get_max_cbs(pigpio_id) << "\n";
#else
		response << "Local pigpiod DCBs: " << gpioWaveGetMaxCbs() << "\n";
#endif
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
			while (wave_tx_at(pigpio_id) == wid) time_sleep(0.01);
			wave_delete(pigpio_id,  wid);
#else
			gpioWaveAddGeneric(testpacket.getPulseTrain().size(), testpacket.getPulseTrain().data());
			int wid = gpioWaveCreate();
			gpioWaveTxSend(wid, PI_WAVE_MODE_ONE_SHOT);
			while(gpioWaveTxAt() == wid) time_sleep(0.1);
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

