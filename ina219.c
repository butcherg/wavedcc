/* 
	ina219.h - Interface to INA219 current-sense module using the pigpiod I2C routines

*/

#include <stdio.h>
#include <errno.h>
#include <string.h>

#ifdef USE_PIGPIOD_IF
#include <pigpiod_if2.h> 
#else
#include <pigpio.h>
#endif

/*
	ic2_read(), ic2_write(), register_read(), register_write(), get_*() and associated #defines from:
	https://github.com/ZigFisher/Glutinium/blob/master/i2c-telemetry/src/ina219.c
	GPL 2.0 License
	
	Modified to use pigpio I2C routines.
*/

#define CONFIG_REG          0
#define SHUNT_REG           1
#define BUS_REG             2
#define POWER_REG           3
#define CURRENT_REG         4
#define CALIBRATION_REG     5

#define INA_ADDRESS         0x40

int i2c_bus = 1;
int i2c_address = INA_ADDRESS;
int ina219_pigpio_id, i2c_handle;


int i2c_read( unsigned char *buf, int len )
{
    int rc = 0;
#ifdef USE_PIGPIOD_IF
	if (i2c_read_device(ina219_pigpio_id, i2c_handle, (char *) buf, len) <= 0 ) 
#else
	if (i2cReadDevice(i2c_handle, (char *) buf, len) <= 0 ) 
#endif

	//if (i2c_read_device(ina219_pigpio_id, i2c_handle, (char *) buf, len) <= 0 )
	{
		printf( "I2C read failed\n" );
		rc = -1;
	}

	return rc;
}


int i2c_write( unsigned char *buf, int len )
{
	int rc = 0;

#ifdef USE_PIGPIOD_IF
	if (i2c_write_device(ina219_pigpio_id, i2c_handle, (char *) buf, len) != 0)
#else
	if (i2cWriteDevice(i2c_handle, (char *) buf, len) != 0)
#endif
	{
		printf( "I2C write failed\n" );
		rc = -1;
	}

    return rc;
}


int register_read( unsigned char reg, unsigned short *data )
{
    int rc = -1;
    unsigned char bite[ 4 ];

    bite[ 0 ] = reg;
    if ( i2c_write( bite, 1 ) == 0 )
    {
        if ( i2c_read( bite, 2 ) == 0 )
        {
            *data = ( bite[ 0 ] << 8 ) | bite[ 1 ];
            rc = 0;
        }
    }

    return rc;
}


int register_write( unsigned char reg, unsigned short data )
{
    int rc = -1;
    unsigned char bite[ 4 ];

    bite[ 0 ] = reg;
    bite[ 1 ] = ( data >> 8 ) & 0xFF;
    bite[ 2 ] = ( data & 0xFF );

    if ( i2c_write( bite, 3 ) == 0 )
    {
        rc = 0;
    }

    return rc;
}

float get_voltage()
{
    short busv;
    if ( register_read( BUS_REG, (unsigned short*)&busv ) != 0 ) return -1;
    return ( float )( ( busv & 0xFFF8 ) >> 1 );
}

float get_shunt_voltage()
{
    short shuntv;
    if ( register_read( SHUNT_REG, (unsigned short*)&shuntv ) != 0 ) return -1;
    return ( float )( ( shuntv & 0xFFF8 ) >> 1 );
}


float get_current( )
{
    short current;
    if ( register_read( CURRENT_REG, (unsigned short*)&current ) != 0 ) return -1;
    return (float)current / 10;
}

short get_config()
{
    short config;
    if ( register_read( CONFIG_REG, (unsigned short*)&config ) != 0 ) return -1;
    return config;
}

short get_calibration()
{
    short calibration;
    if ( register_read( CALIBRATION_REG, (unsigned short*)&calibration ) != 0 ) return -1;
    return calibration;
}


//use this to do the complete pigpio configure
int pigpio_ic2_configure(const char *addrStr, const char *portStr)
{
	int result;
	
#ifdef USE_PIGPIOD_IF
	if (result = ina219_pigpio_id = pigpio_start(addrStr, portStr) < 0) return result;
	if (result = i2c_handle = i2c_open(ina219_pigpio_id, i2c_bus, i2c_address, 0) < 0 ) return result;
#else
	if (result =  gpioInitialise() < 0) return result;
	if (result = i2c_handle = i2cOpen(i2c_bus, i2c_address, 0) < 0 ) return result;
#endif
	return 1;
}

//use this to use a previously obtained ina219_pigpio_id:
#ifdef USE_PIGPIOD_IF
int i2c_configure(int pigpioid)
#else
int i2c_configure()
#endif
{
	int result;

#ifdef USE_PIGPIOD_IF
	ina219_pigpio_id = pigpioid;
	i2c_handle = i2c_open(ina219_pigpio_id, i2c_bus, i2c_address, 0);
#else
	i2c_handle = i2cOpen(i2c_bus, i2c_address, 0);
#endif
	return i2c_handle;
}

void i2c_closeout()
{
#ifdef USE_PIGPIOD_IF
	i2c_close(ina219_pigpio_id, i2c_handle);
#else
	i2cClose(i2c_handle);
#endif
}

void ina219_configure()
{
	// the magic numbers...
	//register_write( CONFIG_REG, 0x1eef);		//16V
	register_write( CONFIG_REG, 0x3eef); 		//32V
	register_write( CALIBRATION_REG, 0x8332); 	//have  no idea, snarfed it from the adafruit python example by reading the reg after it ran...
}
