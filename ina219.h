

/*
	ic2_read(), ic2_write(), register_read(), register_write(), get_*() and associated #defines from:
	https://github.com/ZigFisher/Glutinium/blob/master/i2c-telemetry/src/ina219.c
	GPL 2.0 License
	
	Modified to use pigpio I2C routines.
*/

#ifdef USE_PIGPIOD_IF
#include <pigpiod_if2.h> 
#else
#include <pigpio.h>
#endif

#include <stdio.h>
#include "pigpio_errors.h"

#define CONFIG_REG          0
#define SHUNT_REG           1
#define BUS_REG             2
#define POWER_REG           3
#define CURRENT_REG         4
#define CALIBRATION_REG     5

class INA219
{
public:
	INA219() { }

#ifdef USE_PIGPIOD_IF
	void configure (int pigpioid)
	{
		i2c_bus = 1;
		i2c_address=0x40;
		pigpio_id = pigpioid;
		if ((i2c_handle = i2c_open(pigpio_id, i2c_bus, i2c_address, 0)) < 0) err(i2c_handle);
		//register_write( CONFIG_REG, 0x1eef);		//16V
		register_write( CONFIG_REG, 0x3eef); 		//32V
		register_write( CALIBRATION_REG, 0x8332);
	}
#else
	void configure()
	{
		i2c_bus = 1;
		i2c_address=0x40;
		if (i2c_handle = i2cOpen(i2c_bus, i2c_address, 0) < 0) err(i2c_handle);
		//register_write( CONFIG_REG, 0x1eef);		//16V
		register_write( CONFIG_REG, 0x3eef); 		//32V
		register_write( CALIBRATION_REG, 0x8332);
	}
#endif


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


	void err(int error, const char * msg)
	{
		err_rec r = pigpioError(error);
		printf("%s: %s - %s\n",r.name.c_str(), r.description.c_str());
	}

	int i2c_read( unsigned char *buf, int len )
	{
		int rc = 0;
#ifdef USE_PIGPIOD_IF
		if (rc = i2c_read_device(pigpio_id, i2c_handle, (char *) buf, len) <= 0 ) 
#else
		if (rc = i2cReadDevice(i2c_handle, (char *) buf, len) <= 0 ) 
#endif

		{
			err(rc, "I2C read");
			rc = -1;
		}

		return rc;
	}


	int i2c_write( unsigned char *buf, int len )
	{
		int rc = 0;

#ifdef USE_PIGPIOD_IF
		if (rc = i2c_write_device(pigpio_id, i2c_handle, (char *) buf, len) != 0)
#else
		if (rc = i2cWriteDevice(i2c_handle, (char *) buf, len) != 0)
#endif
		{
			err(rc, "I2C write");
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


private:
	int i2c_bus, i2c_address, i2c_handle, pigpio_id;
	
};


