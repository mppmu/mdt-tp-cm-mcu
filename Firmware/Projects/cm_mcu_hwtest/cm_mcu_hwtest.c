// File: cm_mcu_hwtest.c
// Auth: M. Fras, Electronics Division, MPI for Physics, Munich
// Mod.: M. Fras, Electronics Division, MPI for Physics, Munich
// Date: 08 Apr 2020
// Rev.: 21 Apr 2020
//
// Hardware test firmware running on the ATLAS MDT Trigger Processor (TP)
// Command Module (CM) MCU.
//



#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "driverlib/i2c.h"
#include "driverlib/rom_map.h"
#include "driverlib/sysctl.h"
#include "driverlib/uart.h"
#include "utils/uartstdio.h"
#include "hw/i2c/i2c.h"
#include "hw/uart/uart.h"
#include "uart_ui.h"
#include "cm_mcu_hwtest.h"
#include "cm_mcu_hwtest_io.h"



// The error routine that is called if the driver library encounters an error.
#ifdef DEBUG
void __error__(char *pcFilename, uint32_t ui32Line)
{
    UARTprintf("%s: Unhandled error occured in file `%s', line %d.\n", UI_STR_FATAL, pcFilename, ui32Line);
}
#endif



// Function prototypes.
void Help(void);
void Info(void);
int DelayUs(char *pcCmd, char *pcParam, uint32_t ui32SysClock);
int I2CAccess(char *pcCmd, char *pcParam);
void I2CAccessHelp(void);
int I2CPortCheck(uint8_t ui8I2CPort, tI2C **psI2C);
int I2CDetect(char *pcCmd, char *pcParam);
int UartAccess(char *pcCmd, char *pcParam);
int UartPortCheck(uint8_t ui8UartPort, tUART **psUart);
int UartSetup(char *pcCmd, char *pcParam);
void UartSetupHelp(void);



// Initialize hardware, get and process commands.
int main(void)
{
    uint32_t ui32SysClock;
    char pcUartStr[UI_STR_BUF_SIZE];
    char *pcUartCmd;
    char *pcUartParam;

    // Setup the system clock.
    ui32SysClock = MAP_SysCtlClockFreqSet(SYSTEM_CLOCK_SETTINGS, SYSTEM_CLOCK_FREQ);

    // Initialize the I2C masters.
    for (int i = 0; i < I2C_MASTER_NUM; i++) {
        g_psI2C[i].ui32I2CClk = ui32SysClock;
        I2CMasterInit(&g_psI2C[i]);
    }

    // Initialize the UARTs.
    g_sUart1.ui32UartClk = ui32SysClock;
    g_sUart1.bLoopback = true;      // Enable loopback for testing.
    UartInit(&g_sUart1);
    g_sUart3.ui32UartClk = ui32SysClock;
    g_sUart3.bLoopback = true;      // Enable loopback for testing.
    UartInit(&g_sUart3);
    g_sUart5.ui32UartClk = ui32SysClock;
    g_sUart5.bLoopback = true;      // Enable loopback for testing.
    UartInit(&g_sUart5);

    // Initialize the UART for the user interface.
    // CAUTION: This must be done *after* the initialization of the UARTs.
    //          Otherwise, the UART UI settings would be overwritten.
    g_sUartUi3.ui32SrcClock = ui32SysClock;
    UartUiInit(&g_sUartUi3);

    // Send initial information to UART.
    UARTprintf("\n\n*******************************************************************************\n");
    UARTprintf("MDT-TP CM MCU `%s' firmware version %s, release date: %s\n", FW_NAME, FW_VERSION, FW_RELEASEDATE);
    UARTprintf("*******************************************************************************\n\n");
    UARTprintf("Type `help' to get an overview of available commands.\n");

    while(1)
    {
        UARTprintf("%s", UI_COMMAND_PROMPT);
        UARTgets(pcUartStr, UI_STR_BUF_SIZE);
        pcUartCmd = strtok(pcUartStr, UI_STR_DELIMITER);
        pcUartParam = strtok(NULL, UI_STR_DELIMITER);
        if (pcUartCmd == NULL) {
            continue;
        } else if (!strcasecmp(pcUartCmd, "help")) {
            Help();
        } else if (!strcasecmp(pcUartCmd, "info")) {
            Info();
        // Delay execution for a given number of microseconds.
        } else if (!strcasecmp(pcUartCmd, "delay")) {
            DelayUs(pcUartCmd, pcUartParam, ui32SysClock);
        // I2C based functions.
        } else if (!strcasecmp(pcUartCmd, "i2c")) {
            I2CAccess(pcUartCmd, pcUartParam);
        } else if (!strcasecmp(pcUartCmd, "i2c-det")) {
            I2CDetect(pcUartCmd, pcUartParam);
        // UART based functions.
        } else if (!strcasecmp(pcUartCmd, "uart")) {
            UartAccess(pcUartCmd, pcUartParam);
        } else if (!strcasecmp(pcUartCmd, "uart-s")) {
            UartSetup(pcUartCmd, pcUartParam);
        // Unknown command.
        } else {
            UARTprintf("ERROR: Unknown command `%s'.", pcUartCmd);
        }
        UARTprintf("\n");
    }
}



// Show help.
void Help(void)
{
    UARTprintf("Available commands:\n");
    UARTprintf("  help                                Show this help text.\n");
    UARTprintf("  delay   MICROSECONDS                Delay execution.\n");
    UARTprintf("  i2c     PORT SLV-ADR ACC NUM|DATA   I2C access (ACC bits: R/W, Sr, nP, Q).\n");
    UARTprintf("  i2c-det PORT [MODE]                 I2C detect devices (MODE: 0 = auto,\n");
    UARTprintf("                                          1 = quick command, 2 = read).\n");
    UARTprintf("  info                                Show information about this firmware.\n");
    UARTprintf("  uart    PORT R/W NUM|DATA           UART access (R/W: 0 = write, 1 = read).\n");
    UARTprintf("  uart-s  PORT BAUD [PARITY] [LOOP]   Set up the UART port.");
}



// Show information.
void Info(void)
{
    UARTprintf("MDT-TP CM MCU `%s' firmware version %s, release date: %s\n", FW_NAME, FW_VERSION, FW_RELEASEDATE);
    UARTprintf("It was compiled using gcc %s at %s on %s.", __VERSION__, __TIME__, __DATE__);
}



// Delay execution for a given number of microseconds.
int DelayUs(char *pcCmd, char *pcParam, uint32_t ui32SysClock)
{
    uint32_t ui32DelayUs;

    if (pcParam == NULL) {
        UARTprintf("%s: Parameter required after command `%s'.", UI_STR_ERROR, pcCmd);
        return -1;
    }
    ui32DelayUs = strtoul(pcParam, (char **) NULL, 0);
    // Limit the delay to max. 10 seconds.
    if (ui32DelayUs > 1e7) ui32DelayUs = 1e7;
    // CAUTION: Calling SysCtlDelay(0) will hang the system.
    if (ui32DelayUs > 0)
        // Note: The SysCtlDelay executes a simple 3 instruction cycle loop.
        SysCtlDelay((ui32SysClock / 3e6) * ui32DelayUs);

    UARTprintf("%s.", UI_STR_OK);

    return 0;
}



// I2C access.
int I2CAccess(char *pcCmd, char *pcParam)
{
    int i;
    tI2C *psI2C;
    uint8_t ui8I2CPort = 0;
    uint8_t ui8I2CSlaveAddr = 0;
    uint8_t ui8I2CAccMode = 0;
    uint8_t ui8I2CRw = 0;           // 0 = write; 1 = read
    bool bI2CRepeatedStart = false; // Repeated start.
    bool bI2CStop = true;           // Generate stop condition.
    bool bI2CQuickCmd = false;      // Quick command.
    uint8_t ui8I2CDataNum = 0;
    uint8_t pui8I2CData[32];
    uint32_t ui32I2CMasterStatus;
    // Parse parameters.
    for (i = 0; i < sizeof(pui8I2CData) / sizeof(pui8I2CData[0]); i++) {
        if (i != 0) pcParam = strtok(NULL, UI_STR_DELIMITER);
        if (i == 0) {
            if (pcParam == NULL) {
                UARTprintf("%s: I2C port number required after command `%s'.\n", UI_STR_ERROR, pcCmd);
                I2CAccessHelp();
                return -1;
            } else {
                ui8I2CPort = (uint8_t) strtoul(pcParam, (char **) NULL, 0) & 0xff;
            }
        } else if (i == 1) {
            if (pcParam == NULL) {
                UARTprintf("%s: I2C slave address required after command `%s'.\n", UI_STR_ERROR, pcCmd);
                I2CAccessHelp();
                return -1;
            } else {
                ui8I2CSlaveAddr = (uint8_t) strtoul(pcParam, (char **) NULL, 0) & 0xff;
            }
        } else if (i == 2) {
            if (pcParam == NULL) {
                UARTprintf("%s: I2C access mode required after command `%s'.\n", UI_STR_ERROR, pcCmd);
                I2CAccessHelp();
                return -1;
            } else {
                ui8I2CAccMode = (uint8_t) strtoul(pcParam, (char **) NULL, 0) & 0x0f;
                ui8I2CRw = ui8I2CAccMode & 0x1;
                bI2CRepeatedStart = (ui8I2CAccMode & 0x2) ? true : false;
                bI2CStop = (ui8I2CAccMode & 0x4) ? false : true;
                bI2CQuickCmd = (ui8I2CAccMode & 0x8) ? true : false;
            }
        } else {
            if (i == 3 && ui8I2CRw == 0 && !bI2CQuickCmd && pcParam == NULL) {
                UARTprintf("%s: At least one data byte required after I2C write command `%s'.\n", UI_STR_ERROR, pcCmd);
                I2CAccessHelp();
                return -1;
            }
            if (pcParam == NULL) break;
            else pui8I2CData[i-3] = (uint8_t) strtoul(pcParam, (char **) NULL, 0) & 0xff;
        }
    }
    if (i < 3) return -1;
    // Check if the I2C port number is valid. If so, set the psI2C pointer to the selected I2C port struct.
    if (I2CPortCheck(ui8I2CPort, &psI2C)) return -1;
    // I2C quick command.
    if (bI2CQuickCmd) {
        ui32I2CMasterStatus = I2CMasterQuickCmdAdv(psI2C, ui8I2CSlaveAddr, ui8I2CRw, bI2CRepeatedStart);
    // Standard I2C access.
    } else {
        // I2C write.
        if (ui8I2CRw == 0) {
            ui32I2CMasterStatus = I2CMasterWriteAdv(psI2C, ui8I2CSlaveAddr, pui8I2CData, i - 3, bI2CRepeatedStart, bI2CStop);
        // I2C read.
        } else {
            if (i == 3) ui8I2CDataNum = 1;
            else ui8I2CDataNum = pui8I2CData[0];
            if (ui8I2CDataNum > sizeof(pui8I2CData) / sizeof(pui8I2CData[0])) {
                ui8I2CDataNum = sizeof(pui8I2CData) / sizeof(pui8I2CData[0]);
            }
            ui32I2CMasterStatus = I2CMasterReadAdv(psI2C, ui8I2CSlaveAddr, pui8I2CData, ui8I2CDataNum, bI2CRepeatedStart, bI2CStop);
        }
    }
    // Check the I2C status.
    if (ui32I2CMasterStatus) {
        UARTprintf("%s: Error flags from I2C the master %d: 0x%08x", UI_STR_ERROR, ui8I2CPort, ui32I2CMasterStatus);
        if (ui32I2CMasterStatus & I2C_MASTER_INT_TIMEOUT) UARTprintf("\n%s: I2C timeout.", UI_STR_ERROR);
        if (ui32I2CMasterStatus & I2C_MASTER_INT_NACK) UARTprintf("\n%s: NACK received.", UI_STR_ERROR);
        if (ui32I2CMasterStatus & I2C_MASTER_INT_ARB_LOST) UARTprintf("\n%s: I2C bus arbitration lost.", UI_STR_ERROR);
        if (ui32I2CMasterStatus & 0x1) UARTprintf("\n%s: Unknown error.", UI_STR_ERROR);
    } else {
        UARTprintf("%s.", UI_STR_OK);
        if (ui8I2CRw && !bI2CQuickCmd) {
            UARTprintf(" Data:");
            for (i = 0; i < ui8I2CDataNum; i++) UARTprintf(" 0x%02x", pui8I2CData[i]);
        }
    }

    return 0;
}



// Show help on I2C access command.
void I2CAccessHelp(void)
{
    UARTprintf("I2C access command:\n");
    UARTprintf("  i2c     PORT SLV-ADR ACC NUM|DATA   I2C access (ACC bits: R/W, Sr, nP, Q).\n");
    UARTprintf("I2C access mode (ACC) bits:\n");
    UARTprintf("  0: Read/write (R/W)                 0 = write, 1 = read\n");
    UARTprintf("  1: Repeated start (Sr)              0 = no Sr, 1 = Sr\n");
    UARTprintf("  2: No stop condition (nP)           0 = generate stop cond. P, 1 = omit P\n");
    UARTprintf("  3: Quick command (Q)                0 = no Q, 1 = Q");
}



// Check if the I2C port number is valid. If so, set the psI2C pointer to the selected I2C port struct.
int I2CPortCheck(uint8_t ui8I2CPort, tI2C **psI2C)
{
    if ((ui8I2CPort < 0) || (ui8I2CPort > I2C_MASTER_NUM - 1)) {
        *psI2C = NULL;
        UARTprintf("%s: Only I2C port numbers 0..%d are supported!", UI_STR_ERROR, I2C_MASTER_NUM - 1);
        return -1;
    } else {
        *psI2C = &g_psI2C[ui8I2CPort];
    }
    return 0;
}



// Detect I2C devices.
int I2CDetect(char *pcCmd, char *pcParam)
{
    int i;
    tI2C *psI2C;
    uint8_t ui8I2CPort = 0;
    uint8_t ui8I2CSlaveAddr = 0;
    uint8_t ui8I2CDetectMode = 0;   // 0 = auto; 1 = quick command; 2 = read
    uint8_t pui8I2CData[1];
    uint32_t ui32I2CMasterStatus;
    // Parse parameters.
    for (i = 0; i < 4; i++) {
        if (i != 0) pcParam = strtok(NULL, UI_STR_DELIMITER);
        if (i == 0) {
            if (pcParam == NULL) {
                UARTprintf("%s: I2C port number required after command `%s'.", UI_STR_ERROR, pcCmd);
                return -1;
            } else {
                ui8I2CPort = (uint8_t) strtoul(pcParam, (char **) NULL, 0) & 0xff;
            }
        } else if (i == 1 && pcParam != NULL) {
            ui8I2CDetectMode = (uint8_t) strtoul(pcParam, (char **) NULL, 0) & 0x0f;
        } else {
            break;
        }
    }
    if (i < 1) return -1;
    // Check if the I2C port number is valid. If so, set the psI2C pointer to the selected I2C port struct.
    if (I2CPortCheck(ui8I2CPort, &psI2C)) return -1;
    // Detect I2C devices based on the i2cdetect program of the i2c-tools.
    // Please see: https://github.com/mozilla-b2g/i2c-tools/blob/master/tools/i2cdetect.c
    UARTprintf("%s. I2C device(s) found at slave address:", UI_STR_OK);
    for (ui8I2CSlaveAddr = 1; ui8I2CSlaveAddr <= 0x7f; ui8I2CSlaveAddr++) {
        switch (ui8I2CDetectMode) {
            // Detection mode using I2C quick command.
            case 1:
                ui32I2CMasterStatus = I2CMasterQuickCmd(psI2C, ui8I2CSlaveAddr, false);   // false = write; true = read
            // Detection mode using I2C read command.
            case 2:
                ui32I2CMasterStatus = I2CMasterRead(psI2C, ui8I2CSlaveAddr, pui8I2CData, 1);
            // Automatic mode. Use I2C quick command or I2C read based on the slave address.
            default:
                if ((ui8I2CSlaveAddr >= 0x30 && ui8I2CSlaveAddr <= 0x37)
                    || (ui8I2CSlaveAddr >= 0x50 && ui8I2CSlaveAddr <= 0x5F)) {
                    ui32I2CMasterStatus = I2CMasterRead(psI2C, ui8I2CSlaveAddr, pui8I2CData, 1);
                } else {
                    ui32I2CMasterStatus = I2CMasterQuickCmd(psI2C, ui8I2CSlaveAddr, false);   // false = write; true = read
                }
        }
        if (!ui32I2CMasterStatus) UARTprintf(" 0x%02x", ui8I2CSlaveAddr);
    }

    return 0;
}



// UART access.
int UartAccess(char *pcCmd, char *pcParam)
{
    int i;
    uint8_t ui8UartPort = 0;
    uint8_t ui8UartRw = 0;  // 0 = write; 1 = read
    uint8_t ui8UartDataNum = 0;
    uint8_t pui8UartData[32];
    tUART *psUart;
    int32_t i32UartStatus;
    // Parse parameters.
    for (i = 0; i < sizeof(pui8UartData) / sizeof(pui8UartData[0]); i++) {
        if (i != 0) pcParam = strtok(NULL, UI_STR_DELIMITER);
        if (i == 0) {
            if (pcParam == NULL) {
                UARTprintf("%s: UART port number required after command `%s'.", UI_STR_ERROR, pcCmd);
                return -1;
            } else {
                ui8UartPort = (uint8_t) strtoul(pcParam, (char **) NULL, 0) & 0xff;
            }
        } else if (i == 1) {
            if (pcParam == NULL) {
                UARTprintf("%s: UART read/write required after command `%s'.", UI_STR_ERROR, pcCmd);
                return -1;
            } else {
                ui8UartRw = (uint8_t) strtoul(pcParam, (char **) NULL, 0) & 0x01;
            }
        } else {
            if (i == 2 && ui8UartRw == 0 && pcParam == NULL) {
                UARTprintf("%s: At least one data byte required after UART write command `%s'.", UI_STR_ERROR, pcCmd);
                return -1;
            }
            if (pcParam == NULL) break;
            else pui8UartData[i-2] = (uint8_t) strtoul(pcParam, (char **) NULL, 0) & 0xff;
        }
    }
    if (i < 2) return -1;
    // Check if the UART port number is valid.
    switch (ui8UartPort) {
        case 1: psUart = &g_sUart1; break;
        case 5: psUart = &g_sUart5; break;
        default:
            UARTprintf("%s: Only UART port numbers 1 and 5 are supported!", UI_STR_ERROR);
            return -1;
    }
    // UART write.
    if (ui8UartRw == 0) {
        i32UartStatus = UartWrite(psUart, pui8UartData, i - 2);
        // Check the UART status.
        if (i32UartStatus) {
            UARTprintf("%s: Error status from the UART %d: %d", UI_STR_ERROR, ui8UartPort, i32UartStatus);
        } else {
            UARTprintf("%s.", UI_STR_OK);
        }
    // UART read.
    } else {
        if (i == 2) ui8UartDataNum = 1;
        // Read all available data.
        if (i == 2) {
            for (int iCnt = 0; ; iCnt++) {
                i32UartStatus = UartRead(psUart, pui8UartData, 1);
                if (i32UartStatus != 1) {
                    if (iCnt == 0) UARTprintf("%s: No data available.", UI_STR_WARNING);
                    break;
                } else {
                    if (iCnt == 0) UARTprintf("%s. Data:", UI_STR_OK);
                    UARTprintf(" 0x%02x", pui8UartData[0]);
                }
            }
        // Read given number of data.
        } else {
            ui8UartDataNum = pui8UartData[0];
            if (ui8UartDataNum > sizeof(pui8UartData) / sizeof(pui8UartData[0])) {
                ui8UartDataNum = sizeof(pui8UartData) / sizeof(pui8UartData[0]);
            }
            i32UartStatus = UartRead(psUart, pui8UartData, ui8UartDataNum);
            // Check the UART status.
            if (i32UartStatus != ui8UartDataNum) {
                UARTprintf("%s: Could only read %d data bytes from the UART %d instead of %d.", UI_STR_WARNING, i32UartStatus, ui8UartPort, ui8UartDataNum);
            } else {
                UARTprintf("%s.", UI_STR_OK);
            }
            if (i32UartStatus > 0) {
                UARTprintf(" Data:");
                for (i = 0; i < i32UartStatus; i++) UARTprintf(" 0x%02x", pui8UartData[i]);
            }
        }
    }

    return 0;
}



// Check if the UART port number is valid. If so, set the psUart pointer to the
// selected UART port struct.
int UartPortCheck(uint8_t ui8UartPort, tUART **psUart)
{
    switch (ui8UartPort) {
        case 1: *psUart = &g_sUart1; break;
        case 5: *psUart = &g_sUart5; break;
        default:
            *psUart = NULL;
            UARTprintf("%s: Only UART port numbers 1 and 5 are supported!", UI_STR_ERROR);
            return -1;
    }
    return 0;
}



// Set up the UART port.
int UartSetup(char *pcCmd, char *pcParam)
{
    int i;
    uint8_t ui8UartPort = 0;
    uint32_t ui32UartBaud;
    uint32_t ui32UartParity;
    bool bUartLoopback;
    tUART *psUart;
    // Parse parameters.
    for (i = 0; i <= 3; i++) {
        if (i != 0) pcParam = strtok(NULL, UI_STR_DELIMITER);
        if (i == 0) {
            if (pcParam == NULL) {
                UARTprintf("%s: UART port number required after command `%s'.\n", UI_STR_ERROR, pcCmd);
                UartSetupHelp();
                return -1;
            } else {
                ui8UartPort = (uint8_t) strtoul(pcParam, (char **) NULL, 0) & 0xff;
            }
        } else if (i == 1) {
            if (pcParam == NULL) {
                UARTprintf("%s: UART baud rate required after command `%s'.\n", UI_STR_ERROR, pcCmd);
                UartSetupHelp();
                return -1;
            } else {
                ui32UartBaud = (uint32_t) strtoul(pcParam, (char **) NULL, 0);
                if ((ui32UartBaud < UART_BAUD_MIN) || (ui32UartBaud > UART_BAUD_MAX)) {
                    UARTprintf("%s: UART baud rate %d outside of valid range %d..%d.", UI_STR_ERROR, ui32UartBaud, UART_BAUD_MIN, UART_BAUD_MAX);
                    return -1;
                }
            }
        } else if (i == 2) {
            if (pcParam == NULL) {
                ui32UartParity = UART_CONFIG_PAR_NONE;
            } else {
                ui32UartParity = strtoul(pcParam, (char **) NULL, 0) & 0x07;
                switch (ui32UartParity) {
                    case 0: ui32UartParity = UART_CONFIG_PAR_NONE; break;
                    case 1: ui32UartParity = UART_CONFIG_PAR_EVEN; break;
                    case 2: ui32UartParity = UART_CONFIG_PAR_ODD; break;
                    case 3: ui32UartParity = UART_CONFIG_PAR_ONE; break;
                    case 4: ui32UartParity = UART_CONFIG_PAR_ZERO; break;
                    default:
                        UARTprintf("%s: Invalid UART parity setting %d.", UI_STR_ERROR, ui32UartParity);
                        return -1;
                }
            }
        } else if (i == 3) {
            if (pcParam == NULL) {
                bUartLoopback = false;
            } else {
                bUartLoopback = (bool) strtoul(pcParam, (char **) NULL, 0) & 0x01;
            }
        }
    }
    if (i < 1) return -1;
    // Check if the UART port number is valid. If so, set the psUart pointer to the selected UART port struct.
    if (UartPortCheck(ui8UartPort, &psUart)) return -1;
    // Set up the UART.
    psUart->ui32Baud = ui32UartBaud;
    psUart->bLoopback = bUartLoopback;
    UartInit(psUart);
    UARTParityModeSet(psUart->ui32BaseUart, ui32UartParity);

    UARTprintf("%s.", UI_STR_OK);

    return 0;
}



// Show help on the UART setup command.
void UartSetupHelp(void)
{
    UARTprintf("UART setup command:\n");
    UARTprintf("  uart-s  PORT BAUD [PARITY] [LOOP]   Set up the UART port.");
    UARTprintf("UART baud rate: %d..%d\n", UART_BAUD_MIN, UART_BAUD_MAX);
    UARTprintf("UART partiy options:\n");
    UARTprintf("  0: None.\n");
    UARTprintf("  1: Even.\n");
    UARTprintf("  2: Odd.\n");
    UARTprintf("  3: One.\n");
    UARTprintf("  4: Zero.");
    UARTprintf("UART loopback options:\n");
    UARTprintf("  0: No loopback.\n");
    UARTprintf("  1: Enable internal loopback mode.\n");
}
