#pragma once

#define __NEED_uint8_t

#include <bits/alltypes.h>
#include <applibs/gpio.h>

struct measurement {
    uint8_t temperature;
    uint8_t humidity;
};

struct dht11 
{
    int gpioFd;
    GPIO_Id id;
};

int InitDht11( struct dht11 *, GPIO_Id );

int Measure(struct dht11 *, struct measurement *);

void DeinitDht11(struct dht11 *);