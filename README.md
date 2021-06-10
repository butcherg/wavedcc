# wavedcc
Digital command control (DCC) for model railroads using the Raspberry Pi and libpigpio' s wave functions.

This DCC implementation uses the waves functionality of libpigpio to offload the DCC waveform execution to DMA-gated PWMs.  
The dccpacket.* files contain the class that builds the DCC pulse trains for various packets, and dccengine.* has the code 
to intialize, parse and execute commands, and terminate DCC functions.  wavedcc.cpp and wavedccd.cpp implement a command line
and TCP interface, respectively.  The message format is the DCC++-EX specification, but you can leave out the < and >.

The initial commit just implements the NMRA S9.2 Baseline packets.  Also, only operations mode is implemented at present, but
the infrastructure to to programming mode is present.  And, the pulse train builder for speed/direction packets only recognizes 
14-step mode.

At the PI GPIOs, a bipolar interface is implemented with two output pins.  There is also a separate enable pin for L298-style 
H-bridges.  Configuration of these is to be provided in a wavedcc.conf file, which currently must sit in the same directory as
the executables.

To build the code, it is recommended that a separate build/ directory be created, cd to that, then run ../configure.  That'll 
create an appropriately targete Makefile in the build directory, with which you can then run make.  By default, the 
makefile builds programs that use the socket interface to pigpiod; if you want to control the GPIOs directly, comment out
the CFLAGS and LFLAGs lines for libpigpiod in the Makefile and uncomment the corresponding libpigpio CFLAGS and LFLAGS lines.
All this assumes you're compiling on the Raspberry Pi itself, but the Makefile can be easily modified to cross-compile, e.g., 
with a buildroot toolchain.

All code in this repository is Copyright 2021 by Glenn Butcher, all rights reserved.  However, I license its use under the MIT 
License.

Have Fun!
