#include "DatagramSocket.h"
#include <stdio.h>
#include <string.h>


int main(int argc, char **argv)
{
	FILE *f;
	DatagramSocket s (9035, (char *) "127.0.0.1", FALSE, TRUE);	
	long len;
	char msg[256], buf[256];
	
	if (argc >= 2)
		f = fopen(argv[1], "w");
	else
		f = stdout;
		
	while (1) {
		len = s.receive(msg, 255);
		msg[len] = '\0';
		snprintf(buf, 256, "%s\n", msg);
		fwrite(buf, 1, strlen(buf), f);
		fflush(f);
	}

}

