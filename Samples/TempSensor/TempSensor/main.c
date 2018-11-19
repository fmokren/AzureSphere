#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

// applibs_versions.h defines the API struct versions to use for applibs APIs.
#include "applibs_versions.h"
#include "epoll_timerfd_utilities.h"

#include <applibs/gpio.h>
#include <applibs/log.h>

#include "mt3620_rdb.h"

// Uncomment to enable a debug GPIO output pin that toggles on every observed transition
//#define DEBUG_GPIO MT3620_RDB_HEADER1_PIN6_GPIO

// This sample C application for the MT3620 Reference Development Board (Azure Sphere)
// blinks an LED.
// The blink rate can be changed through a button press.
//
// It uses the API for the following Azure Sphere application libraries:
// - gpio (digital input for button)
// - log (messages shown in Visual Studio's Device Output window during debugging)

// File descriptors - initialized to invalid value
static int gpioButtonFd = -1;
static int gpioButtonTimerFd = -1;
static int gpioLedFd = -1;
static int gpioLedTimerFd = -1;
static int epollFd = -1;

static int gpioTemp0 = -1;

#ifdef DEBUG_GPIO
static int gpioDebug = -1;
static GPIO_Value_Type gpioDebugValue = GPIO_Value_High;
#endif

// Button state variables
static GPIO_Value_Type buttonState = GPIO_Value_High;
static GPIO_Value_Type ledState = GPIO_Value_High;

// Blink interval variables
static const int numBlinkIntervals = 3;
static const struct timespec blinkIntervals[] = {{0, 125000000}, {0, 250000000}, {0, 500000000}};
static int blinkIntervalIndex = 0;

// Termination state
static volatile sig_atomic_t terminationRequired = false;

/// <summary>
///     Signal handler for termination requests. This handler must be async-signal-safe.
/// </summary>
static void TerminationHandler(int signalNumber)
{
    // Don't use Log_Debug here, as it is not guaranteed to be async signal safe
    terminationRequired = true;
}

/// <summary>
///     Handle LED timer event: blink LED.
/// </summary>
static void LedTimerEventHandler()
{
    if (ConsumeTimerFdEvent(gpioLedTimerFd) != 0) {
        terminationRequired = true;
        return;
    }

    // The blink interval has elapsed, so toggle the LED state
    // The LED is active-low so GPIO_Value_Low is on and GPIO_Value_High is off
    ledState = (ledState == GPIO_Value_Low ? GPIO_Value_High : GPIO_Value_Low);
    int result = GPIO_SetValue(gpioLedFd, ledState);
    if (result != 0) {
        Log_Debug("ERROR: Could not set LED output value: %s (%d).\n", strerror(errno), errno);
        terminationRequired = true;
    }

    

}

/// <summary>
///     Handle button timer event: if the button is pressed, change the LED blink rate.
/// </summary>
static void ButtonTimerEventHandler()
{
    if (ConsumeTimerFdEvent(gpioButtonTimerFd) != 0) {
        terminationRequired = true;
        return;
    }

    // Check for a button press
    GPIO_Value_Type newButtonState;
    int result = GPIO_GetValue(gpioButtonFd, &newButtonState);
    if (result != 0) {
        Log_Debug("ERROR: Could not read button GPIO: %s (%d).\n", strerror(errno), errno);
        terminationRequired = true;
        return;
    }

    // If the button has just been pressed, change the LED blink interval
    // The button has GPIO_Value_Low when pressed and GPIO_Value_High when released
    if (newButtonState != buttonState) {
        if (newButtonState == GPIO_Value_Low) {
            blinkIntervalIndex = (blinkIntervalIndex + 1) % numBlinkIntervals;
            /*if (SetTimerFdInterval(gpioLedTimerFd, &blinkIntervals[blinkIntervalIndex]) != 0) {
                terminationRequired = true;
            }*/

            struct timespec deadline;
            memset(&deadline, 0, sizeof(deadline));

            clock_t bitThreshold = .00004 * CLOCKS_PER_SEC;

            // Add the time you want to sleep
            deadline.tv_nsec = 18.1 * 1000 * 1000;

            int hiCount = 0;

            // Set GPIO temp to 0 for >18ms
            result = GPIO_SetValue(gpioTemp0, GPIO_Value_Low);
            if (result != 0)
            {
                Log_Debug("ERROR: Could not set TEMP 0 output value: %s (%d).\n", strerror(errno), errno);
                terminationRequired = true;
            }

            clock_nanosleep(CLOCK_REALTIME, 0, &deadline, NULL);

            result = GPIO_SetValue(gpioTemp0, GPIO_Value_High);
            if (result != 0)
            {
                Log_Debug("ERROR: Could not set TEMP 0 output value: %s (%d).\n", strerror(errno), errno);
                terminationRequired = true;
            }

            CloseFdAndPrintError(gpioTemp0, "Temp 0");
            
            gpioTemp0 = GPIO_OpenAsInput(MT3620_RDB_HEADER1_PIN4_GPIO);
            if (gpioTemp0 < 0)
            {
                Log_Debug("ERROR: Could not open GPIO 0 as input: %s (%d).\n", strerror(errno), errno);
                terminationRequired = true;
            }

            int bitCount = 0;
            GPIO_Value_Type sample;
            GPIO_Value_Type lastSample;
            uint64_t data = 0;
            clock_t startHigh;

            lastSample = sample;

            hiCount = 0;
            bool success = true;
            while (bitCount < 41)
            {
                result = GPIO_GetValue(gpioTemp0, &sample);
                
                if (result != 0)
                {
                    Log_Debug("ERROR: Could not read GPIO 0 as input %s (%d).\n", strerror(errno), errno);
                    success = false;
                    break;
                }

                if (sample == GPIO_Value_Low)
                {
                    // last sample was high so a bit has been "received".
                    if (lastSample != sample)
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
                    if (lastSample != sample)
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
                        success = false;
                        break;
                    }
                }

                lastSample = sample;
            }

            if (success)
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

                uint8_t mySum = temp + tempLow + humidity + humidLow;

                if (checksum == mySum)
                {
                    Log_Debug("Temp: %d\nHumidity: %d\n", temp, humidity);
                }
                else
                {
                    Log_Debug("Check sum doesn't match 0x%02x vs 0x%02x\n", checksum, mySum);
                }
            }

            CloseFdAndPrintError(gpioTemp0, "Temp 0");

            gpioTemp0 = GPIO_OpenAsOutput(MT3620_RDB_HEADER1_PIN4_GPIO, GPIO_OutputMode_PushPull, GPIO_Value_High);
            if (gpioTemp0 < 0)
            {
                Log_Debug("ERROR: Could not open GPIO 0 as output: %s (%d).\n", strerror(errno), errno);
                terminationRequired = true;
            }
        }
        buttonState = newButtonState;
    }
}

/// <summary>
///     Set up SIGTERM termination handler, initialize peripherals, and set up event handlers.
/// </summary>
/// <returns>0 on success, or -1 on failure</returns>
static int InitPeripheralsAndHandlers(void)
{
    struct sigaction action;
    memset(&action, 0, sizeof(struct sigaction));
    action.sa_handler = TerminationHandler;
    sigaction(SIGTERM, &action, NULL);

    epollFd = CreateEpollFd();
    if (epollFd < 0) {
        return -1;
    }

    // Open button GPIO as input, and set up a timer to poll it
    Log_Debug("Opening MT3620_RDB_BUTTON_A as input\n");
    gpioButtonFd = GPIO_OpenAsInput(MT3620_RDB_BUTTON_A);
    if (gpioButtonFd < 0) {
        Log_Debug("ERROR: Could not open button GPIO: %s (%d).\n", strerror(errno), errno);
        return -1;
    }
    struct timespec buttonPressCheckPeriod = {0, 1000000};
    gpioButtonTimerFd = CreateTimerFdAndAddToEpoll(epollFd, &buttonPressCheckPeriod,
                                                   &ButtonTimerEventHandler, EPOLLIN);
    if (gpioButtonTimerFd < 0) {
        return -1;
    }

    // Open LED GPIO, set as output with value GPIO_Value_High (off), and set up a timer to poll it
    Log_Debug("Opening MT3620_RDB_LED1_RED\n");
    gpioLedFd = GPIO_OpenAsOutput(MT3620_RDB_LED1_RED, GPIO_OutputMode_PushPull, GPIO_Value_High);
    if (gpioLedFd < 0) {
        Log_Debug("ERROR: Could not open LED GPIO: %s (%d).\n", strerror(errno), errno);
        return -1;
    }
    /*
    gpioLedTimerFd = CreateTimerFdAndAddToEpoll(epollFd, &blinkIntervals[blinkIntervalIndex],
                                                &LedTimerEventHandler, EPOLLIN);
    if (gpioLedTimerFd < 0) {
        return -1;
    }
    */
    // Open GPIO 0 for temp sensor
    Log_Debug("Opening MT3620_RDB_HEADER1_PIN4_GPIO as an output\n");
    gpioTemp0 = GPIO_OpenAsOutput(MT3620_RDB_HEADER1_PIN4_GPIO, GPIO_OutputMode_PushPull, GPIO_Value_High);
    if (gpioTemp0 < 0)
    {
        Log_Debug("ERROR: Could not open GPIO 0: %s (%d).\n", strerror(errno), errno);
        return -1;
    }

#ifdef DEBUG_GPIO
    // Open GPIO 1 for temp sensor
    Log_Debug("Opening DEBUG_GPIO as an output\n");
    gpioDebug = GPIO_OpenAsOutput(DEBUG_GPIO, GPIO_OutputMode_PushPull, GPIO_Value_High);
    if (gpioDebug < 0)
    {
        Log_Debug("ERROR: Could not open GPIO 0: %s (%d).\n", strerror(errno), errno);
        return -1;
    }

    GPIO_SetValue(gpioDebug, gpioDebugValue);
#endif
    return 0;
}

/// <summary>
///     Close peripherals and handlers.
/// </summary>
static void ClosePeripheralsAndHandlers(void)
{
    // Leave the LED off
    if (gpioLedFd >= 0) {
        GPIO_SetValue(gpioLedFd, GPIO_Value_High);
    }

    Log_Debug("Closing file descriptors\n");
    CloseFdAndPrintError(gpioLedTimerFd, "LedTimer");
    CloseFdAndPrintError(gpioLedFd, "GpioLed");
    CloseFdAndPrintError(gpioButtonTimerFd, "ButtonTimer");
    CloseFdAndPrintError(gpioButtonFd, "GpioButton");
    CloseFdAndPrintError(epollFd, "Epoll");
}

/// <summary>
///     Main entry point for this application.
/// </summary>
int main(int argc, char *argv[])
{
    Log_Debug("Blink application starting\n");
    if (InitPeripheralsAndHandlers() != 0) {
        terminationRequired = true;
    }

    // Use epoll to wait for events and trigger handlers, until an error or SIGTERM happens
    while (!terminationRequired) {
        if (WaitForEventAndCallHandler(epollFd) != 0) {
            terminationRequired = true;
        }
    }

    ClosePeripheralsAndHandlers();
    Log_Debug("Application exiting\n");
    return 0;
}
