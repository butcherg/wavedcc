# wavedcc
Digital command control (DCC) for model railroads using the Raspberry Pi and libpigpio' s wave functions.

This DCC implementation uses the waves functionality of libpigpio to offload the DCC waveform execution to DMA-gated PWMs. The dccpacket.* files contain the class that builds the DCC pulse trains for various packets, and dccengine.* has the code to intialize, parse and execute commands, and terminate DCC functions.  wavedcc.cpp and wavedccd.cpp implement a command line and TCP interface, respectively.  The message format is the DCC++-EX specification, but you can leave out the < and >.

At the PI GPIOs, a bipolar interface is implemented with two output pins.  There is also a separate enable pin for L298-style H-bridges.  Configuration of these is to be provided in a wavedcc.conf file, which currently must sit in the same directory as the executables.

## Building wavedcc

### Prerequisites: 
```
sudo apt install libpigpio-dev libpigpiod-if-dev pigpiod cmake
```
### Direct GPIO access:
```
$ mkdir build
$ cd build
$ cmake -DUSE_PIGPIO=ON ..
$ make
```
### GPIO access through pigpiod:
```
$ mkdir build
$ cd build
$ cmake -DUSE_PIGPIOD_IF=ON ..
$ make
```
If you don't specify a -D option, cmake will configure the USE_PIGPIOD_IF option.

The major differences between direct GPIO access and pigpiod GPIO access, besides having to run the programs as root for direct access, is the CPU loading; on my RPi 3B+, direct uses about 12%, pigpiod access uses about 40%, split between pigpiod and wavedcc(d).

## Running wavedcc(d)

If the GPIO access through pigpiod was compiled, make sure pigpiod is running.  If not:
```
$ sudo systemctl start pigpiod
```
Then, either program can be run from the command line as the regular user.  

If the direct GPIO access version of wavedcc was compiled, wavedcc and wavedccd must be run as root, and pigpiod cannot be running.

Both wavedcc and wavedccd use GPIO 2 and 3 by default for the DCC signal modulation (main1 and main2 in wavedcc.conf) and GPIO 4 for mainenable; it's recommended to use a wavedcc.conf file residing in the same directory as the executables to set these properties to the GPIOs you prefer.  The corresponding properies for service mode are prog1, prog2, and progenable.  Both programs if, compiled with -DUSE_PIGPIOD_IF=ON, will by default attempt to connect to a pigpiod at localhost using port 8888; these can be changed in the wavedcc.conf file with host and port properties.

wavedcc is a command-line program that accepts DCC++ style commands, either encased in the < > or without, e.g., t 3 1 1.  It can be exited with Ctrl-c or 'exit'.

wavedccd is a TCP server that listens for connections on port 9034.  wavedccd emulates DCC++ EX, JMRI can be set up to communicate with wavedccd by setting up a DCC++ connection to the apporpriate host and port.

## Hardware

wavedcc will drive a bipolar H-bridge with the GPIOs identified with the main1/main2 properties in wavedcc.conf.  If the H-bridge also has an enable input, that can be driven with the GPIO identified by mainenable.  The service mode commands have separate prog1/prog2/progenable properties.

I've tested wavedcc with the generic L298n motor driver board available a lot of places.  Here's a current Amazon link (as of 8/17/2021): https://www.amazon.com/Controller-H-Bridge-Stepper-Control-Mega2560/dp/B07WS89781/. If you just want to mess with ops mode, use this with a relatively low amperage 12-15v power supply and take care to avoid short circuits.  However, for safety as well as doing CV reads, current sensing is recommended.  wavedcc currently is only configured to read I2C current-sense devices, and the only one I've tested is the Adafruit INA219 board.  I've looked at the datasheet for their INA260 board, and it looks like it would require changing the register numbers in ina219.h, as well as commenting out the configuration/calibration lines, so very much YMMV.

Here's a picture of my configuration:

![wavedcc hardware](https://github.com/butcherg/wavedcc/blob/main/DSZ_8768.jpg)

I use breadboard jumpers throughout.  There are two groups of jumpers:

1. GPIO17/27/22 to L298n IN1a/IN1b/INEnable, along with a ground connection from PRi pin 9 to the L298n ground,
2. From the RPi channel 1 I2c at GPIO2/3 to the Qwiik connector, also using pins 1 and 6 for 3v/GND.  

For the power through the INA219, power + goes to V+, and the V- goes to load +.  I chose to measure the current before the motor driver for connection simplicity, and wavedcc measures the total current prior to an ack pulse to detect acks.  My L298n draws about 145ma, and a HOn3 K-27 with a Soundtraxx Tsnuami decoder adds about 500ma to that.


## Limitations

The most significant limitation is that wavedcc and wavedccd will only run one DCC mode at a time, ops or service.  This is a limitation of pigpio waveforms, which will only accommodate one pulse train at a time.  This is not likely to change, as it would require a significant redesign of wavedcc to multiplex waveforms.

Maybe not a limitation per se, but I tried it and it didn't work well: Running wavedcc or wavedccd on any other host than localhost to pigpiod.  I tried it, and network latency appears to introduce packet gaps in the pulse train.  This was on a Ethernet-connected desktop, to a WiFi-connected RPi; YMMV...

The following limitations are just a function of the state of wavedcc development; I intend to eventually implement them:

- Only functions 1-12 are implemented.
- Consist control is not implemented.
- DCC accessory packets are not implemented.
- DCC++ sensors and outputs (RPi GPIOs) are not implemented.

## License

All code in this repository is Copyright 2021 by Glenn Butcher, all rights reserved.  However, I sanction its use under the terms of the GPL 3.0 License.

There's an exception, for pigpio_errors.h.  It's really just a reorganization of the libpigpio error constants into a table, along with their explanatory prose, and a lookup function for the table.  In the spirit of the libpigpio project, I release that file into the public domain.

Have Fun!
