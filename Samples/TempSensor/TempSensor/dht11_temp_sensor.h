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

struct grove_dht11
{
    int gpioFd;
    int gpioFd_Pwr;
    int gpioFd_Gnd;
};

int InitDht11( struct dht11 *, GPIO_Id );

int InitGroveDht11(struct grove_dht11 *, GPIO_Id, GPIO_Id, GPIO_Id);

int Measure(struct dht11 *, struct measurement *);

void DeinitDht11(struct dht11 *);

void DeinitGroveDht11(struct grove_dht11 *);