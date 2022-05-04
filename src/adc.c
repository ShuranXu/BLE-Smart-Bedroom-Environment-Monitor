/*
 * @file      adc.c
 * @author    Kevin Tom (keto9919@colorado.edu)
 * @brief     All necessary code for ADC0 on Blue gecko
 * @version   1.0
 * @date      Apr 12, 2022
 * @reference EMLIB API reference :
 *            https://siliconlabs.github.io/Gecko_SDK_Doc/efr32bg13/html/group__ADC.html
 */



#include "src/adc.h"
#include "em_adc.h"
#include "em_cmu.h"
#include <math.h>

#define INCLUDE_LOG_DEBUG 1
#include "src/log.h"

// Init to max ADC clock for Series 1
#define adcFreq                         (16000000) //16 MHz


void initADC0 (void)
{
  // Enable clocks required
  CMU_ClockEnable(cmuClock_HFPER, true);
//  LOG_INFO("HFPER Clock Frequency:%u\n\r",CMU_ClockFreqGet(cmuClock_HFPER));

  CMU_ClockEnable(cmuClock_ADC0, true);
//  LOG_INFO("ADC0 Clock Frequency:%u\n\r",CMU_ClockFreqGet(cmuClock_ADC0));

  // Declare init structs
  ADC_Init_TypeDef ADC0_init = ADC_INIT_DEFAULT;

  // Modify init structs
  // Set to 0 to use currently defined HFPER/HFPERC clock setting.
  ADC0_init.prescale   = ADC_PrescaleCalc(adcFreq, 0);
  ADC0_init.timebase   = ADC_TimebaseCalc(0);
  ADC0_init.ovsRateSel = adcOvsRateSel32;

  ADC_Init(ADC0, &ADC0_init);

  ADC_InitSingle_TypeDef ADC0_Single = {
      .acqTime = adcAcqTime4,
      .diff = false,
      .fifoOverwrite = false,
      .leftAdjust = false,
      .negSel     = adcNegSelVSS,
      .posSel     = adcPosSelAPORT3XCH10,
      .prsEnable  = false,
      .reference  = adcRef5V,
      .rep = false,
      .resolution = adcRes12Bit,
      .singleDmaEm2Wu  = false
  };

  ADC_InitSingle(ADC0, &ADC0_Single);

  // Enable ADC interrupts
  NVIC_ClearPendingIRQ(ADC0_IRQn);
  NVIC_EnableIRQ(ADC0_IRQn);

  // Enable ADC Single Conversion Complete interrupt
  ADC_IntEnable(ADC0, ADC_IEN_SINGLE);

}

/**
 * @brief This function converts the voltage measured from the ADC to sound level
 * in dB.
 * @param volts_mv: voltage in millivolts
 * @return int : sound level in dB
 */
uint32_t ADCmVtodB(uint32_t volts_mv)
{
//  The microphone is -46dB (V/Pa)
//  The op-amp stage gain is stated as 100, which is +40dB
//  This gives a system sensitivity of: -46+40 = -6dB

  if(volts_mv == 0)
    return 0;

  int volts_db = 20*log10(volts_mv);
  //reducing the system sensitivity
  uint32_t spl_db = volts_db - (-6);
  return spl_db;
}
