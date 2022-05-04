/*
 * @file      adc.h
 * @author    Kevin Tom (keto9919@colorado.edu)
 * @brief     All necessary interface for ADC0 on Blue Gecko
 * @version   1.0
 * @date      Apr 12, 2022
 * @reference EMLIB API reference :
 *            https://siliconlabs.github.io/Gecko_SDK_Doc/efr32bg13/html/group__ADC.html
 */


#ifndef SRC_ADC_H_
#define SRC_ADC_H_

#include <stdint.h>

/* This function configures and initializes ADC0 */
void initADC0();

/**
 * @brief This function converts the voltage measured from the ADC to sound level
 * in dB.
 * @param volts_mv: voltage in millivolts
 * @return int : sound level in dB
 */
uint32_t ADCmVtodB(uint32_t volts_mv);


#endif /* SRC_ADC_H_ */
