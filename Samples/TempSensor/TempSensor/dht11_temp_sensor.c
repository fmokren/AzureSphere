#include "dht11_temp_sensor.h"
#include "applibs/log.h"
#include <errno.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

int InitDht11(struct dht11 *desc, GPIO_Id dataId)
{
    Log_Debug("Opening %d as DHT11 data pin\n", dataId);
    int fd = GPIO_OpenAsOutput(dataId, GPIO_OutputMode_PushPull, GPIO_Value_High);

    if (fd < 0)
    {
        Log_Debug("ERROR: Could not open GPIO %d: %s (%d)", dataId, strerror(errno), errno);
        return fd;
    }

    desc->id = dataId;
    desc->gpioFd = fd;
    return 0;
}

void CloseFdAndPrintError2(int fd, const char *fdName)
{
    if (fd >= 0) {
        int result = close(fd);
        if (result != 0) {
            Log_Debug("WARNING: Could not close fd %s: %s (%d).\n", fdName, strerror(errno), errno);
        }
    }
}

int InternalMeasure(struct dht11 *desc, struct measurement *sample)
{
    struct timespec deadline;
    memset(&deadline, 0, sizeof(deadline));

    clock_t bitThreshold = .00004 * CLOCKS_PER_SEC;

    // Add the time you want to sleep
    deadline.tv_nsec = 18.1 * 1000 * 1000;

    int hiCount = 0;

    // Set GPIO temp to 0 for >18ms
    int result = GPIO_SetValue(desc->gpioFd, GPIO_Value_Low);
    if (result != 0)
    {
        Log_Debug("ERROR: Could not set TEMP 0 output value: %s (%d).\n", strerror(errno), errno);
        return 0;
    }

    clock_nanosleep(CLOCK_REALTIME, 0, &deadline, NULL);

    result = GPIO_SetValue(desc->gpioFd, GPIO_Value_High);
    if (result != 0)
    {
        Log_Debug("ERROR: Could not set TEMP 0 output value: %s (%d).\n", strerror(errno), errno);
        return 0;
    }

    CloseFdAndPrintError2(desc->gpioFd, "DHT11 data pin");

    desc->gpioFd = GPIO_OpenAsInput(desc->id);
    if (desc->gpioFd < 0)
    {
        Log_Debug("ERROR: Could not open GPIO 0 as input: %s (%d).\n", strerror(errno), errno);
        return 0;
    }

    int bitCount = 0;
    GPIO_Value_Type pinSample;
    GPIO_Value_Type lastSample;
    uint64_t data = 0;
    clock_t startHigh;

    lastSample = pinSample;
    int success = 1;

    hiCount = 0;
    while (bitCount < 41)
    {
        result = GPIO_GetValue(desc->gpioFd, &pinSample);

        if (result != 0)
        {
            Log_Debug("ERROR: Could not read DHT11 data pin %s (%d).\n", strerror(errno), errno);
            success = 0; 
            break;
        }

        if (pinSample == GPIO_Value_Low)
        {
            // last sample was high so a bit has been "received".
            if (lastSample != pinSample)
            {
                clock_t endSample = clock();


                clock_t sampleDuration = endSample - startHigh;

                if (sampleDuration >= bitThreshold)
                {
                    // detected a 1 bit
                    data <<= 1;
                    data |= 1;
                }
                else
                {
                    // Detected a 0 bit
                    data <<= 1;
                }

                bitCount++;

#ifdef DEBUG_GPIO
                gpioDebugValue = gpioDebugValue == GPIO_Value_High ? GPIO_Value_Low : GPIO_Value_High;
                GPIO_SetValue(gpioDebug, gpioDebugValue);
#endif
            }

            // Reset high count
            hiCount = 0;
        }
        else
        {
            // Last sample was a low
            if (lastSample != pinSample)
            {
                startHigh = clock();
#ifdef DEBUG_GPIO
                gpioDebugValue = gpioDebugValue == GPIO_Value_High ? GPIO_Value_Low : GPIO_Value_High;
                GPIO_SetValue(gpioDebug, gpioDebugValue);
#endif
            }
            hiCount++;
            if (hiCount > 10000)
            {
                Log_Debug("Hi count exceeded threshold\n");
                success = 0;
                break;
            }
        }

        lastSample = pinSample;
    }

    int retVal = 0;
    if (success == 1)
    {
        Log_Debug("Data 0x%016llx\n", data);
        uint8_t checksum = data & 0xff;
        data >>= 8;

        uint8_t tempLow = data & 0xff;

        data >>= 8;

        uint8_t temp = data & 0xff;

        data >>= 8;

        uint8_t humidLow = data & 0xff;

        data >>= 8;

        uint8_t humidity = data & 0xff;

        int mySum = temp + tempLow + humidity + humidLow;

        if (checksum == mySum)
        {
            Log_Debug("Temp: %d\nHumidity: %d\n", temp, humidity);
            sample->temperature = temp;
            sample->humidity = humidity;
            retVal = 1;
        }
        else
        {
            Log_Debug("Check sum doesn't match 0x%02x vs 0x%02x\n", checksum, mySum);
        }
    }

    CloseFdAndPrintError2(desc->gpioFd, "DHT11 data pin");

    desc->gpioFd = GPIO_OpenAsOutput(desc->id, GPIO_OutputMode_PushPull, GPIO_Value_High);
    if (desc->gpioFd < 0)
    {
        Log_Debug("ERROR: Could not open GPIO 0 as output: %s (%d).\n", strerror(errno), errno);
        return 0;
    }

    return retVal;
}


int Measure(struct dht11 *desc, struct measurement *sample)
{
    // Always throw out the first measurement
    InternalMeasure(desc, sample);

    // Try five times to get a successful measurment.
    for (int i = 0; i < 5; i++)
    {
        struct timespec deadline;
        memset(&deadline, 0, sizeof(deadline));

        // Add the time you want to sleep
        deadline.tv_nsec = 500 * 1000 * 1000;

        clock_nanosleep(CLOCK_REALTIME, 0, &deadline, NULL);
        
        if (InternalMeasure(desc, sample) > 0)
        {
            return 1;
        }
    }

    return 0;
}

void DeinitDht11(struct dht11 *desc)
{
    CloseFdAndPrintError2(desc->gpioFd, "DHT11 data pin");
    memset(desc, 0, sizeof(*desc));
}
