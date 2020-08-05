// File: sm_cm.c
// Auth: M. Fras, Electronics Division, MPI for Physics, Munich
// Mod.: M. Fras, Electronics Division, MPI for Physics, Munich
// Date: 04 Aug 2020
// Rev.: 04 Aug 2020
//
// Functions for interfacing the Service Module and the Command Module in the
// hardware test firmware running on the ATLASfirmware running on the ATLAS MDT
// Trigger Processor (TP) Command Module (CM) MCU.
//



#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "driverlib/gpio.h"
#include "utils/uartstdio.h"
#include "hw/gpio/gpio.h"
#include "hw/gpio/gpio_pins.h"
#include "power_control.h"
#include "sm_cm.h"
#include "cm_mcu_hwtest.h"



// Initialize power up/down handshaking between the Service Module and the Command
// Module using the PWR_EN (drive by the SM) and the READY (driven by the CM)
// signals.
int SmCm_PowerHandshakingInit(void)
{
    // Register interrupt routine for the SM_PWR_ENA input.
    GpioInitIntr(&g_sGpio_SmPowerEna, SmCm_IntHandlerSmPowerEna);

    return 0;
}



// Interrupt handler for the SM_PWR_ENA input.
void SmCm_IntHandlerSmPowerEna(void)
{
    uint32_t ui32IntStatusSmPowerEna;

    ui32IntStatusSmPowerEna = GPIOIntStatus(g_sGpio_SmPowerEna.ui32Port, true);
    GPIOIntClear(g_sGpio_SmPowerEna.ui32Port, ui32IntStatusSmPowerEna);

    if ((ui32IntStatusSmPowerEna & g_sGpio_SmPowerEna.ui8Pins) == g_sGpio_SmPowerEna.ui8Pins) {
        // CM power up requested by SM.
        if (GpioGet_SmPowerEna()) {
            // Turn on the CM power domains.
//            PowerControl_All(true, 1);
            PowerControl_Clock(true, 1);
            PowerControl_KU15P(true, 1);
            PowerControl_ZU11EG(true, 1);
//            PowerControl_FireFly(true, 1);
            // Drive the CM_READY output high.
            GpioSet_CmReady(1);
            #ifdef SM_CM_POWER_HANDSHAKING_SHOW_MESSAGE
            UARTprintf("\nPower up requested from SM by driving SM_PWR_ENA high. Driving CM_READY high.\n");
            #endif
        // CM power down requested by SM.
        } else {
            // Turn off the CM power domains.
            PowerControl_All(true, 0);
//            PowerControl_Clock(true, 0);
//            PowerControl_KU15P(true, 0);
//            PowerControl_ZU11EG(true, 0);
//            PowerControl_FireFly(true, 0);
            // Drive the CM_READY output low.
            GpioSet_CmReady(0);
            #ifdef SM_CM_POWER_HANDSHAKING_SHOW_MESSAGE
            UARTprintf("\nPower down requested from SM by driving SM_PWR_ENA low. Driving CM_READY low.\n");
            #endif
        }
        // Show new command prompt.
        UARTprintf("%s", UI_COMMAND_PROMPT);
    }
}
