// File: uart.h
// Auth: M. Fras, Electronics Division, MPI for Physics, Munich
// Mod.: M. Fras, Electronics Division, MPI for Physics, Munich
// Date: 18 Feb 2020
// Rev.: 11 Apr 2020
//
// Header file for the UART functions on the TI Tiva TM4C1290 MCU on the ATLAS
// MDT Trigger Processor (TP) Command Module (CM).
//



#ifndef __UART_H__
#define __UART_H__



// Types.
typedef struct {
    uint32_t ui32PeripheralUart;
    uint32_t ui32PeripheralGpio;
    uint32_t ui32PortGpioBase;
    uint8_t  ui8PinGpioRx;
    uint8_t  ui8PinGpioTx;
    uint32_t ui32PinConfigRx;
    uint32_t ui32PinConfigTx;
    uint32_t ui32BaseUart;
    uint32_t ui32UartClk;
    uint32_t ui32Baud;
    uint32_t ui32Config;
    bool     bLoopback;
} tUART;



// Function prototypes.
void UartInit(tUART *uart);
uint32_t UartWrite(tUART *psUart, uint8_t *ui8Data, uint8_t ui8Length);
uint32_t UartRead(tUART *psUart, uint8_t *ui8Data, uint8_t ui8Length);



#endif  // __UART_H__

