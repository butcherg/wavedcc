# wavedcc
Digital command control (DCC) for model railroads using the Raspberry Pi and libpigpio' s wave functions.

This DCC implementation uses the waves functionality of libpigpio to offload the DCC waveform execution to DMA-gated PWMs.  
The dccpacket.* files contain the class that builds the DCC pulse trains for various packets, and dccengine.* has the code 
to intialize, parse and execute commands, and terminate DCC functions.  wavedcc.cpp and wavedccd.cpp implement a command line
and TCP interface, respectively.  The message format is the DCC++-EX specification, but you can leave out the < and >.

At the PI GPIOs, a bipolar interface is implemented with two output pins.  There is also a separate enable pin for L298-style 
H-bridges.  Configuration of these is to be provided in a wavedcc.conf file, which currently must sit in the same directory as
the executables.

## Building wavedcc

### Prerequisites: 
```
sudo apt install libpigpio-dev libpigpiod-if-dev pigpiod 
```
### Direct GPIO access, CMake:
```
$ mkdir build
$ cd build
$ cmake ..
$ make
```

### GPIO access through pigpiod, CMake:
```
$ mkdir build
$ cd build
$ cmake -DUSE_PIGPIOD_IF=ON ..
$ make
```

The major differences between direct GPIO access pigpiod GPIO access, besides haveing to run the programs as root for 
direct access, is the CPU loading; on my RPi 3B+, direct uses about 12%, pigpiod access uses about 40%, split between 
pigpiod and wavedcc(d).

## Running wavedcc(d)

If the GPIO access through pigpiod was compiled, make sure pigpiod is running.  If not:
```
$ sudo systemctl start pigpiod
```
Then, either program can be run from the command line as the regular user.  If the direct GPIO access was compiled, wavedcc and
wavedccd must be run as root.

Both wavedcc and wavedccd use GPIO 2 and 3 by default for the DCC signal modulation (MAIN1 and MAIN2) and GPIO 4 for MAINENABLE; 
it's recommended to use a wavedcc.conf file residing in the same directory as the executables to set these properties to the
GPIOs you prefer.  The corresponding properies for service mode are PROG1, PROG2, and PROGENABLE.  Both programs if compiled with 
-DUSE_PIGPIOD_IF=ON will by default attempt to connect to a pigpiod at localhost using port 8888; these can be changed in the 
wavedcc.conf file with HOST and PORT properties.

wavedcc is a command-line program that accepts DCC++ style commands, either encased in the < > or without, e.g., t 3 1 1.  It can
be exited with Ctrl-c or 'exit'.

wavedccd is a TCP server that listens for connections on port 9034.  wavedccd emulates DCC++ EX, JMRI can be set up to communicate
with wavedccd by setting up a DCC++ connection to the apporpriate host and port.

## Limitations

The most significant limitation is that wavedcc and wavedccd will only run one DCC mode at a time, ops or service.  This is a 
limitation of pigpio waveforms, which will only accommodate one pulse train at a time.  This is not likely to change, as it would
require a significant redesign of wavedcc to multiplex waveforms.

Maybe not a limitation per se, but I tried it and it didn't work well: Running wavedcc or wavedccd on any other host than localhost to
pigpiod.  I tried it, and network latency appears to introduce packet gaps in the pulse train.  This was on a Ethernet-connected desktop, 
to a WiFi-connected RPi; YMMV...

The following limitations are just a function of the state of wavedcc development; I intend to eventually implement them:

- Only functions 1-12 are implemented.
- Consist control is not implemented.
- CV reading is not implemented, but it's the next capability to be developed.
- Measuring and reporting current draw is not implemented, but will be implemented along with CV reading.
- DCC accessory packets are not implemented.
- DCC++ sensors and outputs (RPi GPIOs) are not implemented.

## License

All code in this repository is Copyright 2021 by Glenn Butcher, all rights reserved.  However, I sanction its use under the terms of the 
GPL 3.0 License.

Have Fun!
