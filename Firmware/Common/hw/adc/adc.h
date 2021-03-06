// File: adc.h
// Auth: M. Fras, Electronics Division, MPI for Physics, Munich
// Mod.: M. Fras, Electronics Division, MPI for Physics, Munich
// Date: 13 Feb 2020
// Rev.: 06 Apr 2020
//
// Header file for the ADC functions on the ATLAS MDT Trigger Processor (TP)
// Command Module (CM) MCU.
//



#ifndef __ADC_H__
#define __ADC_H__



// Types.
typedef struct {
    uint32_t ui32PeripheralAdc;
    uint32_t ui32PeripheralGpio;
    uint32_t ui32PortGpioBase;
    uint8_t  ui8PinGpio;
    uint32_t ui32BaseAdc;
    uint32_t ui32SequenceNum;
    uint32_t ui32Step;
    uint32_t ui32Config;
} tADC;


// Function prototypes.
void AdcReset(tADC *psAdc);
void AdcInit(tADC *psAdc);
uint32_t AdcConvert(tADC *psAdc);



#endif  // __ADC_H__

