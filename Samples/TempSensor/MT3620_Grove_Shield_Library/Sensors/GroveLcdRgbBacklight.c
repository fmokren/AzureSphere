#include "GroveLcdRgbBacklight.h"
#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include "../HAL/GroveI2C.h"
#include "../Common/Delay.h"

#define RGBADDR 0x62
#define TXTADDR 0x3E

#define RED_CMD 4
#define GRN_CMD 3
#define BLU_CMD 2

#define TEXT_CMD        0x80
#define CLEAR_CMD       0x01
#define DISP_ON_CMD     0x08
#define NOCURSOR_CMD    0x04
#define TWO_LINES_CMD   0x28
#define CHAR_CMD        0x40

#define LCD_WIDTH   16
#define LCD_HEIGHT   2

typedef struct
{
	int I2cFd;
}
GroveLcdRgbBacklightInstance;


static void SendCommand(GroveLcdRgbBacklightInstance* this, uint8_t addr, uint8_t *data, uint8_t dataLen)
{
	GroveI2C_Write(this->I2cFd, addr, data, dataLen);
}

void* GroveLcdRgbBacklight_Open(int i2cFd)
{
    GroveLcdRgbBacklightInstance* this = (GroveLcdRgbBacklightInstance*)malloc(sizeof(GroveLcdRgbBacklightInstance));

	this->I2cFd = i2cFd;

    uint8_t command_1[2] = { TEXT_CMD, (DISP_ON_CMD | NOCURSOR_CMD) };

    SendCommand(this, TXTADDR, command_1, 2);

    uint8_t command_2[2] = { TEXT_CMD, TWO_LINES_CMD };

    SendCommand(this, TXTADDR, command_2, 2);

    uint8_t command_3[2] = { TEXT_CMD, CLEAR_CMD };

    SendCommand(this, TXTADDR, command_3, 2);

	return this;
}

