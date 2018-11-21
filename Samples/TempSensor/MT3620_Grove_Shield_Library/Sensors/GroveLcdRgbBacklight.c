
#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include "../HAL/GroveI2C.h"
#include "../Common/Delay.h"
#include "GroveLcdRgbBacklight.h"

#define RGBADDR 0xC4
#define TXTADDR 0x7C

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

    // The next two commands initialize the backlight display
    uint8_t command_4[4] = { 0, 0, 1, 0 };

    SendCommand(this, RGBADDR, command_4, 4);

    uint8_t command_5[2] = { DISP_ON_CMD, 0xaa };

    SendCommand(this, RGBADDR, command_5, 2);

    // The next three commands initialize the text display
    uint8_t command_1[2] = { TEXT_CMD, (DISP_ON_CMD | NOCURSOR_CMD) };

    SendCommand(this, TXTADDR, command_1, 2);

    uint8_t command_2[2] = { TEXT_CMD, TWO_LINES_CMD };

    SendCommand(this, TXTADDR, command_2, 2);

    GroveLcdRgbBacklight_ClearDisplay(this);

	return this;
}

void GroveLcdRgbBacklight_ClearDisplay(void *this)
{
    uint8_t command_3[2] = { TEXT_CMD, CLEAR_CMD };

    SendCommand(this, TXTADDR, command_3, 2);
}

void GroveLcdRgbBacklight_SetBacklightRgb(void *this, uint8_t red, uint8_t green, uint8_t blue)
{
    uint8_t command[2] = { RED_CMD, red };

    SendCommand(this, RGBADDR, command, 2);

    command[0] = GRN_CMD;
    command[1] = green;

    SendCommand(this, RGBADDR, command, 2);

    command[0] = BLU_CMD;
    command[1] = blue;

    SendCommand(this, RGBADDR, command, 2);
}
